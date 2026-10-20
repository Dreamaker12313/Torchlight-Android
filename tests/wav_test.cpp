// RIFF/WAVE, the format every sound effect ships as, on fixtures built here, including shapes
// the archive does not contain.
#include "formats/wav.h"

#include "test_support.h"

#include <cstring>
#include <string>
#include <vector>

namespace {

void put32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

void put16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

void put_tag(std::vector<uint8_t>& out, const char* tag) { out.insert(out.end(), tag, tag + 4); }

/// A whole RIFF/WAVE file, with `payload` as the `data` chunk's contents. `trailing`
/// appends a `LIST` chunk after `data`, as the shipped files carry.
std::vector<uint8_t> make_wav(uint16_t format, uint16_t channels, uint32_t rate, uint16_t bits,
                              const std::vector<uint8_t>& payload, bool trailing = false) {
    std::vector<uint8_t> body;
    put_tag(body, "fmt ");
    put32(body, 16);
    put16(body, format);
    put16(body, channels);
    put32(body, rate);
    put32(body, rate * channels * (bits / 8u));
    put16(body, static_cast<uint16_t>(channels * (bits / 8u)));
    put16(body, bits);

    put_tag(body, "data");
    put32(body, static_cast<uint32_t>(payload.size()));
    body.insert(body.end(), payload.begin(), payload.end());
    if (payload.size() & 1u) body.push_back(0);

    if (trailing) {
        put_tag(body, "LIST");
        const char info[] = "INFOISFT";
        put32(body, static_cast<uint32_t>(sizeof(info) - 1));
        body.insert(body.end(), info, info + sizeof(info) - 1);
    }

    std::vector<uint8_t> file;
    put_tag(file, "RIFF");
    put32(file, static_cast<uint32_t>(4 + body.size()));
    put_tag(file, "WAVE");
    file.insert(file.end(), body.begin(), body.end());
    return file;
}

std::vector<uint8_t> pcm16(const std::vector<int16_t>& samples) {
    std::vector<uint8_t> out;
    for (int16_t s : samples) put16(out, static_cast<uint16_t>(s));
    return out;
}

}  // namespace

/// The shape every file in the archive is: 16-bit mono PCM at 44.1 kHz, with a
/// `LIST` chunk behind the samples.
TL_TEST(wav_reads_the_shape_the_archive_ships) {
    const std::vector<int16_t> samples{0, 1000, -1000, 32767, -32768, 12};
    const std::vector<uint8_t> file = make_wav(1, 1, 44100, 16, pcm16(samples), /*trailing=*/true);

    const tl::formats::WavSound sound = tl::formats::parse_wav(file.data(), file.size());
    TL_CHECK(sound.ok);
    TL_CHECK_EQ(sound.channels, 1);
    TL_CHECK_EQ(sound.sample_rate, 44100);
    TL_CHECK_EQ(sound.frames, samples.size());
    TL_CHECK(sound.samples == samples);

    // And the length, which is what a line of dialogue is held for.
    const float expected = static_cast<float>(samples.size()) / 44100.0f;
    TL_CHECK(std::abs(sound.duration() - expected) < 1e-6f);
    TL_CHECK(std::abs(tl::formats::wav_duration(file.data(), file.size()) - expected) < 1e-6f);
}

/// An odd-length chunk is padded to an even boundary, the pad byte not counted in the size.
TL_TEST(wav_survives_an_odd_length_chunk) {
    std::vector<uint8_t> odd = pcm16({100, 200});
    odd.push_back(0x7F);  // one byte past a whole frame, so `data` is odd
    const std::vector<uint8_t> file = make_wav(1, 1, 22050, 16, odd, /*trailing=*/true);

    const tl::formats::WavSound sound = tl::formats::parse_wav(file.data(), file.size());
    TL_CHECK(sound.ok);
    TL_CHECK_EQ(sound.frames, size_t{2});
    TL_CHECK_EQ(sound.sample_rate, 22050);
}

/// Stereo interleaving is preserved rather than folded down (the mixer pans only mono).
TL_TEST(wav_keeps_stereo_interleaved) {
    const std::vector<int16_t> samples{1, -1, 2, -2, 3, -3};
    const std::vector<uint8_t> file = make_wav(1, 2, 48000, 16, pcm16(samples));

    const tl::formats::WavSound sound = tl::formats::parse_wav(file.data(), file.size());
    TL_CHECK(sound.ok);
    TL_CHECK_EQ(sound.channels, 2);
    TL_CHECK_EQ(sound.frames, size_t{3});
    TL_CHECK(sound.samples == samples);
}

/// 8-bit PCM is the one width that is unsigned, biased by 128.
TL_TEST(wav_converts_eight_bit_from_unsigned) {
    const std::vector<uint8_t> payload{128, 255, 0, 192};
    const std::vector<uint8_t> file = make_wav(1, 1, 11025, 8, payload);

    const tl::formats::WavSound sound = tl::formats::parse_wav(file.data(), file.size());
    TL_CHECK(sound.ok);
    TL_CHECK_EQ(sound.frames, size_t{4});
    TL_CHECK_EQ(sound.samples[0], int16_t{0});
    TL_CHECK(sound.samples[1] > 32000);
    TL_CHECK(sound.samples[2] < -32000);
    TL_CHECK(sound.samples[3] > 16000);
}

