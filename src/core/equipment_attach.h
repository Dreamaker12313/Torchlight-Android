// Where an equipped item is drawn on its wearer: `CEquipment::equipped`
// hands it to `CEquipment::attachToGivenLocation`,
// which puts a copy of its model on a bone of
// `KEQUIP_LOCATION_BONES` - unless the
// wearer's wardrobe draws it (`CEquipment::isWardrobed`).
// A shield always goes on slot 11's bone; an empty bone name attaches
// nothing. Scaled by `WEAPON_SCALE` / `SHIELD_SCALE`.
//
// No OGRE: the renderer makes the entity and hangs it on the bone named here.
#pragma once

#include "core/units.h"

#include <string>

namespace tl::core {

class UnitTypeHierarchy;

/// `KEQUIP_LOCATION_BONES[location]`, "" past its twelve entries.
const char* equip_location_bone(int location);
/// `KEQUIP_LOCATION_BONES_SECONDARY[location]`: the bone of an item's second
/// model (`CEquipment +0x244`); only the shoulders' has one.
const char* equip_location_bone_secondary(int location);

/// `CEquipment::isWardrobed(std::wstring)`:
/// a `[WARDROBE]` block for `character_class` (any block, when the class is
/// empty) that names a `MESH` or a `TEXTURE`.
bool item_is_wardrobed(const ItemStats& item, const std::string& character_class);

struct EquipmentAttachment {
    bool attach = false;  ///< a model goes on `bone`
    std::string bone;
    std::string secondary_bone;
    /// The location whose node the item's own particles go under (`+0x2e8`
    /// and on in `CCharacter`); a shield's is 11.
    int node_location = -1;
    float scale = 1.0f;
};

/// `CEquipment::attachToGivenLocation(CCharacter*, EEQUIP_LOCATIONS)`:
/// what is attached where for `item`
/// equipped at `location` on a wearer of `character_class` whose unit data
/// authors `weapon_scale` and `shield_scale` (1 when it does not).
EquipmentAttachment equipment_attachment(const ItemStats& item, int location,
                                         const std::string& character_class, float weapon_scale,
                                         float shield_scale, const UnitTypeHierarchy& unit_types);

}  // namespace tl::core
