// The 67-slot `CSoundBank` every unit carries, filled by `CCharacter::loadModel`
// from the keys of `KSOUNDBANK_STRINGS` (`.bss): the engine plays slot numbers.
// `playSample` is positional; `CSoundBank::queueGlobalSample`
// is not, takes a volume (0 = authored) and a delay, and rolls `PLAYCHANCE` before queueing.
#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace tl::core {

/// The 67 slots of a `CSoundBank`, in the order `KSOUNDBANK_STRINGS` lists
/// them. The comment on each is the authored key; `kSoundSlotKeys` is the
/// machine-readable copy.
enum class SoundSlot : int {
    // ---- what a body does ------------------------------------------------
    Step = 0,     ///< `STEP_SOUND`
    Strike,       ///< `STRIKE_SOUND` - the blow landing
    PhysicalHit,  ///< `PHYSICALHIT_SOUND`
    IceHit,       ///< `ICEHIT_SOUND`
    FireHit,      ///< `FIREHIT_SOUND`
    ElectricHit,  ///< `ELECTRICHIT_SOUND`
    PoisonHit,    ///< `POISONHIT_SOUND`
    ShieldBlock,  ///< `SHIELDBLOCK` - no `_SOUND` suffix, in the original
    Block,        ///< `BLOCK` - likewise
    Critical,     ///< `CRITICAL_SOUND`
    Attack,       ///< `ATTACK_SOUND` - the swing
    Idle,         ///< `IDLE_SOUND` - the fidget bark, `idleAI`
    Death,        ///< `DEATH_SOUND`
    Roar,         ///< `ROAR_SOUND` - the aggro bark, `idleAINormal`
    Flee,         ///< `FLEE_SOUND`
    Miss,         ///< `MISS_SOUND`
    Fall,         ///< `FALL_SOUND`
    Land,         ///< `LAND_SOUND`
    Take,         ///< `TAKE_SOUND`
    Spawn,        ///< `SPAWN_SOUND`
    Use,          ///< `USE_SOUND`
    Equip,        ///< `EQUIP_SOUND`

    // ---- what the interface does -----------------------------------------
    OpenMenu,        ///< `OPENMENU_SOUND`
    Buy,             ///< `BUY_SOUND`
    Error,           ///< `ERROR_SOUND`
    LevelUp,         ///< `LEVELUP_SOUND` - the chime, not the narrator
    Effort,          ///< `EFFORT_SOUND` - the grunt, `CCharacter::attack()`
    SpendPoint,      ///< `SPENDPOINT_SOUND`
    PortalEnter,     ///< `PORTALENTER_SOUND`
    PortalExit,      ///< `POERTALEXIT_SOUND` - the engine's own typo; see below
    SkillAssign,     ///< `SKILLASSIGN_SOUND`
    QuestReceived,   ///< `QUESTRECEIVED_SOUND`
    QuestCompleted,  ///< `QUESTCOMPLETED_SOUND`
    LowMana,         ///< `LOWMANA_SOUND`
    MissileReflect,  ///< `MISSILEREFLECT_SOUND`
    BadEffect,       ///< `BADEFFECT_SOUND`
    GoodEffect,      ///< `GOODEFFECT_SOUND`
    NpcDialog,       ///< `NPCDIALOG_SOUND`
    NpcGreet,        ///< `NPCGREET`
    NpcBye,          ///< `NPCBYE`
    NpcPurchase,     ///< `NPCPURCHASE`
    NpcSell,         ///< `NPCSELL`