/// 32-bit float, normalised to +-1 and clamped rather than wrapped.
TL_TEST(wav_reads_float_samples) {
    std::vector<uint8_t> payload;
    for (float value : {0.0f, 0.5f, -0.5f, 2.0f}) {
        uint32_t bits = 0;
        std::memcpy(&bits, &value, 4);
        put32(payload, bits);
    }
    const std::vector<uint8_t> file = make_wav(3, 1, 44100, 32, payload);

    const tl::formats::WavSound sound = tl::formats::parse_wav(file.data(), file.size());
    TL_CHECK(sound.ok);
    TL_CHECK_EQ(sound.frames, size_t{4});
    TL_CHECK_EQ(sound.samples[0], int16_t{0});
    TL_CHECK(sound.samples[1] > 16000 && sound.samples[1] < 16600);
    TL_CHECK(sound.samples[2] < -16000 && sound.samples[2] > -16600);
    TL_CHECK_EQ(sound.samples[3], int16_t{32767});  // clamped, not wrapped
}

/// Anything this decoder cannot read says so instead of producing noise, which
/// is the whole reason `WavSound` carries an error string.
TL_TEST(wav_refuses_what_it_cannot_read) {
    const uint8_t junk[] = {'O', 'g', 'g', 'S', 0, 0, 0, 0, 0, 0, 0, 0};
    const tl::formats::WavSound not_riff = tl::formats::parse_wav(junk, sizeof(junk));
    TL_CHECK(!not_riff.ok);
    TL_CHECK(!not_riff.error.empty());

    // A compressed WAVE - Microsoft ADPCM, tag 0x02 - is a RIFF/WAVE this
    // cannot decode.
    const std::vector<uint8_t> adpcm = make_wav(0x02, 1, 44100, 4, {1, 2, 3, 4});
    const tl::formats::WavSound compressed = tl::formats::parse_wav(adpcm.data(), adpcm.size());
    TL_CHECK(!compressed.ok);
    TL_CHECK(compressed.error.find("unsupported") != std::string::npos);

    // And an empty span, which is what a missing archive entry hands back.
    const tl::formats::WavSound nothing = tl::formats::parse_wav(nullptr, 0);
    TL_CHECK(!nothing.ok);
    TL_CHECK_EQ(tl::formats::wav_duration(nullptr, 0), 0.0f);
}

// `WAVE_FORMAT_DVI_ADPCM` (IMA): each block opens with its own sample and step index.
TL_TEST(wav_decodes_ima_adpcm_blocks) {
    // One mono block of 8 bytes: sample 100, index 0, then the codes 4, 12, 1,
    // 0, 0, 0, 0, 0 - low nibble first.
    //   step 7:  code 4  -> +7         = 107, index 2
    //   step 9:  code 12 -> -(1 + 9)   = 97,  index 4
    //   step 11: code 1  -> +(1 + 2)   = 100, index 3
    //   step 10: code 0  -> +1         = 101, index 2
    //   step 9:  code 0  -> +1         = 102, index 1
    //   step 8:  code 0  -> +1         = 103, index 0
    //   step 7:  code 0  -> +0         = 103 (twice)
    const std::vector<uint8_t> block = {100, 0, 0, 0, 0xC4, 0x01, 0x00, 0x00};
    std::vector<uint8_t> payload = block;
    payload.insert(payload.end(), block.begin(), block.end());  // and a second block
    payload.insert(payload.end(), {50, 0, 0, 0});               // and a header alone
    std::vector<uint8_t> file = make_wav(0x11, 1, 22050, 4, payload, true);
    // `nBlockAlign` sits 32 bytes in (RIFF 12, "fmt " 8, then 12 of the format).
    file[32] = 8;
    file[33] = 0;

    const tl::formats::WavSound sound = tl::formats::parse_wav(file.data(), file.size());
    TL_CHECK(sound.ok);
    TL_CHECK_EQ(sound.channels, 1);
    TL_CHECK_EQ(sound.sample_rate, 22050);
    TL_CHECK_EQ(sound.frames, 19u);
    const int16_t expected[9] = {100, 107, 97, 100, 101, 102, 103, 103, 103};
    for (size_t i = 0; i < 9; ++i) {
        TL_CHECK_EQ(sound.samples[i], expected[i]);
        TL_CHECK_EQ(sound.samples[9 + i], expected[i]);  // a block starts afresh
    }
    TL_CHECK_EQ(sound.samples[18], 50);
    TL_CHECK(std::abs(tl::formats::wav_duration(file.data(), file.size()) - 19.0f / 22050.0f) <
             1e-6f);

    // Two channels: four bytes of codes a channel, the frames interleaved.
    const std::vector<uint8_t> stereo = {
        100,  0,    0,    0,    0x38, 0xFF, 0,    0,  // left 100, right -200
        0xC4, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    std::vector<uint8_t> two = make_wav(0x11, 2, 22050, 4, stereo);
    two[32] = 16;
    two[33] = 0;
    const tl::formats::WavSound wide = tl::formats::parse_wav(two.data(), two.size());
    TL_CHECK(wide.ok);
    TL_CHECK_EQ(wide.channels, 2);
    TL_CHECK_EQ(wide.frames, 9u);
    TL_CHECK_EQ(wide.samples[0], 100);
    TL_CHECK_EQ(wide.samples[1], -200);
    TL_CHECK_EQ(wide.samples[2], 107);   // left, its first code
    TL_CHECK_EQ(wide.samples[3], -200);  // right: code 0 at step 7 adds nothing
    TL_CHECK_EQ(wide.samples[4], 97);

    // A predictor driven past the top stays there.
    std::vector<uint8_t> loud =
        make_wav(0x11, 1, 22050, 4, {0xFF, 0x7F, 88, 0, 0x77, 0x77, 0x77, 0x77});
    loud[32] = 8;
    loud[33] = 0;
    const tl::formats::WavSound clipped = tl::formats::parse_wav(loud.data(), loud.size());
    TL_CHECK(clipped.ok);
    for (size_t i = 0; i < clipped.samples.size(); ++i) TL_CHECK_EQ(clipped.samples[i], 32767);
}
