// An item's unit data as the port keeps it (`ItemStats`, `core/units.h`), read
// from its record of the master unit list - what `CBaseUnit::unitInit`,
// `CItem::unitInit` and
// `CEquipment::unitInit` read out of the data
// group before a copy rolls (`init_item_copy`, `core/item_roll.h`).
#pragma once

#include "core/effects.h"
#include "core/units.h"
#include "formats/adm.h"

namespace tl::core {

/// One `ITEMS` record (the master list's, or an item file merged over its
/// `BASEFILE` chain). An item without a `NAME` comes back with an empty name.
ItemStats read_item_stats(const tl::formats::AdmSection& section,
                          const EffectTypeTable& effect_types);

}  // namespace tl::core
