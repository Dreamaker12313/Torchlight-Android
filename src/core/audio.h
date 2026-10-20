// Sound: a Vorbis decoder, a two-channel mixer, and the platform's PCM sink.
// Nothing here decides *what* to play: it is handed bytes. The compressed file
// arrives whole, in memory, and the decode streams, so the mixer thread never
// opens a file. The decoder is `external/stb/stb_vorbis.c`, linked statically;
// without it the engine builds without sound.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace tl::core {

/// Which of the streamed channels a track goes on: a level's `MUSIC` and its
/// layout's ambience are switched by different things and carry their own
/// volumes.
///
/// The channels are this engine's own: the original has one sound group,
/// `Music` (`CSoundManager::initialize`, `FMOD::System::init(64)`), and a group
/// per sample (`CSoundBank::addSample`, `setMaxAudible`). Positional one-shots
/// go to the voice pool below.
enum class Channel {
    Music,
    Ambient,
    Voice,
    Count,
};

/// Bring up the platform's audio device and start the mixing thread. False
/// when there is no device or decoder or the device refuses to open; that is
/// survivable, everything below becomes a no-op. The reason is logged once.
bool audio_init();

/// Stop the mixer, release the device, and free both channels.
void audio_shutdown();

/// True when audio_init() succeeded and there is something to play through.
bool audio_available();

/// Put an Ogg Vorbis file on a channel, replacing whatever was there.
///
/// `ogg` is the whole compressed file and its storage is taken over here, so
/// the caller may `std::move` a buffer it read out of the archive straight in.
/// `name` is only for the log. `gain` is the authored `VOLUME`, where 1.0 is
/// the file as mastered.
///
/// False when the bytes are not a Vorbis stream this decoder can read, which is
/// reported and otherwise harmless.
bool audio_play(Channel channel, std::vector<uint8_t> ogg, const std::string& name, bool loop,
                float gain);

/// Silence a channel and release its decoder.
void audio_stop(Channel channel);

/// Dynamically adjust the playback volume of a playing channel.
void audio_set_channel_gain(Channel channel, float gain);

/// True when a channel has a track currently playing and not yet ended.
bool audio_is_channel_playing(Channel channel);

/// What is playing on a channel, for the log and for "is this already on?".
/// Empty when the channel is silent.
std::string audio_playing(Channel channel);

// ---------------------------------------------------------------------------
// The voice pool: one-shot samples, mixed over the streamed channels. The
// original's shape: `CSoundManager::initialize` calls `FMOD::System::init(0x40, ...)`,
// and `CSoundBank::addSample` makes a sound group per sample with
// `FMOD::SoundGroup::setMaxAudible(6)` and `FMOD::SoundGroup::setMaxAudibleBehavior(1)`
// (mute): a pool of 64 with a cap of six on any one sound. A voice is a decoded
// buffer, not a decoder.
// ---------------------------------------------------------------------------

/// A decoded effect, shared between every voice playing it.
struct Sample {
    std::vector<int16_t> samples;  ///< interleaved, `frames * channels` long
    int channels = 1;
    int rate = 44100;
    size_t frames = 0;
};

using SamplePtr = std::shared_ptr<const Sample>;

/// How many voices play at once, and how many of any one sound.
inline constexpr size_t kVoicePool = 64;
inline constexpr int kMaxAudiblePerSound = 6;

/// Decode a whole Ogg Vorbis file into a `Sample`, for the voice pool (a quarter
/// of the one-shots are Ogg, every voice line among them). Null when the bytes
/// are not Vorbis, when the build has no decoder, or past `kMaxVoiceSeconds` (a
/// guard against a music track in the pool).
SamplePtr audio_decode_ogg(const std::vector<uint8_t>& ogg, const std::string& name);

/// The longest thing `audio_decode_ogg()` will hold in memory.
inline constexpr float kMaxVoiceSeconds = 60.0f;

