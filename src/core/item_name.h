// An item's full and plain name resolution:
// `CEquipment::getFullItemName(bool)` and
// `CEquipment::getItemName()`.
#pragma once

#include "core/unit_types.h"
#include "core/units.h"

#include <string>

namespace tl::core {

/// `CEquipment::getItemName()`:
/// Returns the unidentified name (`+0x2d0`, from `UNIDENTIFIED_NAME`) if the item
/// is not identified (`identified == false`) and `unidentified_name` is non-empty;
/// otherwise returns the plain base name (`+0x2d8`). Gold is a `CItemGold`, whose
/// name is `CItem::getItemName`, `+0x210`:
/// not this function.
const std::string& item_name(const ItemStats& item);

/// `CEquipment::getFullItemName(bool)`:
/// Assembles an item's full name with affix prefix/suffix words and conditional
/// tag replacements. Caches the chosen prefix/suffix in `item.name_part_2e0` and
/// `item.name_part_2e8`.
///
/// When `force_full` is false (`CGameUI::showEquipmentTooltip`),
/// an unidentified item with an unidentified name
/// returns `item.unidentified_name`. When `force_full` is true
/// (`CEquipment::fillSaveState`), identification is
/// ignored and full affix resolution runs so the save state receives the parts.
std::string item_full_name(ItemStats& item, bool force_full, const UnitTypeHierarchy& unit_types);

}  // namespace tl::core
