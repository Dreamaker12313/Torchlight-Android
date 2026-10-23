// A placed `Sound` node (`core/sound_object`): `CSoundObject` and its
// `CSoundBank`, against a mixer made up for the test.

#include "core/sound_object.h"
#include "core/logic_events.h"
#include "core/random.h"
#include "test_support.h"

#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace {

using tl::core::BankSample;
using tl::core::NodeSoundBank;
using tl::core::RunicRandom;
using tl::core::SoundBankHost;
using tl::core::SoundObject;
using tl::core::Vec3;

/// The channels the bank started, by id, with what they were started with.
struct FakeMixer {
    struct Started {
        std::string file;
        float volume = 0.0f;
        bool positional = false;
        bool loop = false;
        bool playing = true;
        bool paused = false;
    };
    std::map<int32_t, Started> channels;
    int32_t next = 0;
    int stops = 0;

    SoundBankHost host() {
        SoundBankHost h;
        h.play = [this](const std::string& file, float volume, float, const Vec3* at, bool loop) {
            Started s;
            s.file = file;
            s.volume = volume;
            s.positional = at != nullptr;
            s.loop = loop;
            channels[next] = s;
            return next++;
        };
        h.playing = [this](int32_t id) {
            auto it = channels.find(id);
            return it != channels.end() && it->second.playing;
        };
        h.stop = [this](int32_t id) {
            ++stops;
            auto it = channels.find(id);
            if (it != channels.end()) it->second.playing = false;
        };
        h.set_volume = [this](int32_t id, float volume) {
            auto it = channels.find(id);
            if (it != channels.end()) it->second.volume = volume;
        };
        h.set_paused = [this](int32_t id, bool paused) {
            auto it = channels.find(id);
            if (it != channels.end()) it->second.paused = paused;
        };
        return h;
    }
};

SoundObject node_with(bool looping, bool environmental, float radius) {
    SoundObject o;
    o.at = {0.0f, 0.0f, 0.0f};
    o.environmental = environmental;
    o.bank.radius = radius;
    o.bank.looping = looping;
    BankSample sample;
    sample.files = {"media/sounds/ambient/a.ogg"};
    sample.radius = radius;
    sample.volume = 1.0f;
    o.bank.slots.push_back(sample);
    return o;
}

bool near(float a, float b) { return std::fabs(a - b) < 1e-5f; }

}  // namespace

// 1 at the listener, 0 at the radius, measured across the ground only.
TL_TEST(sound_volume_scale_falls_to_zero_at_the_radius) {
    TL_CHECK(near(tl::core::configure_volume_scale(10.0f, {3.0f, 50.0f, 4.0f}, {0, 0, 0}), 0.5f));
    TL_CHECK(near(tl::core::configure_volume_scale(10.0f, {0, 0, 0}, {0, 0, 0}), 1.0f));
    TL_CHECK(near(tl::core::configure_volume_scale(10.0f, {30.0f, 0, 0}, {0, 0, 0}), 0.0f));
    TL_CHECK(near(tl::core::configure_volume_scale(0.0f, {1.0f, 0, 0}, {0, 0, 0}), 0.0f));
}

