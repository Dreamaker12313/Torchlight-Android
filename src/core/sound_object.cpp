#include "core/sound_object.h"

#include "core/logic_events.h"
#include "core/random.h"

#include <cmath>

namespace tl::core {

namespace {

/// `FMOD::System::getChannel` takes an index under the manager's 64
/// (`cmpwi r4, 0x40`, and -1 for none).
bool valid_channel(int32_t id) { return id != -1 && id < 0x40; }

}  // namespace

// `CSoundBank::configureVolumeScale`: the
// listener (`CSoundManager +0x14`/`+0x1c`) against the point across the ground,
// `fmadds` for the sum of the squares, the distance held to the radius.
float configure_volume_scale(float radius, const Vec3& listener, const Vec3& at) {
    if (radius == 0.0f) return 0.0f;
    const float dx = listener.x - at.x;
    const float dz = listener.z - at.z;
    const float d = std::sqrt(std::fma(dz, dz, dx * dx));
    const float held = radius < d ? radius : d;
    return 1.0f - held / radius;
}

// `CSoundBank::playSample`.
int32_t sound_bank_play_sample(NodeSoundBank& bank, int32_t slot, const Vec3* node, float volume,
                               float radius, bool untracked, const Vec3& listener,
                               SoundBankHost& host, RunicRandom& volatile_rng) {
    if (!bank.sound_on) return -1;
    if (slot < 0 || static_cast<size_t>(slot) >= bank.slots.size()) return -1;
    const BankSample& sample = bank.slots[static_cast<size_t>(slot)];
    // The play chance first, on the volatile stream: played when the roll of
    // 1..100 is under it.
    if (0 <= sample.chance && sample.chance <= volatile_rng.integer_between(1, 100)) return -1;
    if (volume == 0.0f) volume = sample.volume;
    const float scaled = bank.volume_scale * volume;
    if (radius == 0.0f) radius = sample.radius;
    if (sample.files.empty()) return -1;
    float at_node = 1.0f;
    if (node != nullptr) at_node = configure_volume_scale(radius, listener, *node);
    bank.last_volume = at_node * scaled;
    const int32_t pick =
        volatile_rng.integer_between(0, static_cast<int32_t>(sample.files.size()) - 1);
    const std::string& file =
        sample.files[pick < 0 || static_cast<size_t>(pick) >= sample.files.size()
                         ? 0
                         : static_cast<size_t>(pick)];
    // A looping bank lets what it holds fade (`stop(false)`); any other forgets
    // its channels without stopping them.
    if (bank.looping) {
        sound_bank_stop(bank, false, host);
    } else {
        bank.channels.clear();
    }
    if (bank.fade_time != 0.0f) bank.last_volume = 0.001f;
    // Placed at the node only when the bank loops (`r5 = looping ? node : 0`);
    // a one-shot is heard flat at the volume it started with.
    const int32_t channel = host.play ? host.play(file, bank.last_volume, sample.variation,
                                                  bank.looping ? node : nullptr, bank.looping)
                                      : -1;
    if (!untracked && channel != -1) {
        NodeSoundBank::Channel added;
        added.id = channel;
        added.slot = slot;
        added.fade = 0.001f;
        bank.channels.push_back(added);
    }
    bank.dirty = true;
    return channel;
}

// `CSoundBank::stop(bool)` (the Linux build's `stop()`,
// 0xa68b9x, has only the immediate half).
void sound_bank_stop(NodeSoundBank& bank, bool immediate, SoundBankHost& host) {
    if (bank.fade_time != 0.0f && !immediate) {
        for (NodeSoundBank::Channel& channel : bank.channels) channel.fading_out = true;
        return;
    }
    for (const NodeSoundBank::Channel& channel : bank.channels) {
        if (host.stop) host.stop(channel.id);
    }
    bank.channels.clear();
    bank.dirty = true;
}

// `CSoundBank::clear`. The console tests
// `+0xac`, the loop flag, before it stops; releasing a slot's recordings is
// the mixer's (`CSoundManager::releaseSound`), the port's own.
void sound_bank_clear(NodeSoundBank& bank, SoundBankHost& host) {
    if (bank.looping) sound_bank_stop(bank, false, host);
    bank.slots.clear();
}

// `CSoundBank::update`.
void sound_bank_update(NodeSoundBank& bank, float delta, const Vec3* node, const Vec3& listener,
                       SoundBankHost& host, RunicRandom& volatile_rng) {
    // Ended channels are let go (a looping bank's stopped first). After a removal the walk
    // restarts at the second channel: the one swapped in waits a frame.
    const bool had_channels = !bank.channels.empty();
    for (size_t i = 0; i < bank.channels.size(); ++i) {
        const int32_t id = bank.channels[i].id;
        if (id != -1 && host.playing && host.playing(id)) continue;
        if (bank.looping && host.stop) host.stop(id);
        bank.channels[i] = bank.channels.back();
        bank.channels.pop_back();
        i = 0;
    }
    if (had_channels && bank.channels.empty()) return;
    if (node == nullptr && bank.fade_time == 0.0f) return;
    if (!bank.looping) return;

    const Vec3 at = node != nullptr ? *node : Vec3{};
    // A listener not yet placed (at the origin) standing on the node.
    if (listener.x == 0.0f && listener.z == 0.0f && at.x == listener.x && at.y == listener.y &&
        at.z == listener.z) {
        return;
    }
    const float scale = node != nullptr ? configure_volume_scale(bank.radius, listener, at) : 1.0f;
    const float volume = bank.volume_scale * scale;
    if (!(0.0f < volume) && !bank.channels.empty()) {
        bank.last_volume = 0.0f;
        sound_bank_stop(bank, false, host);
        return;
    }
    if (0.001f < volume && bank.channels.empty() && bank.looping) {
        sound_bank_play_sample(bank, 0, node, 0.0f, bank.radius, false, listener, host,
                               volatile_rng);
        bank.dirty = true;
    }
    if (bank.fade_time != 0.0f) {
        for (const NodeSoundBank::Channel& channel : bank.channels) {
            if (channel.fading_out) bank.dirty = true;
        }
        for (const NodeSoundBank::Channel& channel : bank.channels) {
            if (!channel.fading_out && channel.fade < bank.fade_time) bank.dirty = true;
        }
    }
    if (!bank.dirty && listener.x == bank.last_listener.x && listener.y == bank.last_listener.y &&
        listener.z == bank.last_listener.z) {
        return;
    }
    bank.dirty = false;
    bank.last_listener = listener;
    bank.last_volume = volume;
    for (NodeSoundBank::Channel& channel : bank.channels) {
        if (bank.fade_time != 0.0f) {
            if (channel.fading_out) {
                channel.fade = channel.fade - delta;
                if (channel.fade < 0.0f) {
                    channel.fade = 0.0f;
                    if (host.stop) host.stop(channel.id);
                    channel.id = -1;
                }
            } else {
                channel.fade = channel.fade + delta;
                if (bank.fade_time < channel.fade) channel.fade = bank.fade_time;
            }
        }
        if (channel.id == -1) continue;
        float heard = volume;
        if (bank.fade_time != 0.0f) heard = channel.fade / bank.fade_time * volume;
        if (valid_channel(channel.id) && host.set_volume) host.set_volume(channel.id, heard);
    }
}

namespace {

/// `CSoundObject::setVisible`: the node shown or hidden, and
/// the bank's next update made to run.
void set_visible(SoundObject& o, bool visible) {
    o.visible = visible;
    o.bank.dirty = true;
}

}  // namespace

// `CSoundObject::play`.
void sound_object_play(SoundObject& o, const Vec3& listener, SoundBankHost& host,
                       RunicRandom& volatile_rng, const SoundOutput& out) {
    if (!o.bank.sound_on && !o.environmental) return;
    o.bank.dirty = true;
    if (!o.visible) o.visible = true;
    sound_bank_stop(o.bank, false, host);
    o.playing = true;
    // A node that is not positional is heard everywhere: its bank and every
    // slot at radius 0 - `CSoundBank::setRadius(0)`, inlined.
    //
    if (!o.environmental) {
        o.bank.radius = 0.0f;
        o.bank.dirty = true;
        for (BankSample& slot : o.bank.slots) slot.radius = 0.0f;
    }
    // A positional loop waits for the bank's update to start it, as the
    // listener comes within its radius.
    if (!(o.bank.looping && o.environmental)) {
        const Vec3* node = o.environmental ? &o.at : nullptr;
        if (sound_bank_play_sample(o.bank, 0, node, 0.0f, 0.0f, false, listener, host,
                                   volatile_rng) < 0) {
            o.playing = false;
        }
    }
    if (out) out(logic::kPlaying);
    if (o.paused) sound_object_pause(o, host);
}

// `CSoundObject::stop`.
void sound_object_stop(SoundObject& o, SoundBankHost& host, const SoundOutput& out) {
    sound_bank_stop(o.bank, false, host);
    if (out) out(logic::kSoundStopped);
    o.playing = false;
}

// `CSoundObject::pause`, with
// `CSoundBank::pause` inlined: each channel
// held, the bank made to update.
void sound_object_pause(SoundObject& o, SoundBankHost& host) {
    for (const NodeSoundBank::Channel& channel : o.bank.channels) {
        if (valid_channel(channel.id) && host.set_paused) host.set_paused(channel.id, true);
    }
    o.bank.dirty = true;
    o.paused = true;
}

// `CSoundObject::resume`, with
// `CSoundBank::resume` inlined.
void sound_object_resume(SoundObject& o, SoundBankHost& host) {
    for (const NodeSoundBank::Channel& channel : o.bank.channels) {
        if (valid_channel(channel.id) && host.set_paused) host.set_paused(channel.id, false);
    }
    o.bank.dirty = true;
    o.paused = false;
}

// `CSoundObject::setEnabled`.
void sound_object_set_enabled(SoundObject& o, bool enabled, const Vec3& listener,
                              SoundBankHost& host, RunicRandom& volatile_rng,
                              const SoundOutput& out) {
    o.enabled = enabled;
    if (!enabled || !o.start_on_activate) return;
    sound_object_play(o, listener, host, volatile_rng, out);
    o.bank.dirty = true;
}

// `CSoundObject::updateSounds`.
void sound_object_update(SoundObject& o, float delta, const Vec3& listener, SoundBankHost& host,
                         RunicRandom& volatile_rng, const SoundOutput& out) {
    if (!o.enabled || !o.playing || o.paused || !o.bank.sound_on) return;
    sound_bank_update(o.bank, delta, o.environmental ? &o.at : nullptr, listener, host,
                      volatile_rng);
    // A sound heard everywhere is over when its bank holds nothing (a
    // positional loop is the bank's to start and stop as the player moves).
    if (o.bank.channels.empty() && !o.environmental) {
        o.playing = false;
        if (out) out(logic::kSoundEnded);
    }
}

// `CSoundObjectDescriptor::InputLogicEvent`.
void sound_object_input(SoundObject& o, uint32_t in, const Vec3& listener, SoundBankHost& host,
                        RunicRandom& volatile_rng, const SoundOutput& out) {
    switch (in) {
        case logic::kPlay:
            set_visible(o, true);
            sound_object_set_enabled(o, true, listener, host, volatile_rng, out);
            sound_object_play(o, listener, host, volatile_rng, out);
            break;
        case logic::kStop: sound_object_stop(o, host, out); break;
        case logic::kPause: sound_object_pause(o, host); break;
        case logic::kResume:
            set_visible(o, true);
            sound_object_set_enabled(o, true, listener, host, volatile_rng, out);
            sound_object_resume(o, host);
            break;
        default: break;
    }
}

// The descriptor's scene activation: the nodes that start
// on activation, shown, enabled (which plays them) and played.
void sound_object_scene_activated(SoundObject& o, const Vec3& listener, SoundBankHost& host,
                                  RunicRandom& volatile_rng, const SoundOutput& out) {
    if (!o.start_on_activate) return;
    set_visible(o, true);
    sound_object_set_enabled(o, true, listener, host, volatile_rng, out);
    sound_object_play(o, listener, host, volatile_rng, out);
}

// `CSoundObjectDescriptor::descriptorSceneDeactivated`.
//
void sound_object_scene_deactivated(SoundObject& o, const Vec3& listener, SoundBankHost& host,
                                    RunicRandom& volatile_rng, const SoundOutput& out) {
    sound_object_set_enabled(o, false, listener, host, volatile_rng, out);
}

}  // namespace tl::core
