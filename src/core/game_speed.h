// Game-speed modifiers: temporary speed
// scaling (a slow-down on a killing blow, a skill's slow) averaged into one
// frame-delta multiplier. Curves are passed in already built as StatGraphs.
#pragma once

#include "core/stats.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tl::core {

/// Speed modifier curve type matching EGAMESPEED_TYPE in the original binary.
/// Named Decelerate (0) because type 0 reduces speed (1 - curve * strength)
/// and Accelerate (1) because type 1 increases speed (1 + curve * strength).
enum class GameSpeedType : int32_t {
    Decelerate = 0,
    Accelerate = 1,
};

/// One active speed modifier request (`CSpeedInstance`, 0x30 bytes).
struct SpeedInstance {
    int32_t type = 0;
    float duration = 0.0f;
    float elapsed = 0.0f;
    const StatGraph* curve = nullptr;
    float strength = 0.0f;
};

/// Speed modifier manager (`CGameSpeed`, Linux binary).
class GameSpeed {
public:
    // CGameSpeed::CGameSpeed — initialize
    // speed factor to 1.0f and store curve pointers
    explicit GameSpeed(const StatGraph* type0_curve = nullptr,
                       const StatGraph* type1_curve = nullptr);

    // CGameSpeed::addSpeedModifier — enqueue
    // a new speed modifier instance
    void add_modifier(int32_t type, float duration, float strength);

    // CGameSpeed::calculateGameSpeed: age
    // active modifiers, prune expired, and work the speed out again.
    void calculate(float dt);

    // CGameSpeed::getGameSpeed — return current speed multiplier
    float speed() const;

    // CGameSpeed::clear — remove all active speed modifiers
    void clear();

    // PORT: an accessor.
    size_t size() const { return instances_.size(); }
    const std::vector<SpeedInstance>& instances() const { return instances_; }

private:
    float factor_ = 1.0f;
    const StatGraph* type0_curve_ = nullptr;
    const StatGraph* type1_curve_ = nullptr;
    std::vector<SpeedInstance> instances_;
};

}  // namespace tl::core