// A positional loop does not start with its scene: the bank's update starts
// it once the listener is inside the radius, and stops it when they leave.
TL_TEST(positional_sound_loop_starts_and_stops_with_the_listener) {
    FakeMixer mixer;
    SoundBankHost host = mixer.host();
    RunicRandom rng(7);
    std::vector<uint32_t> outputs;
    auto out = [&](uint32_t e) { outputs.push_back(e); };
    SoundObject o = node_with(true, true, 10.0f);

    const Vec3 far{40.0f, 0.0f, 0.0f};
    tl::core::sound_object_scene_activated(o, far, host, rng, out);
    TL_CHECK(o.playing);
    TL_CHECK(mixer.channels.empty());
    TL_CHECK(!outputs.empty() && outputs.back() == tl::core::logic::kPlaying);

    tl::core::sound_object_update(o, 0.016f, far, host, rng, out);
    TL_CHECK(mixer.channels.empty());

    const Vec3 close{5.0f, 0.0f, 0.0f};
    tl::core::sound_object_update(o, 0.016f, close, host, rng, out);
    TL_CHECK_EQ(mixer.channels.size(), size_t{1});
    TL_CHECK(mixer.channels[0].loop);
    TL_CHECK(mixer.channels[0].positional);
    TL_CHECK(near(mixer.channels[0].volume, 0.5f));
    TL_CHECK_EQ(o.bank.channels.size(), size_t{1});

    tl::core::sound_object_update(o, 0.016f, far, host, rng, out);
    TL_CHECK(!mixer.channels[0].playing);
    TL_CHECK(o.bank.channels.empty());
    // Still playing as far as the node is concerned: it is the bank's to
    // start again.
    TL_CHECK(o.playing);
    tl::core::sound_object_update(o, 0.016f, close, host, rng, out);
    TL_CHECK_EQ(mixer.channels.size(), size_t{2});
}

// A sound heard everywhere plays at once with no position, and says
// `Sound Ended` when its channel has run out.
TL_TEST(level_wide_sound_ends_when_its_channel_does) {
    FakeMixer mixer;
    SoundBankHost host = mixer.host();
    RunicRandom rng(7);
    std::vector<uint32_t> outputs;
    auto out = [&](uint32_t e) { outputs.push_back(e); };
    SoundObject o = node_with(false, false, 0.0f);

    // Started twice, as the console does: `setEnabled(true)`, then `play` restarts it
    // (vtable slots 9 and 7, then).
    tl::core::sound_object_scene_activated(o, {100.0f, 0, 0}, host, rng, out);
    TL_CHECK_EQ(mixer.channels.size(), size_t{2});
    TL_CHECK(!mixer.channels[0].playing);
    TL_CHECK(mixer.channels[1].playing);
    TL_CHECK(!mixer.channels[1].positional);
    TL_CHECK(near(mixer.channels[1].volume, 1.0f));

    tl::core::sound_object_update(o, 0.016f, {100.0f, 0, 0}, host, rng, out);
    TL_CHECK(o.playing);
    mixer.channels[1].playing = false;
    tl::core::sound_object_update(o, 0.016f, {100.0f, 0, 0}, host, rng, out);
    TL_CHECK(!o.playing);
    TL_CHECK(outputs.back() == tl::core::logic::kSoundEnded);
}

// `START ON ACTIVATE` off: the scene leaves it silent until a `Play`.
TL_TEST(sound_node_waits_for_play_when_it_does_not_start_on_activate) {
    FakeMixer mixer;
    SoundBankHost host = mixer.host();
    RunicRandom rng(7);
    std::vector<uint32_t> outputs;
    auto out = [&](uint32_t e) { outputs.push_back(e); };
    SoundObject o = node_with(false, false, 0.0f);
    o.start_on_activate = false;

    tl::core::sound_object_scene_activated(o, {}, host, rng, out);
    TL_CHECK(mixer.channels.empty());
    TL_CHECK(outputs.empty());

    tl::core::sound_object_input(o, tl::core::logic::kPlay, {}, host, rng, out);
    TL_CHECK_EQ(mixer.channels.size(), size_t{1});
    TL_CHECK(o.playing);

    tl::core::sound_object_input(o, tl::core::logic::kStop, {}, host, rng, out);
    TL_CHECK(!mixer.channels[0].playing);
    TL_CHECK(!o.playing);
    TL_CHECK(outputs.back() == tl::core::logic::kSoundStopped);
}

