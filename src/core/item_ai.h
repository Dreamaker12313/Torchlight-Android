// A character's target item: walking to it and using it (`CCharacter::getItemAI`,
// `inInteractionRange`), and the console's second pick of one.
// `getItemAI`, `inInteractionRange`,
// `facingTarget`, the selector.
// No OGRE: the caller measures, tests sight lines and acts.
#pragma once

#include "core/collision.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tl::core {

/// `inInteractionRange`'s reach between the two bodies, and a `FISHING HOLE`'s standing and
/// walking.
inline constexpr float kInteractionRange = 1.75f;
inline constexpr float kFishingRangeStanding = 3.45f;
inline constexpr float kFishingRangeWalking = 2.95f;

/// The ground-plane distance `getItemAI` and `inInteractionRange` measure (x and
/// z; the y term is the constant 0.0).
float item_ground_distance(const Vec3& from, const Vec3& to);

/// `CCharacter::inInteractionRange()`: the
/// ground distance less the character's and the item's `+0x194` radii, within
/// the reach. `walking` is the character's `+0x264` (a route to follow).
bool in_interaction_range(float distance, float character_radius, float item_radius,
                          bool fishing_hole, bool walking);

/// The target item (`+0x350`) as `getItemAI` asks about it.
struct ItemAITarget {
    Vec3 at;                      ///< `getPosition(true)`
    float radius = 0.3f;          ///< `+0x194`
    bool container = false;       ///< `ISA 0x1e`
    bool breakable = false;       ///< `ISA 0x1d`
    bool interactable = false;    ///< `ISA 0x20`
    bool fishing_hole = false;    ///< `ISA 0x7a`
    bool enchant_shrine = false;  ///< `ISA 0x9d`
    bool stat_shrine = false;     ///< `ISA 0x9e`
    bool gem_shrine = false;      ///< `ISA 0x9f`
};

/// The character as `getItemAI` asks about it.
struct ItemAICharacter {
    Vec3 at;               ///< `getPosition(true)`
    float radius = 0.3f;   ///< `+0x194`
    bool walking = false;  ///< `+0x264`
    /// `validLineOfSight(level, true)`
    bool line_of_sight = true;
    bool facing = true;            ///< `facingTarget()`
    bool in_attack_range = false;  ///< `inAttackRange(0)`
};

/// What `getItemAI` does this frame.
enum class ItemAIStep {
    /// In reach, not an interactable: `setAIState(0)`, `getItem(item)`.
    ///
    Take,
    /// In reach and sight, faced: `setAIState(0)`, the item's `interact`
    /// (vtable +0x280).
    Interact,
    /// The same, a `FISHING HOLE`: vtable +0x338 (`startFishing`).
    Fish,
    /// The same, an `ENCHANTSHRINE`/`STATSHRINE`/`GEMSHRINE`: AI state 0x16,
    /// 0x17, 0x18.
    EnchantShrine,
    StatShrine,
    GemShrine,
    /// A breakable container in attack range and sight, faced: `attack()`, then
    /// `setAIState(0)`.
    Attack,
    /// In reach (and sight): stopped but not facing it yet - the turn is `updateMotion`'s.
    Turn,
    /// Out of reach and not walking: `setDestination(item)` and vtable +0x268.
    /// The caller gives up (as `GiveUp`) when no route results.
    Walk,
    /// Out of reach and walking: nothing this frame.
    Wait,
    /// Neither in reach nor farther than the character's own radius, or no
    /// route: `setTargetItem(nullptr)`, `setAIState(2)`.
    GiveUp,
};

/// `CCharacter::getItemAI(float, CLevel&)`
/// with a target item; with none it is `setAIState(0)`, which the caller does
/// without asking.
ItemAIStep item_ai(const ItemAITarget& item, const ItemAICharacter& character);

/// One candidate of the console's second pick: a level item that passed the pick's filters,
/// in the character's own frame.
struct SecondPickCandidate {
    float local_x = 0.0f;       ///< across the facing
    float local_z = 0.0f;       ///< along the facing
    bool fishing_hole = false;  ///< `ISA 0x7a`
    /// The sight line from 5 above the character to 5 above the item is clear.
    bool in_sight = true;
};

/// the selector's pick when neither of its passes found
/// anything. The index of the chosen candidate, or -1; the nearest wins, the first on a tie.
int second_pick(const std::vector<SecondPickCandidate>& candidates);

}  // namespace tl::core
