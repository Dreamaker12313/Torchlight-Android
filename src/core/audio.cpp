#include "core/audio.h"

#include "core/log.h"
#include "formats/wav.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#if defined(TL_HAVE_VORBIS)
// stb_vorbis is a C file compiled into its own library (see CMakeLists.txt), so
// only its declarations are wanted here. It has no header of its own; this is
// the block the file itself documents as the way to use it from C++.
extern "C" {
struct stb_vorbis;
struct stb_vorbis_alloc;
typedef struct {
    unsigned int sample_rate;
    int channels;
    unsigned int setup_memory_required;
    unsigned int setup_temp_memory_required;
    unsigned int temp_memory_required;
    int max_frame_size;
} stb_vorbis_info;

stb_vorbis* stb_vorbis_open_memory(const unsigned char* data, int len, int* error,
                                   const stb_vorbis_alloc* alloc_buffer);
void stb_vorbis_close(stb_vorbis* f);
stb_vorbis_info stb_vorbis_get_info(stb_vorbis* f);
int stb_vorbis_seek_start(stb_vorbis* f);
int stb_vorbis_get_samples_short(stb_vorbis* f, int channels, short** buffer, int num_samples);
int stb_vorbis_get_samples_float_interleaved(stb_vorbis* f, int channels, float* buffer,
                                             int num_floats);
}
#endif

namespace tl::core {
namespace {

/// How many source frames the resampler keeps between calls: a block plus the
/// frame interpolation reads ahead, generously.
[[maybe_unused]] constexpr size_t kSourceCapacity = 8192;  // read by the Vorbis path only

/// Everything the mixer knows about one playing track.
struct Track {
    /// The compressed file. Held for as long as the decoder is open, because
    /// stb_vorbis reads straight out of it rather than copying.
    std::vector<uint8_t> data;
#if defined(TL_HAVE_VORBIS)
    stb_vorbis* vorbis = nullptr;
#endif
    /// A RIFF/WAVE file decoded whole, stereo interleaved, in place of the
    /// Vorbis stream (some ambient loops are not Ogg). `pcm_at` is the next frame.
    std::vector<float> pcm;
    size_t pcm_at = 0;
    std::string name;
    int source_rate = 44100;
    bool loop = true;
    float gain = 1.0f;
    bool ended = false;

    /// Decoded source frames, stereo interleaved, and where the resampler has
    /// got to within them. `cursor` is in frames and fractional: it is the
    /// position in the *source* that the next output frame samples.
    std::vector<float> source;
    size_t source_frames = 0;
    double cursor = 0.0;

    bool playing() const {
#if defined(TL_HAVE_VORBIS)
        return (vorbis != nullptr || !pcm.empty()) && !ended;
#else
        return false;
#endif
    }
};

/// One playing one-shot. See `audio_play_voice()`. Resampled as a track is: the
/// device is 48 kHz on the console, every file 44.1.
struct Voice {
    SamplePtr sample;
    std::string key;
    double cursor = 0.0;
    float gain = 1.0f;
    /// Per-channel multipliers from the pan, computed once at the start.
    float left = 1.0f;
    float right = 1.0f;
    /// Started again from the top at its end; see `audio_play_voice()`.
    bool loop = false;
    /// Held where it is (`FMOD::Channel::setPaused`): not mixed, not moved on,
    /// not swept.
    bool paused = false;
};

struct Mixer {
    std::mutex lock;
    std::unique_ptr<Track> tracks[static_cast<size_t>(Channel::Count)];

    /// The one-shot pool. A plain vector: it is capped at `kVoicePool` and
    /// swept once per block, so nothing here is worth an index.
    std::vector<Voice> voices;

    std::thread thread;
    std::atomic<bool> running{false};
    std::atomic<float> master{1.0f};
    /// `CSoundManager::updateAudioLevels`, as
    /// `audio_gains` turns it into two factors: the music's, and every other
    /// sound's.
    std::atomic<float> level_effects{1.0f};
    std::atomic<float> level_music{1.0f};

    int device_rate = 48000;
    size_t block = 1024;

