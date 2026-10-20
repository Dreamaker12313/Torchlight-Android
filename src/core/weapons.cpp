#include "core/weapons.h"

#include <algorithm>
#include <cctype>

namespace tl::core {
namespace {

// PORT: ASCII case folding, the case-blind compare of names in the port's narrow strings.
std::string upper(const std::string& in) {
    std::string out = in;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}

// PORT: whether a unit type's name holds a word.
bool mentions(const std::string& haystack, const char* needle) {
    return haystack.find(needle) != std::string::npos;
}

}  // namespace

// INVENTED: the attack animation class a weapon swings with, by the words in its
// `UNITTYPE` (see the header); not the console's lookup.
std::string attack_animation_class(const std::string& unittype, Hand hand) {
    const std::string type = upper(unittype);
    const bool left = hand == Hand::Left;

    // CROSSBOW before BOW: the string contains the other one.
    if (mentions(type, "CROSSBOW")) return "Crossbow";
    if (mentions(type, "BOW")) return "Bow";
    if (mentions(type, "RIFLE")) return "Rifle";
    if (mentions(type, "PISTOL")) return left ? "Lpistol" : "Rpistol";
    if (mentions(type, "WAND")) return left ? "Lwand" : "Rwand";
    // A staff swings as a polearm.
    if (mentions(type, "POLEARM") || mentions(type, "STAFF")) return "Polearm";
    if (mentions(type, "SWORD") || mentions(type, "AXE") || mentions(type, "MACE")) {
        return left ? "Lslash" : "Rslash";
    }
    return "Attack";
}

// INVENTED: a weapon's damage kind by the words in its `UNITTYPE` (see the header).
WeaponDamageKind weapon_damage_kind(const std::string& unittype) {
    const std::string type = upper(unittype);
    // CROSSBOW before BOW, as above.
    if (mentions(type, "CROSSBOW") || mentions(type, "BOW") || mentions(type, "RIFLE") ||
        mentions(type, "PISTOL")) {
        return WeaponDamageKind::Ranged;
    }
    if (mentions(type, "STAFF") || mentions(type, "WAND")) return WeaponDamageKind::Magic;
    if (mentions(type, "SWORD") || mentions(type, "AXE") || mentions(type, "MACE") ||
        mentions(type, "POLEARM")) {
        return WeaponDamageKind::Melee;
    }
    return WeaponDamageKind::None;
}

// INVENTED: the hand a weapon goes in by the words in its `UNITTYPE` (a bow is left-handed).
Hand default_hand(const std::string& unittype) {
    const std::string type = upper(unittype);
    if (mentions(type, "CROSSBOW")) return Hand::Right;
    if (mentions(type, "BOW")) return Hand::Left;
    return Hand::Right;
}

}  // namespace tl::core
