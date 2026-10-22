// The gameplay half of render/renderer_tips.cpp (core/world.h).
#include "core/context_tips.h"
#include "core/settings_menu.h"
#include "core/text.h"
#include "core/world.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace tl::core {

namespace {

constexpr const char* kContextTipsGroup = "CONTEXTTIPS";

/// CGameGlobals::reload: media/GLOBALS.DAT, the
/// console's copy (worded for a pad) first.
constexpr const char* kGlobalsFiles[] = {"xbox/media/GLOBALS.DAT.adm", "media/GLOBALS.DAT.adm"};

}  // namespace

// CGameGlobals::reload, the `[CONTEXTTIPS]`
// loop
void World::load_context_tips() {
    if (tip_texts_loaded) return;
    tip_texts_loaded = true;
    for (const char* path : kGlobalsFiles) {
        const std::vector<uint8_t> bytes = read_resource(resolve_archive_path(path));
        if (bytes.empty()) continue;
        const tl::formats::AdmFile globals = tl::formats::parse_adm(bytes.data(), bytes.size());
        if (!globals.ok) continue;
        size_t found = 0;
        globals.visit([&](const tl::formats::AdmSection& section) {
            if (section.name != kContextTipsGroup) return;
            for (const tl::formats::AdmRecord& record : section.records) {
                const int tip = tl::core::context_tip_by_name(record.key);
                if (tip < 0 || record.text.empty()) continue;
                // The texts carry colour runs (`|cFFFFBA00...|u`); the window
                // draws one colour, so they come out.
                tip_texts[static_cast<size_t>(tip)] = tl::core::strip_markup(record.text);
                ++found;
            }
        });
        if (found == 0) continue;
        TL_LOG_INFO("tips: %zu context tip(s) from %s", found, path);
        return;
    }
    TL_LOG_WARN("tips: no [CONTEXTTIPS] in GLOBALS.DAT; no tip will open");
}

// CCharacter::equipmentPickedUp, called by
// CInventory::pickupEquipment for every item taken.
void World::queue_pet_bag_tip() {
    if (!pet.alive()) return;
    const tl::core::Inventory& bag = pet.bag();
    const uint32_t pickups = bag.pickups();
    if (pickups == tip_pet_pickups) return;
    tip_pet_pickups = pickups;
    if (tl::core::pet_bag_full_tip(bag.players_pet(),
                                   bag.items_in_pane(tl::core::InventoryPane::Backpack),
                                   bag.pane_size(tl::core::InventoryPane::Backpack))) {
        queue_tip(tl::core::ContextTip::PetInventoryFull);
    }
}

// CPlayer::update, the tips at
//
void World::queue_player_update_tips() {
    if (!player_control || subject_node == nullptr) return;
    const std::vector<tl::core::ContextTip> tips = tl::core::player_update_tips(
        static_cast<int>(player_health.current), static_cast<int>(player_health.max),
        static_cast<int>(player_character.mp()), static_cast<int>(player_character.max_mp()),
        pet.alive(), pet.near_death());
    for (tl::core::ContextTip tip : tips) queue_tip(tip);
}

}  // namespace tl::core
