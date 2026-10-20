// RIFF/WAVE decoding: every sound effect the game's sound tables name is a `.wav`
// (music and ambient loops are Ogg).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tl::formats {

/// One decoded PCM sound: interleaved 16-bit signed, other widths converted.
/// `samples.size() == frames * channels`.
struct WavSound {
    bool ok = false;
    std::string error;

    int sample_rate = 0;
    int channels = 0;
    size_t frames = 0;
    std::vector<int16_t> samples;

    /// How long it plays, in seconds. Zero for an empty or unreadable file.
    float duration() const {
        return sample_rate > 0 ? static_cast<float>(frames) / static_cast<float>(sample_rate)
                               : 0.0f;
    }
};

/// Decode a RIFF/WAVE file held whole in memory: PCM 8/16/24/32-bit, IEEE float 32,
/// either inside `WAVE_FORMAT_EXTENSIBLE`, and IMA ADPCM (4-bit, 1-2 channels).
/// Anything else is an error, not an approximation.
WavSound parse_wav(const uint8_t* data, size_t size);

/// How long a `.wav` plays, without decoding it.
float wav_duration(const uint8_t* data, size_t size);

}  // namespace tl::formats
