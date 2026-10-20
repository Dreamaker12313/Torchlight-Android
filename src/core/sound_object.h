// A placed `Sound` node: `CSoundObject` and the console's `CSoundBank` it owns.
// Only an editor setter sets the fade time, so the fade
// branches are kept but do not run in the game. The mixer's channels are `SoundBankHost`'s.
#pragma once

#include "core/collision.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace tl::core {

class RunicRandom;

/// One slot of a bank, as `CSoundBank::addSample(slot, name, loop, volume, ..,
/// radius, chance)` leaves it. A radius or volume of 0 takes the bank's;
/// a chance below 0 always plays.
struct BankSample {
    std::vector<std::string> files;
    float radius = 0.0f;
    float volume = 0.0f;
    int32_t chance = -1;
    /// The definition's `VOLUMEVARIATION`, which `CSoundManager::playSound`
    /// adds to the volume it starts a channel at.
    float variation = 0.0f;
};

/// The console's `CSoundBank`, the part a `Sound` node's uses; defaults from
/// `CSoundBank::CSoundBank(manager, dirty)`.
struct NodeSoundBank {
    struct Channel {
        int32_t id = -1;          ///< `+0xc`: the manager's channel, -1 once stopped
        int32_t slot = 0;         ///< `+0x1c`
        bool fading_out = false;  ///< `+0x2c`
        float fade = 0.0f;        ///< `+0x3c`
    };
    std::vector<Channel> channels;
    std::vector<BankSample> slots;
    bool dirty = false;         ///< `+0x8c`, the constructor's argument
    float volume_scale = 1.0f;  ///< `+0x90`, the node's `VOLUME`
    float radius = 10.0f;       ///< `+0x94`; a `Sound` node's is 5 until `RADIUS`
    Vec3 last_listener;         ///< `+0x9c`..`+0xa4`
    float last_volume = 0.0f;   ///< `+0xa8`
    bool looping = false;       ///< `+0xac`, the definition's loop flag
    float fade_time = 0.0f;     ///< `+0xb0`
    /// Whether the manager plays sounds at all (`CSoundManager +0x375`, `+0x374`): off,
    /// `playSample` plays nothing and only a positional node's `play` goes on.
    bool sound_on = true;
};

/// What a bank asks of the mixer.
struct SoundBankHost {
    /// `CSoundManager::playSound(file, volume, node)`: a new channel at
    /// `volume` plus up to `variation`, placed at `at` if given; -1 when none could start.
    std::function<int32_t(const std::string& file, float volume, float variation, const Vec3* at,
                          bool loop)>
        play;
    /// `CSoundManager::isChannelPlaying`.
    std::function<bool(int32_t channel)> playing;
    /// `CSoundManager::stopSound`.
    std::function<void(int32_t channel)> stop;
    /// `CSoundManager::setSoundVolume(channel, volume)`: nothing for -1 or
    /// past the 64; else `FMOD::Channel::setVolume`.
    std::function<void(int32_t channel, float volume)> set_volume;
    /// `CSoundManager::setPauseSound(channel, paused)`: the
    /// same bounds, then `FMOD::Channel::setPaused`.
    std::function<void(int32_t channel, bool paused)> set_paused;
};

/// `CSoundBank::configureVolumeScale(radius, at)`:
/// 1 at the listener, 0 at the radius and beyond,
/// by the distance across the ground; 0 for a radius of 0.
float configure_volume_scale(float radius, const Vec3& listener, const Vec3& at);

/// `CSoundBank::playSample(slot, node, volume, radius, untracked)`.
/// `node` null for none. Returns the
/// channel, or -1.
int32_t sound_bank_play_sample(NodeSoundBank& bank, int32_t slot, const Vec3* node, float volume,
                               float radius, bool untracked, const Vec3& listener,
                               SoundBankHost& host, RunicRandom& volatile_rng);

/// `CSoundBank::stop(immediate)`: with a fade time and not
/// `immediate`, every channel starts fading out; otherwise each is stopped
/// and the lists emptied.
void sound_bank_stop(NodeSoundBank& bank, bool immediate, SoundBankHost& host);

/// `CSoundBank::clear()`: a looping bank
/// is stopped first (`stop(false)`, so with a fade time it fades), then every
/// slot's sounds are released and the slots emptied.
void sound_bank_clear(NodeSoundBank& bank, SoundBankHost& host);

/// `CSoundBank::update(delta, node)`.
void sound_bank_update(NodeSoundBank& bank, float delta, const Vec3* node, const Vec3& listener,
                       SoundBankHost& host, RunicRandom& volatile_rng);

/// A `Sound` node's `CSoundObject`; defaults from `CSoundObject::CSoundObject`,
/// its bank at radius 5. `environmental` is positional.
struct SoundObject {
    NodeSoundBank bank;
    Vec3 at;
    bool visible = true;
    bool enabled = true;
    bool start_on_activate = true;
    bool playing = false;
    bool paused = false;
    bool environmental = true;
};

/// The outputs a sound object broadcasts: `Playing` (0xb), `Sound Stopped`
/// (0xc), `Sound Ended` (0xd).
using SoundOutput = std::function<void(uint32_t event)>;

/// `CSoundObject::play`.
void sound_object_play(SoundObject& o, const Vec3& listener, SoundBankHost& host,
                       RunicRandom& volatile_rng, const SoundOutput& out);
/// `CSoundObject::stop`.
void sound_object_stop(SoundObject& o, SoundBankHost& host, const SoundOutput& out);
/// `CSoundObject::pause`.
void sound_object_pause(SoundObject& o, SoundBankHost& host);
/// `CSoundObject::resume`.
void sound_object_resume(SoundObject& o, SoundBankHost& host);
/// `CSoundObject::setEnabled`.
void sound_object_set_enabled(SoundObject& o, bool enabled, const Vec3& listener,
                              SoundBankHost& host, RunicRandom& volatile_rng,
                              const SoundOutput& out);
/// `CSoundObject::updateSounds(delta)`,
/// which `CSoundObjectDescriptor::update`
/// runs on each sound object every frame.
void sound_object_update(SoundObject& o, float delta, const Vec3& listener, SoundBankHost& host,
                         RunicRandom& volatile_rng, const SoundOutput& out);
/// `CSoundObjectDescriptor::InputLogicEvent`:
/// 9 `Play` and 12 `Resume` show and enable the
/// node first (which plays it again when it starts on activation), 10 `Stop`,
/// 11 `Pause`.
void sound_object_input(SoundObject& o, uint32_t in, const Vec3& listener, SoundBankHost& host,
                        RunicRandom& volatile_rng, const SoundOutput& out);
/// The descriptor's scene activation: a node that starts on
/// activation is shown, enabled and played.
void sound_object_scene_activated(SoundObject& o, const Vec3& listener, SoundBankHost& host,
                                  RunicRandom& volatile_rng, const SoundOutput& out);
/// `CSoundObjectDescriptor::descriptorSceneDeactivated`:
/// every node disabled.
void sound_object_scene_deactivated(SoundObject& o, const Vec3& listener, SoundBankHost& host,
                                    RunicRandom& volatile_rng, const SoundOutput& out);

}  // namespace tl::core
