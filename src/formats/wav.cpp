#include "formats/wav.h"

#include "formats/byte_reader.h"
#include "formats/float_convert.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace tl::formats {
namespace {

/// The two `wFormatTag` values the archive uses, plus the wrapper that carries
/// either of them in a `SubFormat` GUID whose first two bytes are the tag.
constexpr uint16_t kFormatPcm = 0x0001;
constexpr uint16_t kFormatFloat = 0x0003;
constexpr uint16_t kFormatImaAdpcm = 0x0011;
constexpr uint16_t kFormatExtensible = 0xFFFE;

int16_t clamp_to_i16(float value) {
    if (value > 1.0f) value = 1.0f;
    if (value < -1.0f) value = -1.0f;
    return x86_float_to_int16(value * 32767.0f);  // NaN passes both clamps
}

/// One chunk of a RIFF file: its four-character id and where its payload sits.
struct Chunk {
    char id[5] = {0, 0, 0, 0, 0};
    size_t at = 0;
    size_t size = 0;
};

/// Walk the chunk list, calling `visit` for each. Chunks are padded to an even
/// length and the pad byte is not counted in the size.
template <typename Visit>
bool walk_chunks(const uint8_t* data, size_t size, const Visit& visit) {
    size_t at = 12;  // "RIFF" + size + "WAVE"
    while (at + 8 <= size) {
        Chunk chunk;
        std::memcpy(chunk.id, data + at, 4);
        uint32_t payload = 0;
        std::memcpy(&payload, data + at + 4, 4);
        chunk.at = at + 8;
        chunk.size = std::min(static_cast<size_t>(payload), size - chunk.at);
        if (!visit(chunk)) return true;
        at = chunk.at + chunk.size + (chunk.size & 1u);
    }
    return true;
}

struct Format {
    bool found = false;
    uint16_t tag = 0;
    uint16_t channels = 0;
    uint32_t rate = 0;
    uint16_t block_align = 0;
    uint16_t bits = 0;
};

Format read_format(const uint8_t* data, size_t size) {
    Format format;
    walk_chunks(data, size, [&](const Chunk& chunk) {
        if (std::strcmp(chunk.id, "fmt ") != 0) return true;
        if (chunk.size < 16) return false;
        ByteReader reader(data + chunk.at, chunk.size);
        format.tag = reader.u16();
        format.channels = reader.u16();
        format.rate = reader.u32();
        reader.u32();                       // nAvgBytesPerSec, derivable and unused
        format.block_align = reader.u16();  // derivable for PCM; an ADPCM block's size
        format.bits = reader.u16();
        // `WAVE_FORMAT_EXTENSIBLE` puts the real tag in the first two bytes of
        // its 16-byte SubFormat GUID, 8 bytes into the extension.
        if (format.tag == kFormatExtensible && chunk.size >= 40) {
            reader.u16();  // cbSize
            reader.u16();  // wValidBitsPerSample
            reader.u32();  // dwChannelMask
            format.tag = reader.u16();
        }
        format.found = reader.ok();
        return false;
    });
    return format;
}

bool find_data(const uint8_t* data, size_t size, size_t& at, size_t& bytes) {
    bool found = false;
    walk_chunks(data, size, [&](const Chunk& chunk) {
        if (std::strcmp(chunk.id, "data") != 0) return true;
        at = chunk.at;
        bytes = chunk.size;
        found = true;
        return false;
    });
    return found;
}

bool is_riff_wave(const uint8_t* data, size_t size) {
    return data != nullptr && size >= 12 && std::memcmp(data, "RIFF", 4) == 0 &&
           std::memcmp(data + 8, "WAVE", 4) == 0;
}

/// How many bytes one frame of this format occupies, or 0 for one this decoder
/// will not read.
size_t frame_bytes(const Format& format) {
    if (format.channels == 0) return 0;
    if (format.tag == kFormatPcm) {
        if (format.bits != 8 && format.bits != 16 && format.bits != 24 && format.bits != 32) {
            return 0;
        }
    } else if (format.tag == kFormatFloat) {
        if (format.bits != 32) return 0;
    } else {
        return 0;
    }
    return static_cast<size_t>(format.channels) * (format.bits / 8u);
}

/// The IMA ADPCM tables (the IMA's own, as every decoder carries them).
constexpr int kImaIndexStep[16] = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};
constexpr int kImaStep[89] = {
    7,     8,     9,     10,    11,    12,    13,    14,    16,    17,    19,   21,    23,
    25,    28,    31,    34,    37,    41,    45,    50,    55,    60,    66,   73,    80,
    88,    97,    107,   118,   130,   143,   157,   173,   190,   209,   230,  253,   279,
    307,   337,   371,   408,   449,   494,   544,   598,   658,   724,   796,  876,   963,
    1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,  2272,  2499,  2749, 3024,  3327,
    3660,  4026,  4428,  4871,  5358,  5894,  6484,  7132,  7845,  8630,  9493, 10442, 11487,
    12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};

