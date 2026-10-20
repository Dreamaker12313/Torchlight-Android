// AI flags (CAIFlag, CAIFlagManager, CAIManager): seven timed countdown flags per
// thinking character, ticked from CCharacter::update. Side effects go through
// owner-provided hooks.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace tl::core {

/// `EAIFLAG_TYPES`, in `g_AIFLAG_TYPE_NAMES`'s order.
enum class AIFlag : uint32_t {
    Aware = 0,              ///< doubles sight range; spreads across formation
    Berserk = 1,            ///< turns against friends; sets state and alignment
    CannotInterrupt = 2,    ///< prevents hit interrupts
    Frighten = 3,           ///< halves bravery
    NoLineOfSight = 4,      ///< line of sight checks pass (validLineOfSight returns true)
    NeverChangeTarget = 5,  ///< prevents switching target while target is held
    CannotTarget = 6,       ///< prevents being targeted
};

inline constexpr size_t kAIFlagCount = 7;

/// Side effect hooks supplied by the owning character/level.
struct AIFlagsHooks {
    /// CAIFlagManager::flagSet type 0 (AWARE): spread AWARE to a formation peer.
    std::function<void(int64_t target_id, float duration)> on_spread_aware;
    /// CAIFlagManager::flagSet type 1 (BERSERK): berserk started.
    std::function<void()> on_berserk_start;
    /// CAIFlagManager::flagRemoved type 1 (BERSERK): berserk ended.
    std::function<void()> on_berserk_end;
};

/// Transcribed AI flag manager (CAIFlagManager;
/// CAIFlag; and CAIManager flag forwarders / formation list).
class AIFlags {
public:
    // CAIFlagManager::CAIFlagManager and
    // CAIFlag::CAIFlag
    explicit AIFlags(AIFlagsHooks hooks = {});

    void set_hooks(AIFlagsHooks hooks) { hooks_ = std::move(hooks); }

    // CAIFlagManager::hasAIFlag / CAIManager::hasAIFlag
    bool has(AIFlag flag) const;

    // CAIFlagManager::addAIFlag / CAIManager::addAIFlag
    void add(AIFlag flag, float duration);

    // CAIFlagManager::removeAIFlag / CAIManager::removeAIFlag
    void remove(AIFlag flag);

    // CAIFlagManager::updateAIFlags / CAIFlag::update
    void update(float dt);

    // CAIFlagManager::updateAIFlag — an empty no-op in the original.
    void update_ai_flag(AIFlag flag, float dt);

    // CAIManager::addFormationUnit
    void add_formation_unit(int64_t id);

    /// Read access to remaining time in seconds for a flag (for inspection and tests).
    float time(AIFlag flag) const;

    const std::vector<int64_t>& formation_units() const { return formation_units_; }

private:
    // CAIFlagManager::flagSet
    void flag_set(AIFlag flag);

    // CAIFlagManager::flagRemoved
    void flag_removed(AIFlag flag);

    std::array<float, kAIFlagCount> times_{};
    std::vector<int64_t> formation_units_;
    AIFlagsHooks hooks_;
};

}  // namespace tl::core
