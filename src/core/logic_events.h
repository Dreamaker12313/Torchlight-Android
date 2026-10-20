// The layout script's event vocabulary, as the original numbers it: `EINPUT_EVENTS` and
// `EOUTPUT_EVENTS`, named by `gINPUT_EVENT_NAMES` / `gOUTPUT_EVENTS_NAMES`. A
// descriptor accepts some of them in the order `CDescriptor::AddInputLogic`
// and `AddOutputLogic` register them; a
// compiled timeline event key stores the position in that list
// (`CTimelineProperty::triggerEvent`). Names repeat across descriptors, so a wire's
// name only means something against its descriptor's list.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace tl::core::logic {

/// `EINPUT_EVENTS`.
enum In : uint32_t {
    kShow = 0x00,
    kHide = 0x01,
    kEnable = 0x02,
    kDisable = 0x03,
    kEnableAndShow = 0x04,
    kDisableAndHide = 0x05,
    kReset = 0x06,
    kAdd = 0x07,
    kSubtract = 0x08,
    kPlay = 0x09,
    kStop = 0x0a,
    kPause = 0x0b,
    kResume = 0x0c,
    kPlayLevelMusic = 0x0d,
    kIncrement = 0x0e,
    kActivate = 0x0f,
    kStartParticle = 0x10,
    kStopParticle = 0x11,
    kForceStopParticle = 0x12,
    kPauseParticle = 0x13,
    kResumeParticle = 0x14,
    kTimelinePlay = 0x15,
    kTimelinePlayBackwards = 0x16,
    kTimelineStop = 0x17,
    kTimelineStopToEnd = 0x18,
    kTimelinePause = 0x19,
    kTimelineReset = 0x1a,
    kTimelineResetToEnd = 0x1b,
    kTimelineFastForwardToEnd = 0x1c,
    kTimelineRewindToStart = 0x1d,
    kSetStateOne = 0x1e,
    kSetStateTwo = 0x1f,
    kSpawnUnits = 0x20,
    kDestroySpawnedUnits = 0x21,
    kHideAndDisableSpawnedUnits = 0x22,
    kIncrementLevelDelta = 0x23,
    kDecrementLevelDelta = 0x24,
    kActivateWarper = 0x25,
    kActivateTeleport = 0x26,
    kRoll = 0x27,
    kPuzzleInput1 = 0x28,
    kTrigger = 0x2e,
    kToggle = 0x2f,
    kInteract = 0x30,
    kMakeInvulnerable = 0x31,
    kMakeVulnerable = 0x32,
    kStartCamera = 0x33,
    kCameraOff = 0x34,
    kForceAccept = 0x35,
    kForceNotAccepted = 0x36,
    kForceComplete = 0x37,
    kForceNotComplete = 0x38,
    kStartSkill = 0x39,
    kStopSkill = 0x3a,
    kLearnSkill = 0x3b,
    kUnlearnSkill = 0x3c,
    kAlertMonster = 0x3d,
    kEnableTargeting = 0x3e,
    kDisableTargeting = 0x3f,
    kEnableTargetingAndAlert = 0x40,
    kHunt = 0x41,
    kAnimationPlay = 0x42,
    kAnimationPlayLooping = 0x43,
    kAnimationStop = 0x44,
    kAnimationStopAndIdle = 0x45,
    kCannotBeTargeted = 0x46,
    kCanBeTargeted = 0x47,
    kAddAsPet = 0x48,
    kRemoveAsPet = 0x49,
    kKillMonster = 0x4a,
    kWarpPetsToPlayer = 0x4b,
    kHealPlayer = 0x4c,
    kCollidable = 0x4d,
    kNotCollidable = 0x4e,
    kTakeMoney = 0x4f,
    kShowTip = 0x50,
    kClearHistory = 0x51,
    kStopSkills = 0x52,
    kKillPets = 0x53,
    kGroupStop = 0x54,
    kGroupStart = 0x55,
    kGroupPause = 0x56,
    kGroupInput1 = 0x57,
    kInputCount = 0x5c,
};

