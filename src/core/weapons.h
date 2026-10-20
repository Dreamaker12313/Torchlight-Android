// Which swing a weapon makes: `UNITTYPE` to animation class, the classes being Core.dll's list.
// Read off the type hierarchy and the shipped monsters' skeletons; a staff swings
// on the polearm animation. No OGRE and no file I/O.
#pragma once

#include <string>

namespace tl::core {

/// Which hand an item is held in; a one-handed weapon can be in either, so the caller says.
enum class Hand { Right, Left };

/// The animation-class stem for a weapon's `UNITTYPE` ("Polearm" -> `Polearm1.SKELETON`); an
/// empty or unrecognised type is bare hands, `Attack`.
std::string attack_animation_class(const std::string& unittype, Hand hand);

/// Which of the stats screen's three damage rows a weapon's damage is counted on: its
/// `ITEMCATEGORYMARTIAL`, `ITEMCATEGORYRANGED` or `ITEMCATEGORYMAGIC` ancestor in the hierarchy.
enum class WeaponDamageKind { None, Melee, Ranged, Magic };
WeaponDamageKind weapon_damage_kind(const std::string& unittype);

/// Which hand the hierarchy puts this weapon in when nothing else says: only `BOW` is
/// `LEFT HANDED`.
Hand default_hand(const std::string& unittype);

/// Frames per second of a `.animation` manifest's frame numbers (`HIT`, `ATTACKSOUND`,
/// `FOOTSTEP`); OGRE times an animation in seconds.
inline constexpr float kAnimationFramesPerSecond = 30.0f;

}  // namespace tl::core