/// Start one voice. False when the pool is full, when `key` already has
/// `kMaxAudiblePerSound` playing, or when there is no device.
///
/// `key` is the sound definition, not the file (the original's group is per
/// sample). `gain` includes the caller's distance attenuation; `pan` is -1 hard
/// left to +1 hard right. A `loop` voice wraps and plays until stopped by key,
/// its mix kept current with `audio_set_voice_mix()`.
bool audio_play_voice(SamplePtr sample, const std::string& key, float gain, float pan,
                      bool loop = false);

/// A new gain and pan for every voice playing under `key`, from the next block.
void audio_set_voice_mix(const std::string& key, float gain, float pan);

/// Hold or release every voice under `key` where it stands
/// (`FMOD::Channel::setPaused`, which `CSoundObject::pause` and `resume` call).
void audio_set_voice_paused(const std::string& key, bool paused);

/// Stop every voice. Called when a level goes away, so a bark does not outlive
/// the dog that made it.
void audio_stop_voices();

/// Stop the voices started under `key` (`CSoundBank::stop(slot)`).
void audio_stop_voice(const std::string& key);

/// How many voices are sounding right now. For the log and for the tests.
size_t audio_voices_playing();

/// Whether a voice started under `key` is still sounding
/// (`CSoundManager::isChannelPlaying`, for the
/// one channel the voice queue keeps). False with no device.
bool audio_voice_playing(const std::string& key);

// ---------------------------------------------------------------------------
// The mixer's arithmetic: pure functions, which the mixer thread runs on every
// block and the tests run on their own.
// ---------------------------------------------------------------------------

/// Constant-power pan, so a sound crossing the middle does not dip: `pan`
/// clamped to [-1, 1], then left = cos and right = sin of (pan + 1) * pi / 4.
void audio_pan_gains(float pan, float& left, float& right);

/// Add a voice's next `frames` output frames to `mix` (stereo, interleaved),
/// resampled linearly from `sample.rate` to `device_rate` (48000 when not
/// positive) and scaled by `left` and `right`; a mono sample feeds both sides.
/// `cursor` is the position in source frames and advances by the rate ratio
/// per output frame. False once the voice has run off its end - the frames
/// before that point are mixed, the rest of `mix` is left as it was. A `loop`
/// voice never runs off: its cursor wraps back by the sample's length.
bool audio_mix_voice(const Sample& sample, double& cursor, float left, float right, float* mix,
                     size_t frames, int device_rate, bool loop = false);

/// One mixed sample as the device takes it: clamped to [-1, 1] (a NaN comes
/// out of the clamp as 1), scaled by 32767 and truncated.
int16_t audio_to_pcm16(float mixed);

/// Scale everything, after the per-track gains. 0 is silence, 1 is unattenuated
/// and the default: how loud the game plays is the settings' business
/// (`audio_set_levels`), which default to the original's 0.8 and 0.5.
void audio_set_master(float gain);

/// The settings' two volumes and mutes as the mixer applies them: `music` on
/// the music channel, `effects` on every other channel and voice. What
/// `tl::core::audio_gains` makes of a `CSoundManager::updateAudioLevels`
/// call.
void audio_set_levels(float effects, float music);

// ---------------------------------------------------------------------------
// The platform's half. Implemented in src/platform/<target>/platform_audio.cpp
// and called only by the mixer; nothing else should touch it.
// ---------------------------------------------------------------------------
namespace audio_device {

/// Open a 16-bit signed, stereo, interleaved output. Returns the rate the
/// device runs at (the console's is fixed at 48 kHz), or 0 when there is none.
int open();

void close();

/// Hand the device one block of stereo frames, waiting for a free buffer: the
/// device's pacing is the mixer's clock. False once the device has gone away.
bool write(const int16_t* interleaved, size_t frames);

/// How many frames one write() should carry. The device picks it, because the
/// alignment rules are the device's: `audout` wants whole 0x1000-byte buffers.
size_t block_frames();

/// Called once by the mixer thread before its first block, so a platform can
/// place the thread (the console moves it off the game's core).
void prepare_mixer_thread();

}  // namespace audio_device

}  // namespace tl::core