    // ---- the voice bank: slots 42 to 65, all through `queueGlobalSample` -----
    // (`SoundSlot::VoicePetReturned` and its neighbours)
    VoicePetInventoryFull,  ///< `VOICEPETINVENTORYFULL_SOUND`, `CCharacter::getItem`
    /// `VOICEPETDEPARTED_SOUND`, `CGameUI::onClick` - the button, not the vanishing
    VoicePetDeparted,
    VoicePetReturned,       ///< `VOICEPETRETURNED_SOUND`, `CCharacter::updateInTownTimer`
    VoicePetFled,           ///< `VOICEPETFLED_SOUND`, `CCharacter::modifyHP`
    VoicePetLevelUp,        ///< `VOICEPETLEVELUP_SOUND`, `CCharacter::levelUp`
    VoiceInventoryFull,     ///< `VOICEINVENTORYFULL_SOUND`
    VoiceInsufficientGold,  ///< `VOICEINSUFFICIENTGOLD_SOUND`
    VoiceImpossible,        ///< `VOICEIMPOSSIBLE_SOUND`
    VoiceCantCast,          ///< `VOICECANTCAST_SOUND`
    VoiceCantCastHere,      ///< `VOICECANTCASTHERE_SOUND`
    VoiceHealthLow,         ///< `VOICEHEALTHLOW_SOUND`, `CCharacter::modifyHP`
    VoiceManaLow,           ///< `VOICEMANALOW_SOUND`
    VoiceTrapSprung,        ///< `VOICETRAPSPRUNG_SOUND`
    VoiceRefreshed,         ///< `VOICEREFRESHED_SOUND`
    VoicePowerful,          ///< `VOICEPOWERFUL_SOUND`
    VoiceHealthy,           ///< `VOICEHEALTHY_SOUND`
    VoiceIll,               ///< `VOICEILL_SOUND`
    VoicePoisoned,          ///< `VOICEPOISONED_SOUND`
    VoiceLevelUp,           ///< `VOICELEVELUP_SOUND`, `CPlayer::levelUp`
    VoiceFameUp,            ///< `VOICEFAMEUP_SOUND`, `CCharacter::fameUp`
    VoiceSkillUp,           ///< `VOICESKILLUP_SOUND`
    VoiceFortuneGood,       ///< `VOICEFORTUNEGOOD_SOUND`, the enchanter
    VoiceFortuneBad,        ///< `VOICEFORTUNEBAD_SOUND`, likewise
    VoiceSpellLearned,      ///< `VOICESPELLLEARNED_SOUND`; see the note below

    /// No authored key (`KSOUNDBANK_STRINGS[66]` is empty): menus fill it by hand.
    MenuClose,

    Count,
};

inline constexpr size_t kSoundSlots = static_cast<size_t>(SoundSlot::Count);
static_assert(kSoundSlots == 67, "the original's bank is 0x43 slots");

/// The authored key for each slot - `KSOUNDBANK_STRINGS`, transcribed. Two are misspelled in
/// the original and kept: slot 29 `POERTALEXIT_SOUND`, and slot 65 `VOICESPELLLEARNED_SOUND`
/// (the class files author `VOICESPELLEARNED_SOUND`, so `NarratorNewSpell` never plays).
extern const std::array<const char*, kSoundSlots> kSoundSlotKeys;

/// `VOICEHEALTHLOW_SOUND` fires when health crosses `maxHP() * 0.2` downwards, not while
/// below it (`CCharacter::modifyHP`); the delay is passed.
inline constexpr float kVoiceHealthLowFraction = 0.2f;
inline constexpr float kVoiceHealthLowDelay = 0.5f;

/// What one unit's `.DAT` chain named for each slot, by sound *definition*
/// name - `DogAttack`, `FootstepDog`, `NarratorPetReturned`. Empty where the
/// unit authors nothing, which is most of them.
class SoundBank {
public:
    // PORT: an accessor of the port's own bookkeeping.
    const std::string& operator[](SoundSlot slot) const {
        return names_[static_cast<size_t>(slot)];
    }

    // PORT: an accessor of a unit's sound slots.
    bool has(SoundSlot slot) const { return !(*this)[slot].empty(); }

    void set(SoundSlot slot, std::string name) {
        names_[static_cast<size_t>(slot)] = std::move(name);
    }

    /// Fill every slot this bank has not got yet from one file of the `BASEFILE` chain;
    /// `value_of(key)` returns what it authors, or empty. First writer (concrete file) wins.
    template <typename ValueOf>
    void inherit(const ValueOf& value_of) {
        for (size_t i = 0; i < kSoundSlots; ++i) {
            if (!names_[i].empty()) continue;
            if (kSoundSlotKeys[i][0] == '\0') continue;
            names_[i] = value_of(kSoundSlotKeys[i]);
        }
    }

    /// How many slots are filled, for the log.
    size_t filled() const;

private:
    std::array<std::string, kSoundSlots> names_;
};

