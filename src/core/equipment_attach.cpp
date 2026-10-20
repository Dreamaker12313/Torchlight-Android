#include "core/equipment_attach.h"

#include "core/unit_types.h"

#include <array>
#include <cctype>

namespace tl::core {
namespace {

std::string upper(std::string text) {
    for (char& c : text) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return text;
}

/// `KEQUIP_LOCATION_BONES` and `KEQUIP_LOCATION_BONES_SECONDARY`, as the static
/// initialiser builds them (the console's copies at
/// 0x1c bytes a string).
constexpr std::array<const char*, 12> kBones = {
    "tag_righthand", "tag_lefthand", "", "Bip01 Head", "", "Bip01 L Clavicle", "", "", "", "", "",
    "tag_leftarm"};
constexpr std::array<const char*, 12> kSecondaryBones = {"", "", "", "", "", "Bip01 R Clavicle",
                                                         "", "", "", "", "", ""};

constexpr int kShieldLocation = 11;

}  // namespace

// `KEQUIP_LOCATION_BONES[location]`, "" past its twelve entries. PORT: an accessor.
const char* equip_location_bone(int location) {
    return location >= 0 && location < static_cast<int>(kBones.size())
               ? kBones[static_cast<size_t>(location)]
               : "";
}

// PORT: the second bone of an equip location, "" past its entries.
const char* equip_location_bone_secondary(int location) {
    return location >= 0 && location < static_cast<int>(kSecondaryBones.size())
               ? kSecondaryBones[static_cast<size_t>(location)]
               : "";
}

bool item_is_wardrobed(const ItemStats& item, const std::string& character_class) {
    // `CEquipment::isWardrobed`: the class upper-cased; each
    // `[WARDROBE]` block in order whose upper-cased `CLASS` is it - every block
    // when the class is empty - is the item's when it names a `MESH`, and
    // failing that a `TEXTURE`, that is not empty.
    const std::string wearer = upper(character_class);
    for (const ItemStats::Wardrobe& block : item.wardrobe) {
        if (!wearer.empty() && upper(block.character_class) != wearer) continue;
        if (!block.mesh.empty()) return true;
        if (!block.texture.empty()) return true;
    }
    return false;
}

EquipmentAttachment equipment_attachment(const ItemStats& item, int location,
                                         const std::string& character_class, float weapon_scale,
                                         float shield_scale, const UnitTypeHierarchy& unit_types) {
    // `CEquipment::attachToGivenLocation`.
    EquipmentAttachment out;
    if (item_is_wardrobed(item, character_class)) return out;
    const bool shield = unit_types.isa(item.unittype, unit_type::kShield);
    // The weapon's scale for a weapon (ISA 8), the shield's for a shield
    // (ISA 0x15), 1 for anything else.
    if (unit_types.isa(item.unittype, unit_type::kWeapon)) {
        out.scale = weapon_scale;
    } else if (shield) {
        out.scale = shield_scale;
    }
    const std::string bone = equip_location_bone(location);
    if (!bone.empty()) {
        out.attach = true;
        out.bone = bone;
        out.node_location = location;
        // A shield goes on slot 11's bone, its particles under slot 11's
        // node, from whichever location it was equipped.
        if (shield) {
            out.bone = equip_location_bone(kShieldLocation);
            out.node_location = kShieldLocation;
        }
    }
    out.secondary_bone = equip_location_bone_secondary(location);
    return out;
}

}  // namespace tl::core