/// `EOUTPUT_EVENTS`.
enum Out : uint32_t {
    kTriggered = 0x00,
    kTriggeredFirstTime = 0x01,
    kDeactivated = 0x02,
    kDeactivatedFirstTime = 0x03,
    kOnVisible = 0x04,
    kOnInvisible = 0x05,
    kEnabled = 0x06,
    kDisabled = 0x07,
    kActivated = 0x08,
    kWasReset = 0x09,
    kInitialized = 0x0a,
    kPlaying = 0x0b,
    kSoundStopped = 0x0c,
    kSoundEnded = 0x0d,
    kSoundPaused = 0x0e,
    kSoundResumed = 0x0f,
    kIncremented = 0x10,
    kFirstIncrement = 0x11,
    kMonstersSpawned = 0x17,
    kMonsterKilled = 0x18,
    kAllMonstersDead = 0x19,
    kAllUnitsSpawned = 0x1a,
    kItemPickedUp = 0x1b,
    kAllItemsPickedUp = 0x1c,
    kItemInteracted = 0x1d,
    kAllItemsInteractedWith = 0x1e,
    kParticleStarted = 0x1f,
    kParticleStopped = 0x20,
    kParticlePaused = 0x21,
    kStopped = 0x23,
    kStarted = 0x24,
    kPaused = 0x25,
    kResetToBeginning = 0x26,
    kResetToEnd = 0x27,
    kLooped = 0x28,
    kStartedBackwards = 0x29,
    kStartedForwards = 0x2a,
    kStoppedBackwards = 0x2b,
    kStoppedForwards = 0x2c,
    kFinished = 0x2d,
    kStateOne = 0x2e,
    kStateTwo = 0x2f,
    kActivationFailed = 0x30,
    kOne = 0x31,
    kFailed = 0x36,
    kSuccess = 0x37,
    kInteractedWithUnit = 0x38,
    kHp90Pct = 0x39,
    kHp80Pct = 0x3a,
    kHp70Pct = 0x3b,
    kHp60Pct = 0x3c,
    kHp50Pct = 0x3d,
    kHp40Pct = 0x3e,
    kHp30Pct = 0x3f,
    kHp20Pct = 0x40,
    kHp10Pct = 0x41,
    kMonsterAlerted = 0x42,
    kPlayerHpBelow90Pct = 0x43,
    kPlayerHpBelow80Pct = 0x44,
    kPlayerHpBelow70Pct = 0x45,
    kPlayerHpBelow60Pct = 0x46,
    kPlayerHpBelow50Pct = 0x47,
    kPlayerHpBelow40Pct = 0x48,
    kPlayerHpBelow30Pct = 0x49,
    kPlayerHpBelow20Pct = 0x4a,
    kPlayerHpBelow10Pct = 0x4b,
    kPlayerHpAbove90Pct = 0x4c,
    kPlayerHpAbove80Pct = 0x4d,
    kPlayerHpAbove70Pct = 0x4e,
    kPlayerHpAbove60Pct = 0x4f,
    kPlayerHpAbove50Pct = 0x50,
    kPlayerHpAbove40Pct = 0x51,
    kPlayerHpAbove30Pct = 0x52,
    kPlayerHpAbove20Pct = 0x53,
    kPlayerHpAbove10Pct = 0x54,
    kAccepted = 0x55,
    kDeclined = 0x56,
    kCameraMoving = 0x57,
    kCameraStopped = 0x58,
    kCameraPausing = 0x59,
    kCameraControlRestored = 0x5a,
    kInteracting = 0x5b,
    kInteracted = 0x5c,
    kInteractedAccepted = 0x5d,
    kInteractedDeclined = 0x5e,
    kInteractedClosed = 0x5f,
    kInvulnerable = 0x60,
    kVulnerable = 0x61,
    kQuestActive = 0x62,
    kQuestNotActive = 0x63,
    kQuestComplete = 0x64,
    kQuestNotComplete = 0x65,
    kQuestAbandoned = 0x66,
    kSkillStarted = 0x67,
    kSkillStopped = 0x68,
    kSkillLearned = 0x69,
    kSkillUnlearned = 0x6a,
    kEndOfPathReached = 0x6e,
    kClicked = 0x6f,
    kAnimationStopped = 0x70,
    kAnimationPlaying = 0x71,
    kSkipCutscene = 0x72,
    kLevelActivated = 0x73,
    kGroupStopOut = 0x76,
    kOutputCount = 0x7e,
};

/// The name the original gives an input or output, or "?" out of range.
const char* input_name(uint32_t id);
const char* output_name(uint32_t id);

/// One descriptor's logic vocabulary, in registration order.
struct Vocabulary {
    const char* descriptor = "?";  ///< the descriptor's name, as the layout spells it
    const uint32_t* inputs = nullptr;
    size_t input_count = 0;
    const uint32_t* outputs = nullptr;
    size_t output_count = 0;

    /// The input id a wire's name means on this descriptor: the first wrapper with
    /// that name, which is what `CDescriptor::GetInputLogicWrapper` finds walking the
    /// list. `kNone` when the descriptor has no such input.
    uint32_t input_by_name(const std::string& name) const;
    uint32_t output_by_name(const std::string& name) const;

    /// The input id at a compiled timeline event key's index, or `kNone`.
    uint32_t input_at(size_t index) const;
    uint32_t output_at(size_t index) const;
};

inline constexpr uint32_t kNone = 0xffffffffu;

/// The vocabulary of a compiled node type (`formats::cmp_type`), or an empty one
/// named "?" for a type this table does not know.
const Vocabulary& vocabulary(uint32_t node_type);

}  // namespace tl::core::logic