/// One channel's running state in an IMA ADPCM block.
struct ImaState {
    int predictor = 0;
    int index = 0;

    int16_t step(unsigned nibble) {
        const int size = kImaStep[index];
        int diff = size >> 3;
        if (nibble & 1u) diff += size >> 2;
        if (nibble & 2u) diff += size >> 1;
        if (nibble & 4u) diff += size;
        predictor += (nibble & 8u) ? -diff : diff;
        predictor = std::clamp(predictor, -32768, 32767);
        index = std::clamp(index + kImaIndexStep[nibble & 15u], 0, 88);
        return static_cast<int16_t>(predictor);
    }
};

/// How many frames one full block of `block_align` bytes decodes to: the
/// header's own sample, then eight to every four data bytes a channel has.
size_t ima_frames_per_block(const Format& format) {
    const size_t header = 4u * format.channels;
    if (format.channels == 0 || format.channels > 2 || format.bits != 4 ||
        format.block_align <= header) {
        return 0;
    }
    return 1 + ((format.block_align - header) / (4u * format.channels)) * 8u;
}

/// `WAVE_FORMAT_DVI_ADPCM`: blocks of `block_align` bytes, each opening with a
/// 16-bit sample and a step index per channel, then 4-bit codes - low nibble
/// first, and for two channels in runs of four bytes a channel. A short last
/// block decodes what whole groups it holds.
void decode_ima_adpcm(const Format& format, const uint8_t* from, size_t bytes, WavSound& sound) {
    const size_t channels = format.channels;
    const size_t header = 4 * channels;
    for (size_t at = 0; at + header <= bytes; at += format.block_align) {
        const size_t block = std::min<size_t>(format.block_align, bytes - at);
        const uint8_t* p = from + at;
        ImaState state[2];
        for (size_t c = 0; c < channels; ++c) {
            int16_t first = 0;
            std::memcpy(&first, p + c * 4, 2);
            state[c].predictor = first;
            state[c].index = std::clamp(static_cast<int>(p[c * 4 + 2]), 0, 88);
            sound.samples.push_back(first);
        }
        const size_t groups = (block - header) / (4 * channels);
        for (size_t group = 0; group < groups; ++group) {
            const uint8_t* codes = p + header + group * 4 * channels;
            int16_t decoded[2][8];
            for (size_t c = 0; c < channels; ++c) {
                for (size_t i = 0; i < 8; ++i) {
                    const uint8_t byte = codes[c * 4 + i / 2];
                    decoded[c][i] = state[c].step((i & 1u) ? (byte >> 4) : (byte & 15u));
                }
            }
            for (size_t i = 0; i < 8; ++i) {
                for (size_t c = 0; c < channels; ++c) sound.samples.push_back(decoded[c][i]);
            }
        }
    }
    sound.frames = sound.samples.size() / channels;
}

}  // namespace

