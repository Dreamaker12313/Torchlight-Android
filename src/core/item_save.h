// An item copy to and from its `CItemSaveState`: `CEquipment::fillSaveState`
// and `CEquipment::applySaveState`. No affix
// is saved, only the effects the affixes put on the item's manager: a loaded copy has those as
// plain effects.
#pragma once

#include "core/effect_manager.h"
#include "core/random.h"
#include "core/unit_themes.h"
#include "core/unit_types.h"
#include "core/units.h"
#include "formats/save_file.h"

#include <cstdint>
#include <functional>
#include <optional>

namespace tl::core {

/// One live effect as `CItemSaveState::save`
/// writes it.
tl::formats::EffectSaveState effect_save_record(const LiveEffect& live);

/// One effect as `CItemSaveState::load`
/// rebuilds it, with the value and elapsed time `applySaveState` keeps.
ItemStats::SavedEffect effect_from_save_record(const tl::formats::EffectSaveState& record,
                                               const EffectTypeTable& types,
                                               const UnitThemeBook* themes);

/// `CEquipment::fillSaveState(state, slot, equipped)`
/// with `CItem::fillSaveState`,
/// for a copy `count` deep. Position,
/// orientation, room and ids are the caller's (a held item has none).
tl::formats::ItemSaveState fill_item_save_state(const ItemStats& item, int32_t count, int32_t slot,
                                                bool equipped,
                                                const EffectManager::Context& effects,
                                                const UnitTypeHierarchy& unit_types);

/// A socketed item's own record made into a copy: `createUnit` by its GUID
/// from a save, then that copy's `applySaveState`. Empty when there is none.
using SocketedFromSave = std::function<std::optional<ItemStats>(const tl::formats::ItemSaveState&)>;

/// `CEquipment::applySaveState`, over
/// `CItem::applySaveState`, on a copy made with `from_save` set.
/// `socketed` makes the socketed items; `effects` is what the copy's own manager is built
/// with; `rolls` is `g_RandVolatile`, which each saved effect's roll draws from (null: nothing
/// drawn). The position, orientation and spawner are the caller's.
void apply_item_save_state(ItemStats& copy, const tl::formats::ItemSaveState& state,
                           const EffectTypeTable& types, const UnitThemeBook* themes,
                           const EffectManager::Context& effects,
                           const UnitTypeHierarchy& unit_types,
                           const SocketedFromSave& socketed = {}, RunicRandom* rolls = nullptr);

}  // namespace tl::core