/// `CSoundManager`'s queue of global samples, played one at a time, each after its delay;
/// starts empty (`CSoundManager::CSoundManager`). `channel`
/// is the port's: the key of the voice started last; the caller says whether it still sounds.
class VoiceQueue {
public:
    struct Entry {
        int slot = -1;
        std::string sample;  ///< the sound definition
        std::string file;    ///< the recording `queueGlobalSample` drew
        float volume = 0.0f;
        float delay = 0.0f;
    };

    /// `CSoundManager::queueSound(slot, sample, volume, delay)` (X360 inline):
    /// refused while the slot plays or is already queued.
    bool queue(Entry entry, bool channel_playing);

    /// `CSoundManager::update(listener, position, delta)`,
    /// its queue: once the channel is silent and the first entry's delay has passed, that
    /// entry comes back to be played. Returns whether there is one.
    bool update(float delta, bool channel_playing, Entry& out);

    /// The channel the caller started for the entry `update` returned.
    // PORT: an accessor of the voice queue.
    void set_channel(std::string key) { channel_ = std::move(key); }
    // PORT: an accessor of the voice queue.
    const std::string& channel() const { return channel_; }
    int playing_slot() const { return playing_slot_; }
    size_t size() const { return queue_.size(); }
    float waited() const { return waited_; }
    void clear();

private:
    std::vector<Entry> queue_;
    int playing_slot_ = -1;  ///< X360 +0x20, Linux +0x2c
    std::string channel_;    ///< X360 +0x398, Linux +0x6f0: -1 is empty
    float waited_ = 0.0f;    ///< X360 +0x39c, Linux +0x6f4
};

/// The interface's own sounds: banks the menus, `CGameUI` and `CQuestManager` fill by hand
/// (`CSoundBank::addSample(slot, guid)`), naming definitions directly. Each row: the
/// definition, the constructor that binds it, the slot. The master menu takes the
/// `InventoryOpen`/`InventoryClose` pair.
enum class InterfaceSound {
    MenuOpen,         ///< `InventoryOpen`  - CInventoryMenu, slot 22
    MenuClose,        ///< `InventoryClose` - CInventoryMenu, slot 66
    PointAssign,      ///< `PointAssign`    - CStatsMenu, slot 27
    Error,            ///< `Error`          - CGameUI::create, slot 24
    QuestReceived,    ///< `QuestReceived`  - CQuestManager, slot 31
    QuestCompleted,   ///< `QuestCompleted` - CQuestManager, slot 32
    QuestAbandoned,   ///< `LowMana`        - CQuestMenu, slot 35 (sic)
    GoldBuy,          ///< `GoldBuy`        - CGameUI::create, slot 23
    Reveal,           ///< `Reveal`         - CGameUI::create, slot 36
    LevelUp,          ///< `Levelup`        - CPlayer::unitInit, slot 25
    ItemTake,         ///< `WeaponSwap`     - CInventoryMenu, slot 18
    GoldPickup,       ///< `GoldPickup`     - CItemGold, slot 18
    TownPortalEnter,  ///< `TOWNPORTALENTER` - CGameClient::create, slot 0x1c
    SkillAssign,      ///< `ASSIGNSKILL`    - X360 CMasterHousingXenon, slot 0x1e
    Navigate,         ///< `NAVIGATE`       - X360 CMasterHousingXenon, slot 0x43
    /// `LowMana`        - CEnchantMenu, slot 0x23: nothing happens or all is lost
    Disenchant,
    Count,
};

/// The definition each names, as the sound tables spell it (matched without case).
inline constexpr const char* kInterfaceSoundNames[] = {
    "InventoryOpen",   "InventoryClose", "PointAssign", "Error",
    "QuestReceived",   "QuestCompleted", "LowMana",     "GoldBuy",
    "Reveal",          "Levelup",        "WeaponSwap",  "GoldPickup",
    "TOWNPORTALENTER", "AssignSkill",    "Navigate",    "LowMana",
};
static_assert(sizeof(kInterfaceSoundNames) / sizeof(kInterfaceSoundNames[0]) ==
                  static_cast<size_t>(InterfaceSound::Count),
              "the interface sound table and its enum have drifted apart");

// PORT: an interface sound's name in the table.
inline const char* interface_sound_name(InterfaceSound sound) {
    return kInterfaceSoundNames[static_cast<size_t>(sound)];
}

}  // namespace tl::core
