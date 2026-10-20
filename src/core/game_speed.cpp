#include "core/game_speed.h"
#include "core/fused.h"

#include <cmath>

namespace tl::core {

// CGameSpeed::CGameSpeed — initialize speed
// factor to 1.0f and store curve pointers
GameSpeed::GameSpeed(const StatGraph* type0_curve, const StatGraph* type1_curve)
    : factor_(1.0f), type0_curve_(type0_curve), type1_curve_(type1_curve) {}

// CGameSpeed::addSpeedModifier — enqueue a
// new speed modifier instance
void GameSpeed::add_modifier(int32_t type, float duration, float strength) {
    SpeedInstance inst;
    inst.type = type;
    inst.duration = duration;
    inst.elapsed = 0.0f;
    inst.curve = (type == 0) ? type0_curve_ : (type == 1) ? type1_curve_ : nullptr;
    inst.strength = strength;
    instances_.push_back(inst);
}

namespace {

// `maxss term, 0` as the original computes it: the zero
// operand wins unless the term is strictly greater, so a NaN or a -0.0 term
// becomes +0.0.
float clamp_non_negative(float term) { return term > 0.0f ? term : 0.0f; }

// `CGraph::getValue(elapsed / duration, 0)`. INVENTED: a null curve reads 0.
// The original's singleton always builds both curves, so it never meets one.
float sample(const SpeedInstance& inst) {
    if (inst.curve == nullptr) return 0.0f;
    return inst.curve->at(inst.elapsed / inst.duration);
}

}  // namespace

// CGameSpeed::calculateGameSpeed: age active
// modifiers, prune expired, and work the speed out again.
void GameSpeed::calculate(float dt) {
    factor_ = 1.0f;
    float divisor = 1.0f;

    size_t i = 0;
    while (i < instances_.size()) {
        SpeedInstance& inst = instances_[i];
        inst.elapsed += dt;

        if (inst.elapsed < inst.duration) {
            divisor += 1.0f;
            // a slowdown's term is `1 - v * strength`
            // in one `fnmsubs`; a speed-up's is added and then 1.
            //
            if (inst.type == 0) {
                factor_ += clamp_non_negative(fnmsub(sample(inst), inst.strength, 1.0f));
            } else if (inst.type == 1) {
                factor_ = (factor_ + clamp_non_negative(sample(inst) * inst.strength)) + 1.0f;
            }
            // Any other type contributes nothing to factor_, while divisor still grew by 1.0f.
            ++i;
        } else {
            // Quirk kept: swap-with-last removal with the
            // index still advancing, so the element moved into slot i waits for
            // the next calculate() call.
            inst = instances_.back();
            instances_.pop_back();
            ++i;
        }
    }

    factor_ = factor_ / divisor;
}

// CGameSpeed::getGameSpeed — return current speed multiplier
float GameSpeed::speed() const { return factor_; }

// CGameSpeed::clear — remove all active speed modifiers
void GameSpeed::clear() { instances_.clear(); }

}  // namespace tl::core