// `Pause` holds the channels and the update leaves a paused node alone;
// `Resume` lets them go on.
TL_TEST(sound_node_pause_holds_its_channels) {
    FakeMixer mixer;
    SoundBankHost host = mixer.host();
    RunicRandom rng(7);
    auto out = [](uint32_t) {};
    SoundObject o = node_with(false, false, 0.0f);
    tl::core::sound_object_scene_activated(o, {}, host, rng, out);
    TL_CHECK_EQ(mixer.channels.size(), size_t{2});

    tl::core::sound_object_input(o, tl::core::logic::kPause, {}, host, rng, out);
    TL_CHECK(mixer.channels[1].paused);
    mixer.channels[1].playing = false;
    tl::core::sound_object_update(o, 0.016f, {}, host, rng, out);
    TL_CHECK(o.playing);  // not looked at while paused

    tl::core::sound_object_input(o, tl::core::logic::kResume, {}, host, rng, out);
    TL_CHECK(!o.paused);
}

// A play chance of 0 never plays (`chance <= roll`, the roll 1..100).
TL_TEST(sound_bank_play_chance_zero_never_plays) {
    FakeMixer mixer;
    SoundBankHost host = mixer.host();
    RunicRandom rng(7);
    NodeSoundBank bank;
    BankSample sample;
    sample.files = {"a.ogg"};
    sample.volume = 1.0f;
    sample.chance = 0;
    bank.slots.push_back(sample);
    for (int i = 0; i < 50; ++i) {
        TL_CHECK_EQ(
            tl::core::sound_bank_play_sample(bank, 0, nullptr, 0.0f, 0.0f, false, {}, host, rng),
            -1);
    }
    TL_CHECK(mixer.channels.empty());
}

// The console's walk over ended channels: after a removal it goes on from the
// second channel, so the one swapped into the emptied place waits a frame.
TL_TEST(sound_bank_update_skips_the_channel_swapped_in) {
    FakeMixer mixer;
    SoundBankHost host = mixer.host();
    RunicRandom rng(7);
    NodeSoundBank bank;
    for (int i = 0; i < 3; ++i) {
        NodeSoundBank::Channel c;
        c.id = mixer.host().play("x.ogg", 1.0f, 0.0f, nullptr, false);
        bank.channels.push_back(c);
    }
    mixer.channels[0].playing = false;  // ended
    mixer.channels[2].playing = false;  // ended, swapped into place 0
    tl::core::sound_bank_update(bank, 0.016f, nullptr, {}, host, rng);
    TL_CHECK_EQ(bank.channels.size(), size_t{2});
    TL_CHECK_EQ(bank.channels[0].id, 2);
    TL_CHECK_EQ(bank.channels[1].id, 1);
    tl::core::sound_bank_update(bank, 0.016f, nullptr, {}, host, rng);
    TL_CHECK_EQ(bank.channels.size(), size_t{1});
    TL_CHECK_EQ(bank.channels[0].id, 1);
}

// A bank going with its node: a looping one is cut at once, fade time or not;
// a one-shot plays out.
TL_TEST(sound_bank_destroy_cuts_a_loop_and_lets_a_one_shot_finish) {
    FakeMixer mixer;
    SoundBankHost host = mixer.host();
    RunicRandom rng(7);
    auto out = [](uint32_t) {};

    SoundObject loop = node_with(true, false, 0.0f);
    loop.bank.fade_time = 0.5f;
    tl::core::sound_object_scene_activated(loop, {}, host, rng, out);
    TL_CHECK(!loop.bank.channels.empty());
    const int32_t looping = loop.bank.channels.back().id;
    tl::core::sound_bank_destroy(loop.bank, host);
    TL_CHECK(!mixer.channels[looping].playing);
    TL_CHECK(loop.bank.channels.empty());
    TL_CHECK(loop.bank.slots.empty());

    SoundObject once = node_with(false, false, 0.0f);
    tl::core::sound_object_scene_activated(once, {}, host, rng, out);
    const int32_t one_shot = once.bank.channels.back().id;
    tl::core::sound_bank_destroy(once.bank, host);
    TL_CHECK(mixer.channels[one_shot].playing);
    TL_CHECK(once.bank.slots.empty());
}