    /// Scratch, owned by the mixer thread alone.
    std::vector<float> mix;
    std::vector<int16_t> out;
};

Mixer& mixer() {
    static Mixer m;
    return m;
}

#if defined(TL_HAVE_VORBIS)

/// Pull more source frames in behind whatever the resampler has not consumed.
/// False when the track has run out and is not looping; the caller still plays
/// what is in the buffer.
bool refill(Track& track, size_t wanted_frames) {
    if (track.source.size() < kSourceCapacity * 2) track.source.resize(kSourceCapacity * 2);
    wanted_frames = std::min(wanted_frames, kSourceCapacity);

    // A decoded RIFF/WAVE file: the same filling, out of memory.
    if (!track.pcm.empty()) {
        const size_t total = track.pcm.size() / 2;
        while (track.source_frames < wanted_frames) {
            if (track.pcm_at >= total) {
                if (!track.loop) return false;
                track.pcm_at = 0;
            }
            const size_t take = std::min(wanted_frames - track.source_frames, total - track.pcm_at);
            std::memcpy(track.source.data() + track.source_frames * 2,
                        track.pcm.data() + track.pcm_at * 2, take * 2 * sizeof(float));
            track.source_frames += take;
            track.pcm_at += take;
        }
        return true;
    }

    while (track.source_frames < wanted_frames) {
        const size_t room = wanted_frames - track.source_frames;
        float* into = track.source.data() + track.source_frames * 2;
        const int got = stb_vorbis_get_samples_float_interleaved(track.vorbis, 2, into,
                                                                 static_cast<int>(room * 2));
        if (got > 0) {
            track.source_frames += static_cast<size_t>(got);
            continue;
        }
        // The stream is over. A looping track goes back to the top and keeps
        // filling the same buffer, so the seam falls inside one block and is
        // not a gap; a one-shot stops here.
        if (!track.loop) return false;
        if (stb_vorbis_seek_start(track.vorbis) == 0) return false;
        // A zero-length or unseekable file would spin here forever otherwise.
        const int retry = stb_vorbis_get_samples_float_interleaved(
            track.vorbis, 2, track.source.data() + track.source_frames * 2,
            static_cast<int>((wanted_frames - track.source_frames) * 2));
        if (retry <= 0) return false;
        track.source_frames += static_cast<size_t>(retry);
    }
    return true;
}

/// Resample one track into `mix` (stereo interleaved, `frames` long), adding
/// rather than replacing, so the two channels sum.
void render(Track& track, float* mix, size_t frames, float master) {
    const double ratio =
        static_cast<double>(track.source_rate) / static_cast<double>(mixer().device_rate);

    // Linear interpolation between neighbouring source frames. The +2 is what
    // interpolation reads past the last output position.
    const size_t needed =
        static_cast<size_t>(track.cursor + static_cast<double>(frames) * ratio) + 2;
    const bool alive = refill(track, needed);

    const float gain = track.gain * master;
    size_t produced = 0;
    for (; produced < frames; ++produced) {
        const size_t index = static_cast<size_t>(track.cursor);
        if (index + 1 >= track.source_frames) break;
        const float t = static_cast<float>(track.cursor - static_cast<double>(index));
        const float* a = track.source.data() + index * 2;
        const float* b = a + 2;
        mix[produced * 2 + 0] += (a[0] + (b[0] - a[0]) * t) * gain;
        mix[produced * 2 + 1] += (a[1] + (b[1] - a[1]) * t) * gain;
        track.cursor += ratio;
    }

    // Drop what has been consumed, keeping the frame the next block will
    // interpolate from. Without the -1 the seam between blocks reads the wrong
    // neighbour and clicks once per block, which at 48 kHz is a 47 Hz buzz.
    const size_t consumed = static_cast<size_t>(track.cursor);
    if (consumed > 0 && consumed <= track.source_frames) {
        const size_t keep = track.source_frames - consumed;
        std::memmove(track.source.data(), track.source.data() + consumed * 2,
                     keep * 2 * sizeof(float));
        track.source_frames = keep;
        track.cursor -= static_cast<double>(consumed);
    }

    if (!alive && produced < frames) track.ended = true;
}

#endif  // TL_HAVE_VORBIS

/// Mix one voice into `mix`, returning false once it has run off its end.
/// Unconditional on TL_HAVE_VORBIS: the codec is optional, not the mixer.
bool render_voice(Voice& voice, float* mix, size_t frames, float master, int device_rate) {
    if (voice.paused) return true;
    return audio_mix_voice(*voice.sample, voice.cursor, voice.left * voice.gain * master,
                           voice.right * voice.gain * master, mix, frames, device_rate, voice.loop);
}

// maybe_unused: without a Vorbis decoder `audio_init` returns before starting it.
[[maybe_unused]] void mix_thread() {
    audio_device::prepare_mixer_thread();
    Mixer& m = mixer();
    while (m.running.load(std::memory_order_relaxed)) {
        const size_t frames = m.block;
        std::fill(m.mix.begin(), m.mix.end(), 0.0f);

        {
            std::lock_guard<std::mutex> guard(m.lock);
            const float all = m.master.load(std::memory_order_relaxed);
            const float master = all * m.level_effects.load(std::memory_order_relaxed);
#if defined(TL_HAVE_VORBIS)
            const float music = all * m.level_music.load(std::memory_order_relaxed);
            for (size_t channel = 0; channel < static_cast<size_t>(Channel::Count); ++channel) {
                auto& slot = m.tracks[channel];
                if (!slot || !slot->playing()) continue;
                render(*slot, m.mix.data(), frames,
                       channel == static_cast<size_t>(Channel::Music) ? music : master);
                if (slot->ended && !slot->loop) {
                    TL_LOG_INFO("audio: '%s' finished", slot->name.c_str());
                }
            }
#endif
            // The one-shots, swept in place: a voice that has run out is
            // erased here rather than left for a separate pass, so the pool
            // frees a slot on the block the sound ends on.
            for (size_t i = 0; i < m.voices.size();) {
                if (render_voice(m.voices[i], m.mix.data(), frames, master, m.device_rate)) {
                    ++i;
                } else {
                    m.voices[i] = std::move(m.voices.back());
                    m.voices.pop_back();
                }
            }
        }

        for (size_t i = 0; i < frames * 2; ++i) {
            m.out[i] = audio_to_pcm16(m.mix[i]);
        }
        if (!audio_device::write(m.out.data(), frames)) break;
    }
}

void close_track(std::unique_ptr<Track>& slot) {
#if defined(TL_HAVE_VORBIS)
    if (slot && slot->vorbis != nullptr) {
        stb_vorbis_close(slot->vorbis);
        slot->vorbis = nullptr;
    }
#endif
    slot.reset();
}

bool g_ready = false;

}  // namespace

bool audio_init() {
    if (g_ready) return true;

#if !defined(TL_HAVE_VORBIS)
    TL_LOG_WARN(
        "no sound: built without a Vorbis decoder - run "
        "`python scripts/fetch_stb.py` and rebuild");
    return false;
#else
    Mixer& m = mixer();
    const int rate = audio_device::open();
    if (rate <= 0) {
        TL_LOG_WARN("no sound: the platform has no audio output");
        return false;
    }
    m.device_rate = rate;
    m.block = audio_device::block_frames();
    m.mix.assign(m.block * 2, 0.0f);
    m.out.assign(m.block * 2, 0);

    m.running.store(true, std::memory_order_relaxed);
    m.thread = std::thread(mix_thread);
    g_ready = true;
    TL_LOG_INFO("audio: %d Hz stereo, %zu-frame blocks", rate, m.block);
    return true;
#endif
}

void audio_shutdown() {
    if (!g_ready) return;
    Mixer& m = mixer();

    // The mixer is blocked inside the device's write() waiting for a buffer, so
    // the flag alone will not wake it: closing the device is what releases it,
    // and write() then reports the failure that ends the loop.
    m.running.store(false, std::memory_order_relaxed);
    audio_device::close();
    if (m.thread.joinable()) m.thread.join();

    std::lock_guard<std::mutex> guard(m.lock);
    for (auto& slot : m.tracks) close_track(slot);
    m.voices.clear();
    g_ready = false;
}

SamplePtr audio_decode_ogg(const std::vector<uint8_t>& ogg, const std::string& name) {
#if !defined(TL_HAVE_VORBIS)
    (void)ogg;
    (void)name;
    return nullptr;
#else
    if (ogg.empty()) return nullptr;
    int error = 0;
    stb_vorbis* vorbis =
        stb_vorbis_open_memory(ogg.data(), static_cast<int>(ogg.size()), &error, nullptr);
    if (vorbis == nullptr) {
        TL_LOG_WARN("audio: could not decode '%s' (stb_vorbis error %d)", name.c_str(), error);
        return nullptr;
    }
    const stb_vorbis_info info = stb_vorbis_get_info(vorbis);
    const int rate = info.sample_rate > 0 ? static_cast<int>(info.sample_rate) : 44100;

    // Mono is what the pool pans: asking stb_vorbis for one channel does the
    // mixdown. It must be the non-interleaved call; the interleaved one mixes down
    // only to two channels and would overrun the one-channel buffer.
    auto sample = std::make_shared<Sample>();
    sample->channels = 1;
    sample->rate = rate;

    const size_t cap = static_cast<size_t>(kMaxVoiceSeconds * static_cast<float>(rate));
    constexpr size_t kChunk = 4096;
    std::vector<int16_t> pcm;
    for (;;) {
        const size_t was = pcm.size();
        pcm.resize(was + kChunk);
        short* into[1] = {pcm.data() + was};
        const int got = stb_vorbis_get_samples_short(vorbis, 1, into, static_cast<int>(kChunk));
        if (got <= 0) {
            pcm.resize(was);
            break;
        }
        pcm.resize(was + static_cast<size_t>(got));
        if (pcm.size() >= cap) {
            TL_LOG_WARN("audio: '%s' is longer than %.0f s; not a voice", name.c_str(),
                        static_cast<double>(kMaxVoiceSeconds));
            stb_vorbis_close(vorbis);
            return nullptr;
        }
    }
    stb_vorbis_close(vorbis);
    if (pcm.empty()) return nullptr;

    sample->frames = pcm.size();
    sample->samples = std::move(pcm);
    return sample;
#endif
}

void audio_pan_gains(float pan, float& left, float& right) {
    const float clamped = std::max(-1.0f, std::min(1.0f, pan));
    const float angle = (clamped + 1.0f) * 0.25f * 3.14159265358979323846f;
    left = std::cos(angle);
    right = std::sin(angle);
}

bool audio_mix_voice(const Sample& sample, double& cursor, float left, float right, float* mix,
                     size_t frames, int device_rate, bool loop) {
    if (sample.frames == 0 || sample.channels <= 0) return false;
    // A loop needs two frames to interpolate between before it can wrap.
    if (loop && sample.frames < 2) return false;

    const double ratio = static_cast<double>(sample.rate) /
                         static_cast<double>(device_rate > 0 ? device_rate : 48000);
    const int channels = sample.channels;
    constexpr float kToFloat = 1.0f / 32768.0f;

    for (size_t produced = 0; produced < frames; ++produced) {
        size_t index = static_cast<size_t>(cursor);
        if (index + 1 >= sample.frames) {
            if (!loop) return false;
            cursor -= static_cast<double>(sample.frames - 1);
            index = static_cast<size_t>(cursor);
        }
        const float t = static_cast<float>(cursor - static_cast<double>(index));
        const int16_t* a = sample.samples.data() + index * static_cast<size_t>(channels);
        const int16_t* b = a + channels;
        // Mono is the whole archive, so it is the case worth writing out: one
        // source sample lands on both outputs, scaled by the pan.
        const float l0 = static_cast<float>(a[0]) * kToFloat;
        const float l1 = static_cast<float>(b[0]) * kToFloat;
        const float r0 = channels > 1 ? static_cast<float>(a[1]) * kToFloat : l0;
        const float r1 = channels > 1 ? static_cast<float>(b[1]) * kToFloat : l1;
        mix[produced * 2 + 0] += (l0 + (l1 - l0) * t) * left;
        mix[produced * 2 + 1] += (r0 + (r1 - r0) * t) * right;
        cursor += ratio;
    }
    return true;
}

int16_t audio_to_pcm16(float mixed) {
    const float sample = std::max(-1.0f, std::min(1.0f, mixed));
    return static_cast<int16_t>(sample * 32767.0f);
}

bool audio_play_voice(SamplePtr sample, const std::string& key, float gain, float pan, bool loop) {
    if (!g_ready || !sample || sample->frames == 0) return false;
    Mixer& m = mixer();
    std::lock_guard<std::mutex> guard(m.lock);

    // The pool first, then this sound's own cap, both the original's:
    // `FMOD::System::init(64)` and `setMaxAudible(6)` per sample group. Over either
    // the sound is dropped rather than stealing a voice (`..._BEHAVIOR_MUTE`).
    if (m.voices.size() >= kVoicePool) return false;
    int already = 0;
    for (const Voice& voice : m.voices) {
        if (voice.key == key && ++already >= kMaxAudiblePerSound) return false;
    }

    Voice voice;
    voice.sample = std::move(sample);
    voice.key = key;
    voice.gain = std::max(0.0f, gain);
    voice.loop = loop;
    audio_pan_gains(pan, voice.left, voice.right);
    m.voices.push_back(std::move(voice));
    return true;
}

void audio_set_voice_mix(const std::string& key, float gain, float pan) {
    if (!g_ready) return;
    Mixer& m = mixer();
    std::lock_guard<std::mutex> guard(m.lock);
    for (Voice& voice : m.voices) {
        if (voice.key != key) continue;
        voice.gain = std::max(0.0f, gain);
        audio_pan_gains(pan, voice.left, voice.right);
    }
}

void audio_set_voice_paused(const std::string& key, bool paused) {
    if (!g_ready) return;
    Mixer& m = mixer();
    std::lock_guard<std::mutex> guard(m.lock);
    for (Voice& voice : m.voices) {
        if (voice.key == key) voice.paused = paused;
    }
}

void audio_stop_voices() {
    if (!g_ready) return;
    Mixer& m = mixer();
    std::lock_guard<std::mutex> guard(m.lock);
    m.voices.clear();
}

void audio_stop_voice(const std::string& key) {
    if (!g_ready) return;
    Mixer& m = mixer();
    std::lock_guard<std::mutex> guard(m.lock);
    m.voices.erase(std::remove_if(m.voices.begin(), m.voices.end(),
                                  [&](const Voice& voice) { return voice.key == key; }),
                   m.voices.end());
}

bool audio_voice_playing(const std::string& key) {
    if (!g_ready || key.empty()) return false;
    Mixer& m = mixer();
    std::lock_guard<std::mutex> guard(m.lock);
    return std::any_of(m.voices.begin(), m.voices.end(),
                       [&](const Voice& voice) { return voice.key == key; });
}

size_t audio_voices_playing() {
    if (!g_ready) return 0;
    Mixer& m = mixer();
    std::lock_guard<std::mutex> guard(m.lock);
    return m.voices.size();
}

bool audio_available() { return g_ready; }

void audio_set_master(float gain) {
    mixer().master.store(std::max(0.0f, std::min(1.0f, gain)), std::memory_order_relaxed);
}

void audio_set_levels(float effects, float music) {
    Mixer& m = mixer();
    m.level_effects.store(std::max(0.0f, effects), std::memory_order_relaxed);
    m.level_music.store(std::max(0.0f, music), std::memory_order_relaxed);
}

// By value: the bytes are moved into the track. A build without stb_vorbis
// never gets that far, which is all cppcheck sees there.
// cppcheck-suppress passedByValue
// NOLINTNEXTLINE(performance-unnecessary-value-param)
bool audio_play(Channel channel, std::vector<uint8_t> ogg, const std::string& name, bool loop,
                float gain) {
    if (!g_ready) return false;
    if (ogg.empty()) {
        TL_LOG_WARN("audio: '%s' is empty", name.c_str());
        return false;
    }

#if !defined(TL_HAVE_VORBIS)
    (void)channel;
    (void)loop;
    (void)gain;
    return false;
#else
    auto track = std::make_unique<Track>();
    track->data = std::move(ogg);
    track->name = name;
    track->loop = loop;
    track->gain = gain;

    stb_vorbis_info info{};
    if (track->data.size() >= 12 && std::memcmp(track->data.data(), "RIFF", 4) == 0) {
        // Not every loop the data names is Ogg: a RIFF/WAVE file is decoded
        // whole and played from memory.
        const tl::formats::WavSound wav =
            tl::formats::parse_wav(track->data.data(), track->data.size());
        if (!wav.ok) {
            TL_LOG_WARN("audio: could not decode '%s': %s", name.c_str(), wav.error.c_str());
            return false;
        }
        track->pcm.resize(wav.frames * 2);
        const size_t channels = static_cast<size_t>(wav.channels);
        for (size_t frame = 0; frame < wav.frames; ++frame) {
            const float left = static_cast<float>(wav.samples[frame * channels]) / 32768.0f;
            const float right =
                channels > 1 ? static_cast<float>(wav.samples[frame * channels + 1]) / 32768.0f
                             : left;
            track->pcm[frame * 2] = left;
            track->pcm[frame * 2 + 1] = right;
        }
        track->data.clear();
        track->data.shrink_to_fit();
        info.sample_rate = static_cast<unsigned int>(wav.sample_rate);
        info.channels = wav.channels;
    } else {
        int error = 0;
        track->vorbis = stb_vorbis_open_memory(
            track->data.data(), static_cast<int>(track->data.size()), &error, nullptr);
        if (track->vorbis == nullptr) {
            TL_LOG_WARN("audio: could not decode '%s' (stb_vorbis error %d)", name.c_str(), error);
            return false;
        }
        info = stb_vorbis_get_info(track->vorbis);
    }
    track->source_rate = static_cast<int>(info.sample_rate);
    if (track->source_rate <= 0) track->source_rate = 44100;
    track->source.resize(kSourceCapacity * 2);

    Mixer& m = mixer();
    {
        std::lock_guard<std::mutex> guard(m.lock);
        std::unique_ptr<Track>& slot = m.tracks[static_cast<size_t>(channel)];
        close_track(slot);
        slot = std::move(track);
    }
    const char* chan_name =
        channel == Channel::Music ? "music" : (channel == Channel::Ambient ? "ambient" : "voice");
    TL_LOG_INFO("audio: %s '%s' (%u Hz, %d ch, %s, gain %.2f)", chan_name, name.c_str(),
                info.sample_rate, info.channels, loop ? "looping" : "one shot",
                static_cast<double>(gain));
    return true;
#endif
}

void audio_stop(Channel channel) {
    if (!g_ready) return;
    Mixer& m = mixer();
    std::lock_guard<std::mutex> guard(m.lock);
    close_track(m.tracks[static_cast<size_t>(channel)]);
}

void audio_set_channel_gain(Channel channel, float gain) {
    if (!g_ready) return;
    Mixer& m = mixer();
    std::lock_guard<std::mutex> guard(m.lock);
    std::unique_ptr<Track>& slot = m.tracks[static_cast<size_t>(channel)];
    if (slot) slot->gain = gain;
}

bool audio_is_channel_playing(Channel channel) {
    if (!g_ready) return false;
    Mixer& m = mixer();
    std::lock_guard<std::mutex> guard(m.lock);
    const std::unique_ptr<Track>& slot = m.tracks[static_cast<size_t>(channel)];
    return slot && slot->playing();
}

std::string audio_playing(Channel channel) {
    if (!g_ready) return {};
    Mixer& m = mixer();
    std::lock_guard<std::mutex> guard(m.lock);
    const std::unique_ptr<Track>& slot = m.tracks[static_cast<size_t>(channel)];
    return slot ? slot->name : std::string();
}

}  // namespace tl::core