WavSound parse_wav(const uint8_t* data, size_t size) {
    WavSound sound;
    if (!is_riff_wave(data, size)) {
        sound.error = "not a RIFF/WAVE file";
        return sound;
    }

    const Format format = read_format(data, size);
    if (!format.found) {
        sound.error = "no readable 'fmt ' chunk";
        return sound;
    }
    const bool adpcm = format.tag == kFormatImaAdpcm && ima_frames_per_block(format) != 0;
    const size_t stride = frame_bytes(format);
    if (stride == 0 && !adpcm) {
        sound.error = "unsupported WAVE format tag " + std::to_string(format.tag) + " at " +
                      std::to_string(format.bits) + " bits";
        return sound;
    }

    size_t at = 0;
    size_t bytes = 0;
    if (!find_data(data, size, at, bytes)) {
        sound.error = "no 'data' chunk";
        return sound;
    }

    if (adpcm) {
        sound.sample_rate = static_cast<int>(format.rate);
        sound.channels = static_cast<int>(format.channels);
        decode_ima_adpcm(format, data + at, bytes, sound);
        sound.ok = sound.frames > 0 && sound.sample_rate > 0;
        if (!sound.ok) sound.error = "the 'data' chunk holds no frames";
        return sound;
    }

    sound.sample_rate = static_cast<int>(format.rate);
    sound.channels = static_cast<int>(format.channels);
    sound.frames = bytes / stride;
    sound.samples.resize(sound.frames * static_cast<size_t>(format.channels));

    const uint8_t* from = data + at;
    const size_t values = sound.samples.size();
    switch (format.tag == kFormatFloat ? 0u : static_cast<unsigned>(format.bits)) {
        case 0u:  // 32-bit IEEE float, normalised to +-1
            for (size_t i = 0; i < values; ++i) {
                float value = 0.0f;
                std::memcpy(&value, from + i * 4, 4);
                sound.samples[i] = clamp_to_i16(value);
            }
            break;
        case 8u:  // unsigned, biased by 128 - the one PCM width that is not signed
            for (size_t i = 0; i < values; ++i) {
                sound.samples[i] = static_cast<int16_t>((static_cast<int>(from[i]) - 128) * 256);
            }
            break;
        case 16u:
            for (size_t i = 0; i < values; ++i) {
                int16_t value = 0;
                std::memcpy(&value, from + i * 2, 2);
                sound.samples[i] = value;
            }
            break;
        case 24u:
            for (size_t i = 0; i < values; ++i) {
                const uint8_t* p = from + i * 3;
                const int32_t value = (static_cast<int32_t>(p[2]) << 24) |
                                      (static_cast<int32_t>(p[1]) << 16) |
                                      (static_cast<int32_t>(p[0]) << 8);
                sound.samples[i] = static_cast<int16_t>(value >> 16);
            }
            break;
        case 32u:
            for (size_t i = 0; i < values; ++i) {
                int32_t value = 0;
                std::memcpy(&value, from + i * 4, 4);
                sound.samples[i] = static_cast<int16_t>(value >> 16);
            }
            break;
        default: sound.error = "unsupported PCM width"; return sound;
    }

    sound.ok = sound.frames > 0 && sound.sample_rate > 0 && sound.channels > 0;
    if (!sound.ok && sound.error.empty()) sound.error = "the 'data' chunk holds no frames";
    return sound;
}

float wav_duration(const uint8_t* data, size_t size) {
    if (!is_riff_wave(data, size)) return 0.0f;
    const Format format = read_format(data, size);
    if (format.rate == 0) return 0.0f;
    size_t at = 0;
    size_t bytes = 0;
    if (format.tag == kFormatImaAdpcm) {
        const size_t per_block = ima_frames_per_block(format);
        if (per_block == 0 || !find_data(data, size, at, bytes)) return 0.0f;
        const size_t header = 4u * format.channels;
        const size_t tail = bytes % format.block_align;
        size_t frames = (bytes / format.block_align) * per_block;
        if (tail >= header) frames += 1 + ((tail - header) / header) * 8;
        return static_cast<float>(frames) / static_cast<float>(format.rate);
    }
    const size_t stride = frame_bytes(format);
    if (stride == 0) return 0.0f;
    if (!find_data(data, size, at, bytes)) return 0.0f;
    const size_t frames = bytes / stride;  // whole frames: a partial one does not play
    return static_cast<float>(frames) / static_cast<float>(format.rate);
}

}  // namespace tl::formats
