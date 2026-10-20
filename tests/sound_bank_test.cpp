// The 67 slots a `CSoundBank` has and the keys that fill them (`KSOUNDBANK_STRINGS`),
// misspellings included.
#include "core/sound_bank.h"

#include "test_support.h"

#include <map>
#include <string>

namespace {

using namespace tl::core;

/// A stand-in for one file of a `BASEFILE` chain.
struct Authored {
    std::map<std::string, std::string> keys;
    std::string operator()(const char* key) const {
        const auto found = keys.find(key);
        return found == keys.end() ? std::string() : found->second;
    }
};

}  // namespace

/// The bank is the length the original's is, and every slot but the last has a
/// key. `CCharacter::loadModel` loops `i < 0x43`.
TL_TEST(sound_bank_has_the_originals_sixty_seven_slots) {
    TL_CHECK_EQ(kSoundSlots, static_cast<size_t>(67));
    for (size_t i = 0; i + 1 < kSoundSlots; ++i) {
        TL_CHECK(kSoundSlotKeys[i][0] != '\0');
    }
    // The last one is `KSOUNDBANK_STRINGS[66]`, initialised to the empty string
    // - the menus fill it by hand.
    TL_CHECK_EQ(std::string(kSoundSlotKeys[static_cast<size_t>(SoundSlot::MenuClose)]),
                std::string(""));
}

/// The slots the engine names by hand are where the original puts them.
TL_TEST(sound_bank_slot_numbers_match_the_disassembly) {
    TL_CHECK_EQ(static_cast<int>(SoundSlot::Step), 0);
    TL_CHECK_EQ(static_cast<int>(SoundSlot::Attack), 10);  // CCharacter::attack, 0x1a is Effort
    TL_CHECK_EQ(static_cast<int>(SoundSlot::Idle), 11);    // idleAI's fidget bark
    TL_CHECK_EQ(static_cast<int>(SoundSlot::Roar), 13);    // idleAINormal's aggro bark
    TL_CHECK_EQ(static_cast<int>(SoundSlot::Effort), 26);
    TL_CHECK_EQ(static_cast<int>(SoundSlot::VoicePetInventoryFull), 42);
    TL_CHECK_EQ(static_cast<int>(SoundSlot::VoicePetDeparted), 43);
    TL_CHECK_EQ(static_cast<int>(SoundSlot::VoicePetReturned), 44);  // queueGlobalSample(0x2c)
    TL_CHECK_EQ(static_cast<int>(SoundSlot::VoicePetFled), 45);
    TL_CHECK_EQ(static_cast<int>(SoundSlot::VoiceHealthLow), 52);
    TL_CHECK_EQ(static_cast<int>(SoundSlot::VoiceLevelUp), 60);
    TL_CHECK_EQ(static_cast<int>(SoundSlot::VoiceFameUp), 61);
    TL_CHECK_EQ(static_cast<int>(SoundSlot::MenuClose), 66);
}

/// Two keys are misspelled in the original and kept: `VOICESPELLLEARNED_SOUND` (the data
/// spells two Ls, so `NarratorNewSpell` never plays).
TL_TEST(sound_bank_keeps_the_originals_two_misspellings) {
    TL_CHECK_EQ(std::string(kSoundSlotKeys[static_cast<size_t>(SoundSlot::PortalExit)]),
                std::string("POERTALEXIT_SOUND"));
    TL_CHECK_EQ(std::string(kSoundSlotKeys[static_cast<size_t>(SoundSlot::VoiceSpellLearned)]),
                std::string("VOICESPELLLEARNED_SOUND"));
}

/// A nearer file of the `BASEFILE` chain wins, and a file that says nothing
/// about a slot leaves whatever an earlier one put there.
TL_TEST(sound_bank_inherits_first_writer_wins) {
    SoundBank bank;
    const Authored dog{{{"ATTACK_SOUND", "DogAttack"}, {"STRIKE_SOUND", "DogBite"}}};
    const Authored base{{{"ATTACK_SOUND", "MonsterAttack"}, {"STEP_SOUND", "FootstepGeneric"}}};

    bank.inherit(dog);
    bank.inherit(base);

    TL_CHECK_EQ(bank[SoundSlot::Attack], std::string("DogAttack"));
    TL_CHECK_EQ(bank[SoundSlot::Strike], std::string("DogBite"));
    TL_CHECK_EQ(bank[SoundSlot::Step], std::string("FootstepGeneric"));
    TL_CHECK(!bank.has(SoundSlot::Roar));
    TL_CHECK_EQ(bank.filled(), static_cast<size_t>(3));
}

/// The empty key is never asked for, so nothing an authored file happens to
/// store under "" can reach slot 66.
TL_TEST(sound_bank_never_reads_the_blank_key) {
    SoundBank bank;
    const Authored anything{{{"", "SomethingThatIsNotAMenuClose"}}};
    bank.inherit(anything);
    TL_CHECK(!bank.has(SoundSlot::MenuClose));
    TL_CHECK_EQ(bank.filled(), static_cast<size_t>(0));
}

// `CSoundManager::queueSound` (inlined in `queueGlobalSample`)
// and `CSoundManager::update`'s queue.
namespace {
tl::core::VoiceQueue::Entry voice(int slot, float delay) {
    tl::core::VoiceQueue::Entry e;
    e.slot = slot;
    e.sample = "S" + std::to_string(slot);
    e.volume = 1.0f;
    e.delay = delay;
    return e;
}
}  // namespace

TL_TEST(voice_queue_keeps_one_line_per_slot) {
    tl::core::VoiceQueue q;
    TL_CHECK(q.queue(voice(31, 1.0f), false));
    TL_CHECK(!q.queue(voice(31, 0.0f), false));  // already queued
    TL_CHECK(q.queue(voice(44, 0.1f), false));
    TL_CHECK_EQ(q.size(), 2u);
}

TL_TEST(voice_queue_plays_after_the_delay_then_waits_for_the_channel) {
    tl::core::VoiceQueue q;
    q.queue(voice(31, 1.0f), false);
    q.queue(voice(44, 0.1f), false);
    tl::core::VoiceQueue::Entry out;
    TL_CHECK(!q.update(0.5f, false, out));
    TL_CHECK(q.update(0.5f, false, out));  // 1.0 waited: the first plays
    TL_CHECK_EQ(out.slot, 31);
    TL_CHECK_EQ(q.playing_slot(), 31);
    TL_CHECK_EQ(q.waited(), 0.0f);
    q.set_channel("voice:s31");
    // While it sounds, the next waits and its delay does not run.
    TL_CHECK(!q.update(5.0f, true, out));
    TL_CHECK_EQ(q.waited(), 0.0f);
    // The slot sounding is refused; once silent it may be queued again.
    TL_CHECK(!q.queue(voice(31, 0.0f), true));
    TL_CHECK(!q.update(0.05f, false, out));
    TL_CHECK(q.update(0.05f, false, out));
    TL_CHECK_EQ(out.slot, 44);
    // Empty and silent: the channel is forgotten.
    q.set_channel("voice:s44");
    TL_CHECK(!q.update(0.1f, true, out));
    TL_CHECK(q.channel() == "voice:s44");
    TL_CHECK(!q.update(0.1f, false, out));
    TL_CHECK(q.channel().empty());
    TL_CHECK(q.queue(voice(31, 0.0f), false));
}
