#include "core/item_ai.h"
#include "core/runic_math.h"

#include <cmath>

namespace tl::core {

float item_ground_distance(const Vec3& from, const Vec3& to) {
    // `(to.x - from.x)^2 + 0.0`, then the z term.
    const float dx = to.x - from.x;
    const float dz = to.z - from.z;
    return std::sqrt(dx * dx + 0.0f + dz * dz);
}

bool in_interaction_range(float distance, float character_radius, float item_radius,
                          bool fishing_hole, bool walking) {
    // `CCharacter::inInteractionRange()`.
    // `setbe` after `ucomiss`: an unordered compare counts as in reach.
    float reach = kInteractionRange;
    if (fishing_hole) reach = walking ? kFishingRangeWalking : kFishingRangeStanding;
    const float gap = distance - character_radius - item_radius;
    return !(reach < gap);
}

ItemAIStep item_ai(const ItemAITarget& item, const ItemAICharacter& character) {
    // `CCharacter::getItemAI(float, CLevel&)`,
    // with `+0x350` set.
    const float distance = item_ground_distance(character.at, item.at);
    if (item.container) {
        // a breakable container is struck from attack range.
        if (item.breakable && character.in_attack_range && character.line_of_sight) {
            return character.facing ? ItemAIStep::Attack : ItemAIStep::Turn;
        }
    } else if (!item.interactable) {
        // anything else is taken once in reach - no sight, no turn.
        if (in_interaction_range(distance, character.radius, item.radius, item.fishing_hole,
                                 character.walking)) {
            return ItemAIStep::Take;
        }
    } else if (in_interaction_range(distance, character.radius, item.radius, item.fishing_hole,
                                    character.walking) &&
               character.line_of_sight) {
        // stopped where it stands, then the type's use once faced.
        if (!character.facing) return ItemAIStep::Turn;
        if (item.fishing_hole) return ItemAIStep::Fish;
        if (item.enchant_shrine) return ItemAIStep::EnchantShrine;
        if (item.stat_shrine) return ItemAIStep::StatShrine;
        if (item.gem_shrine) return ItemAIStep::GemShrine;
        return ItemAIStep::Interact;
    }
    // `ucomiss +0x194` then `jbe` - farther than its own radius (and
    // not unordered) it walks, unless it already is.
    if (character.radius < distance) {
        return character.walking ? ItemAIStep::Wait : ItemAIStep::Walk;
    }
    return character.walking ? ItemAIStep::Wait : ItemAIStep::GiveUp;
}

int second_pick(const std::vector<SecondPickCandidate>& candidates) {
    // The distance is x*x fused onto z*z; the angle is the console's
    // `atan2(x, z)` in double, rounded. An unordered compare passes.
    constexpr float kDegrees = 57.29582595825195f;
    constexpr float kNone = -1.0f;
    const auto distance_of = [](const SecondPickCandidate& c) {
        return std::sqrt(std::fma(c.local_x, c.local_x, c.local_z * c.local_z));
    };
    int chosen = -1;
    float best = kNone;
    for (size_t i = 0; i < candidates.size(); ++i) {
        const SecondPickCandidate& c = candidates[i];
        float distance = distance_of(c);
        const float angle =
            std::fabs(static_cast<float>(crt_atan2(static_cast<double>(c.local_x),
                                                   static_cast<double>(c.local_z))) *
                      kDegrees);
        bool near = !(angle > 60.0f) && !(distance > 3.0f);
        if (!(distance > 0.5f)) near = true;
        if (c.fishing_hole) {
            distance = distance - 0.8f;
            if (!(angle > 60.0f) && !(distance > 3.0f)) near = true;
        }
        if (!near) continue;
        if (!c.in_sight) distance = distance + 10.0f;
        if (best == kNone || distance < best) {
            chosen = static_cast<int>(i);
            best = distance;
        }
    }
    if (chosen >= 0) return chosen;
    // nothing in front; anything within 2.0.
    for (size_t i = 0; i < candidates.size(); ++i) {
        const SecondPickCandidate& c = candidates[i];
        float distance = distance_of(c);
        if (distance > 2.0f) continue;
        if (!c.in_sight) distance = distance + 10.0f;
        if (best == kNone || distance < best) {
            chosen = static_cast<int>(i);
            best = distance;
        }
    }
    return chosen;
}

}  // namespace tl::core
