// The mixer's arithmetic (`core/audio.h`): constant-power pan, the linear
// resampler a voice runs through, and the conversion to the device's 16 bits.
// No device: these are the functions the mixer thread calls on each block.
#include "core/audio.h"
#include "test_support.h"

#include <cmath>
#include <limits>
#include <vector>

using tl::core::Sample;

namespace {

Sample mono(std::vector<int16_t> samples, int rate) {
    Sample s;
    s.channels = 1;
    s.rate = rate;
    s.frames = samples.size();
    s.samples = std::move(samples);
    return s;
}

}  // namespace

// Hard left is angle 0: cos 0 = 1, sin 0 = 0. Hard right is pi / 2, whose
// float is a hair past it, so cos is -4.37e-8 rather than 0 and sin is 1.
// The middle is pi / 4 on both sides, 0.70710677. Beyond +-1 is clamped.
TL_TEST(audio_pan_is_constant_power) {
    float left = 0.0f, right = 0.0f;
    tl::core::audio_pan_gains(-1.0f, left, right);
    TL_CHECK_EQ(left, 1.0f);
    TL_CHECK_EQ(right, 0.0f);
    tl::core::audio_pan_gains(1.0f, left, right);
    TL_CHECK(std::fabs(left) < 1e-7f);
    TL_CHECK_EQ(right, 1.0f);
    tl::core::audio_pan_gains(0.0f, left, right);
    TL_CHECK_EQ(left, right);
    TL_CHECK(std::fabs(left - 0.70710677f) < 1e-7f);
    float far_left = 0.0f, far_right = 0.0f;
    tl::core::audio_pan_gains(5.0f, far_left, far_right);
    tl::core::audio_pan_gains(1.0f, left, right);
    TL_CHECK_EQ(far_left, left);
    TL_CHECK_EQ(far_right, right);
}

// Same rate: one source frame per output frame. 16384 / 32768 = 0.5, scaled
// by left 1 and right 0.5; added to what `mix` held (0.25 on the right).
TL_TEST(audio_voice_at_its_own_rate_copies_and_adds) {
    const Sample s = mono({0, 16384, -16384, 32767}, 48000);
    std::vector<float> mix = {0.0f, 0.25f, 0.0f, 0.25f};
    double cursor = 0.0;
    TL_CHECK(tl::core::audio_mix_voice(s, cursor, 1.0f, 0.5f, mix.data(), 2, 48000));
    TL_CHECK_EQ(mix[0], 0.0f);
    TL_CHECK_EQ(mix[1], 0.25f);
    TL_CHECK_EQ(mix[2], 0.5f);
    TL_CHECK_EQ(mix[3], 0.25f + 0.25f);  // 0.5 * 0.5, plus the 0.25 already there
    TL_CHECK_EQ(cursor, 2.0);
}

// 44.1 kHz into 48 kHz: the cursor moves 44100 / 48000 = 0.91875 of a source
// frame per output frame, so output frame 1 is 0.91875 of the way from source
// 0 (0.0) to source 1 (0.5), and output frame 2 sits at 1.8375, between 0.5
// and -0.5.
TL_TEST(audio_voice_resamples_linearly) {
    const Sample s = mono({0, 16384, -16384, 0}, 44100);
    std::vector<float> mix(6, 0.0f);
    double cursor = 0.0;
    TL_CHECK(tl::core::audio_mix_voice(s, cursor, 1.0f, 1.0f, mix.data(), 3, 48000));
    const float t1 = static_cast<float>(44100.0 / 48000.0);
    TL_CHECK_EQ(mix[2], 0.5f * t1);
    const double at2 = 2.0 * (44100.0 / 48000.0);
    const float t2 = static_cast<float>(at2 - 1.0);
    TL_CHECK_EQ(mix[4], 0.5f + (-0.5f - 0.5f) * t2);
    TL_CHECK_EQ(mix[4], mix[5]);  // mono: both sides
    TL_CHECK_EQ(cursor, 3.0 * (44100.0 / 48000.0));
}

// Three source frames give two whole intervals: the third output frame would
// read past the end, so the voice stops there and leaves the rest alone.
TL_TEST(audio_voice_stops_at_its_last_interval) {
    const Sample s = mono({100, 200, 300}, 48000);
    std::vector<float> mix(8, 7.0f);
    double cursor = 0.0;
    TL_CHECK(!tl::core::audio_mix_voice(s, cursor, 1.0f, 1.0f, mix.data(), 4, 48000));
    TL_CHECK(mix[0] != 7.0f);
    TL_CHECK(mix[2] != 7.0f);
    TL_CHECK_EQ(mix[4], 7.0f);
    TL_CHECK_EQ(mix[6], 7.0f);
    TL_CHECK_EQ(cursor, 2.0);
}

// A stereo sample keeps its sides; a device rate of 0 means 48 kHz; an empty
// sample is over before it starts.
TL_TEST(audio_voice_stereo_default_rate_and_empty) {
    Sample stereo;
    stereo.channels = 2;
    stereo.rate = 48000;
    stereo.frames = 2;
    stereo.samples = {16384, -16384, 0, 0};
    std::vector<float> mix(2, 0.0f);
    double cursor = 0.0;
    TL_CHECK(tl::core::audio_mix_voice(stereo, cursor, 1.0f, 1.0f, mix.data(), 1, 0));
    TL_CHECK_EQ(mix[0], 0.5f);
    TL_CHECK_EQ(mix[1], -0.5f);
    TL_CHECK_EQ(cursor, 1.0);

    const Sample empty = mono({}, 44100);
    double start = 0.0;
    TL_CHECK(!tl::core::audio_mix_voice(empty, start, 1.0f, 1.0f, mix.data(), 1, 48000));
}

// Clamped to [-1, 1], times 32767, truncated: 0.5 -> 16383.5 -> 16383, and
// -0.5 -> -16383. A NaN comes out of std::min(1, NaN) as 1.
TL_TEST(audio_pcm16_clamps_and_truncates) {
    TL_CHECK_EQ(tl::core::audio_to_pcm16(0.5f), int16_t{16383});
    TL_CHECK_EQ(tl::core::audio_to_pcm16(-0.5f), int16_t{-16383});
    TL_CHECK_EQ(tl::core::audio_to_pcm16(1.5f), int16_t{32767});
    TL_CHECK_EQ(tl::core::audio_to_pcm16(-2.0f), int16_t{-32767});
    TL_CHECK_EQ(tl::core::audio_to_pcm16(std::numeric_limits<float>::quiet_NaN()), int16_t{32767});
}
