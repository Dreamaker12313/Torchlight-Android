// The renderer's implementation: crafting, sockets, player death, particle triggers,
// dropped items, themes.
#include "core/equipment_attach.h"
#include "core/item_ai.h"
#include "core/targeting.h"
#include "render/renderer_impl.h"
#include "render/unit_opacity.h"

namespace tl::render {
Renderer::Impl::CombinerState* Renderer::Impl::combiner_named(const std::string& unit) {
    load_unit_types();
    const auto range = scene_units.equal_range(unit);
    for (auto it = range.first; it != range.second; ++it) {
        const auto found = combiners.find(it->second.node);
        if (found != combiners.end()) return &found->second;
    }
    // The inventory every character has from its `unitInit`.
    const tl::core::UnitStats stats = stats_for_unit(unit);
    if (!unit_types.isa(unit_types.id_of(stats.unittype), tl::core::unit_type::kCombiner)) {
        return nullptr;
    }
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second.node == 0) continue;
        CombinerState& state = combiners[it->second.node];
        state.unit = unit;
        return &state;
    }
    return nullptr;
}

void Renderer::Impl::load_recipes() {
    if (recipes_loaded) return;
    recipes_loaded = true;
    load_unit_types();
    // `CRecipes::reload`: one recipe per file.
    std::vector<tl::formats::AdmFile> files;
    for (const std::string& path : archive_paths_under("media/recipes/", ".dat.adm")) {
        const std::vector<uint8_t> bytes = read_resource(path);
        if (bytes.empty()) continue;
        tl::formats::AdmFile file = tl::formats::parse_adm(bytes.data(), bytes.size());
        if (file.ok) files.push_back(std::move(file));
    }
    std::vector<const tl::formats::AdmFile*> pointers;
    for (const tl::formats::AdmFile& file : files) pointers.push_back(&file);
    recipes.reload(pointers, unit_types);
    TL_LOG_INFO("items: %zu recipe(s)", recipes.size());
}

void Renderer::Impl::gem_service_selected() {
    const VendorScreen page = inventory_menu.vendor();
    const bool recover = page == VendorScreen::RecoverGems;
    if (!recover && page != VendorScreen::DestroyGems) return;
    // the focused tray's selected item.
    const int slot = inventory_menu.focused_tray() == InventoryMenu::Tray::Bag
                         ? inventory_menu.selected_slot_index()
                         : -1;
    tl::core::ItemStats* item =
        slot >= 0 ? player_inventory.item_in_slot(static_cast<size_t>(slot)) : nullptr;
    load_unit_types();
    // without an item or a gem in it, the result window is
    // emptied and `ERROR` plays ((0x18, player node)`).
    if (item == nullptr || !tl::core::item_has_gems(*item)) {
        inventory_menu.set_gem_result("");
        play_ui(tl::core::InterfaceSound::Error);
        TL_LOG_INFO("%s: nothing to do", recover ? "recover gems" : "destroy gems");
        update_menu();
        return;
    }
    const std::string host = item->name;
    const size_t gems = item->socketed.size();
    std::string line;
    if (recover) {
        // `REVEAL`, each gem to the bag (`pickupEquipment`, forced)
        // or the floor, then the item removed (`removeEquipment`) and deleted.
        play_ui(tl::core::InterfaceSound::Reveal);
        const tl::core::ItemRollContext context = item_roll_context();
        std::vector<tl::core::ItemStats> out = tl::core::take_out_socketed_gems(*item, context);
        for (const tl::core::ItemStats& gem : out) {
            if (player_inventory.pickup(gem, 1, unit_types) < 0) {
                drop_item_near_player(gem, 1);
                TL_LOG_INFO("recover gems: '%s' does not fit, dropped", gem.name.c_str());
            }
        }
        player_inventory.remove_item(static_cast<size_t>(slot),
                                     player_inventory.slots()[static_cast<size_t>(slot)].count);
        line = tl::core::gems_recovered_line();
    } else {
        // the line, `REVEAL`, the gems deleted.
        const std::string name = tl::core::item_name(*item);
        line = tl::core::destroy_socketed_gems(*item, name);
        play_ui(tl::core::InterfaceSound::Reveal);
    }
    // Both: the equipment cache recalculated, the
    // line in the result window, the tray's refresh.
    player_inventory.calculate_effect_values();
    inventory_menu.set_gem_result(line);
    TL_LOG_INFO("%s: '%s', %zu gem(s); \"%s\"", recover ? "recover gems" : "destroy gems",
                host.c_str(), gems, line.c_str());
    sync_equipped_weapon();
    update_menu();
    update_character_preview(0.0f);
    refresh_game_hud();
}

void Renderer::Impl::retire_selected() {
    if (inventory_menu.vendor() != VendorScreen::Retire) return;
    // a Weapon, an Armor or a Trinket goes on, anything
    // else does nothing.
    const int slot = inventory_menu.focused_tray() == InventoryMenu::Tray::Bag
                         ? inventory_menu.selected_slot_index()
                         : -1;
    tl::core::ItemStats* item =
        slot >= 0 ? player_inventory.item_in_slot(static_cast<size_t>(slot)) : nullptr;
    load_unit_types();
    if (item == nullptr || !tl::core::can_become_heirloom(*item, unit_types)) return;

    // mode 4: the target cleared, every menu closed,
    // the item out of the bag (`removeEquipment`).
    tl::core::ItemStats heirloom = *item;
    clear_talk_target();
    player_inventory.remove_item(static_cast<size_t>(slot),
                                 player_inventory.slots()[static_cast<size_t>(slot)].count);
    player_inventory.calculate_effect_values();
    sync_equipped_weapon();
    inventory_menu.hide();
    sync_stats_page();

    // The item: renamed the first time, one heirloom level up, improved.
    const tl::core::ItemRollContext rolls = item_roll_context();
    tl::core::EffectManager::Context effects;
    effects.types = &effect_types;
    effects.graphs = &stat_graphs;
    effects.owner.present = false;
    effects.theme_book = &unit_themes;
    const std::string was = tl::core::item_name(heirloom);
    tl::core::make_heirloom(heirloom, player_character.name(), rolls, effects);

    // Into the client (`+0x1458`) with the generation (`+0x145c`), the player retired
    // (`+0x9a4`); then (not read), the menu's close, and `setGameState(0, 2)`:
    // leaving saves, New Game opens in retirement mode.
    // DIVERGENCE: the console's: the PC (`CEnchantMenu::update`) waits for the
    // NPC's animation; `CPlayer::update`'s fade has no frame to show in.
    heirloom_handoff.generation = player_character.generation() + 1;
    TL_LOG_INFO("retire: '%s' becomes '%s' (heirloom level %d), generation %d next", was.c_str(),
                tl::core::item_full_name(heirloom, true, unit_types).c_str(),
                heirloom.heirloom_level, heirloom_handoff.generation);
    heirloom_handoff.item = std::move(heirloom);
    player_character.set_retired(true);
    retirement_requested = true;
    quit_requested = true;
    refresh_game_hud();
}

bool Renderer::Impl::open_gem_socketing() {
    // the gem in the player's or the first pet's inventory, then the
    // dialog (`CGameUI +0x1894`). INVENTED: a live pet (`Pet::alive`) stands in for the list.
    const int slot = inventory_menu.selected_slot_index();
    if (slot < 0 || !player_inventory.occupied(static_cast<size_t>(slot)) || !pet.alive()) {
        return false;
    }
    if (!inventory_menu.open_item_select()) {
        TL_LOG_WARN(
            "item select: no '%s' in the console data; re-run "
            "scripts/extract_console_ui.py",
            "media/UI/messagehousing_xenon.layout");
        return false;
    }
    item_select_stick_y_held = true;
    item_select_stick_x_held = true;
    TL_LOG_INFO("item select: open for '%s'",
                player_inventory.slots()[static_cast<size_t>(slot)].item.name.c_str());
    update_menu();
    return true;
}

void Renderer::Impl::handle_item_select(const tl::core::InputState& in) {
    // `CMessageItemSelectXenon::vfunc_2`, by the pad's code.
    if (std::abs(in.move_y) > 0.5f) {
        if (!item_select_stick_y_held) {
            // 0x11 / 0x12: (-1 / +1), then the tray's refresh.
            inventory_menu.item_select_step(in.move_y > 0.5f ? -1 : 1);
            update_menu();
        }
        item_select_stick_y_held = true;
    } else {
        item_select_stick_y_held = false;
    }
    if (std::abs(in.move_x) > 0.5f) {
        if (!item_select_stick_x_held) {
            // 0x13: the tab left and `NAVIGATE` (0x43); 0x14:.
            inventory_menu.item_select_step_tab(in.move_x > 0.0f ? 1 : -1);
            play_ui(tl::core::InterfaceSound::Navigate);
            update_menu();
        }
        item_select_stick_x_held = true;
    } else {
        item_select_stick_x_held = false;
    }
    if (in.confirm && !confirm_held) {
        // Code 5: the choice, then the default case's `vfunc_3(0)`.
        inventory_menu.confirm_item_select();
        socket_selected_gem();
    } else if (in.skip && !skip_held) {
        // Any other code closes it through the default case, the choice left empty.
        inventory_menu.close_item_select();
        socket_selected_gem();
    }
}

// PORT: the level as `PetDepartureLevel` reads it, from the port's level state.
tl::core::PetDepartureLevel Renderer::Impl::pet_departure_level() const {
    tl::core::PetDepartureLevel level;
    // A layout loaded on its own is no dungeon's level and has no template.
    level.has_template = !level_dungeon.empty();
    level.allow_pet_return = level_allows_pet_return;
    level.is_town = level_is_town;
    return level;
}

void Renderer::Impl::refresh_pet_commands() {
    // `vfunc_5`: nothing without a pet (`+0x2c`).
    if (!pet.alive()) return;
    const bool can_depart = tl::core::pet_can_depart(pet_departure_level(), pet.state());
    inventory_menu.set_pet_commands_view(tl::core::pet_commands_view(pet.stance(), can_depart));
}

bool Renderer::Impl::open_pet_commands() {
    // code 0xd hands the dialog the player's first pet (`--pet none`: none).
    if (!pet.alive()) return false;
    refresh_pet_commands();
    if (!inventory_menu.open_pet_commands()) return false;
    TL_LOG_INFO("pet commands: open");
    update_menu();
    return true;
}

void Renderer::Impl::handle_pet_commands(const tl::core::InputState& in,
                                         const PetCommandEdges& edges) {
    // `CMessagePetCommandsXenon::vfunc_2`, by the pad's code; left and
    // right (0x13, 0x14) once per push; up, down, A and X are ignored.
    int code = -1;
    if (std::abs(in.move_x) > 0.5f) {
        if (!pet_commands_stick_x_held) code = in.move_x > 0.0f ? 0x14 : 0x13;
        pet_commands_stick_x_held = true;
    } else {
        pet_commands_stick_x_held = false;
    }
    if (code < 0) {
        if (edges.y) {
            code = 8;
        } else if (in.skip && !skip_held) {
            code = 6;
        } else if (edges.start) {
            code = 0xd;
        } else if (edges.back) {
            code = 0xe;
        } else if (edges.other) {
            code = 9;  // RT; the shoulders, LT and the D-pad fall to the same default
        }
    }
    if (code < 0) return;

    switch (tl::core::pet_command_for_code(code)) {
        case tl::core::PetCommand::Ignore: return;
        case tl::core::PetCommand::StanceLeft:
        case tl::core::PetCommand::StanceRight: {
            const int direction =
                tl::core::pet_command_for_code(code) == tl::core::PetCommand::StanceLeft ? -1 : 1;
            pet.set_stance(tl::core::step_pet_stance(pet.stance(), direction));
            // `CCharacter::setTarget(pet, nullptr)`.
            apply_pet_stance();
            const tl::core::PetStance stance = pet.stance();
            TL_LOG_INFO("pet commands: %s", stance == tl::core::PetStance::Aggressive ? "aggressive"
                                            : stance == tl::core::PetStance::Defensive ? "defensive"
                                                                                       : "passive");
            refresh_pet_commands();
            refresh_game_hud();
            update_menu();
            return;
        }
        case tl::core::PetCommand::SendToTown:
            // first; the voice (`VOICEPETDEPARTED`) and `sendToTown`
            // only when it says yes. Then the dialog closes either way.
            if (tl::core::pet_can_depart(pet_departure_level(), pet.state())) {
                send_pet_to_town();
            }
            break;
        case tl::core::PetCommand::Close: break;
    }
    inventory_menu.close_pet_commands();
    TL_LOG_INFO("pet commands: closed");
    update_menu();
}

void Renderer::Impl::swap_bag_and_pet() {
    // from the player's tray (tray, pet), then into
    // the pet's inventory; from the pet's (ISA 0x57) into the player's.
    if (!pet.alive()) return;
    load_unit_types();
    const bool from_pet = inventory_menu.pet_tray_focused();
    tl::core::Inventory& from = from_pet ? pet.bag() : player_inventory;
    tl::core::Inventory& to = from_pet ? player_inventory : pet.bag();
    const int slot =
        from_pet ? inventory_menu.selected_pet_slot() : inventory_menu.selected_slot_index();
    if (slot < 0 || !from.occupied(static_cast<size_t>(slot))) return;
    if (!from_pet &&
        !tl::core::pet_takes_items(/*tray_owner_is_player=*/true, pet.alive(), pet.state())) {
        return;
    }
    // DIVERGENCE: X refuses a worn item, as Drop does; takes it off and moves it.
    if (tl::core::Inventory::is_equip_location(static_cast<size_t>(slot))) {
        TL_LOG_INFO("%s: '%s' is equipped; unequip it first", from_pet ? "pet tray" : "pet bag",
                    from.slots()[static_cast<size_t>(slot)].item.name.c_str());
        return;
    }
    const tl::core::InventorySlot moved = from.slots()[static_cast<size_t>(slot)];
    switch (tl::core::move_to_inventory(from, static_cast<size_t>(slot), to,
                                        /*target_is_combiner=*/false, unit_types)) {
        case tl::core::TrayMove::Nothing: return;
        case tl::core::TrayMove::Refused:
            play_ui(tl::core::InterfaceSound::Error);
            TL_LOG_INFO("%s: '%s' does not fit", from_pet ? "bag" : "pet bag",
                        moved.item.name.c_str());
            break;
        case tl::core::TrayMove::Moved:
            play_ui(tl::core::InterfaceSound::ItemTake);
            load_stat_graphs();
            TL_LOG_INFO("%s: %d x %s, worth %d gold", from_pet ? "bag (from the pet)" : "pet bag",
                        moved.count, moved.item.name.c_str(),
                        tl::core::item_sell_price(moved.item, stat_graphs, moved.count));
            break;
    }
    player_inventory.calculate_effect_values();
    sync_equipped_weapon();
    update_menu();
    refresh_game_hud();
}

void Renderer::Impl::drop_selected_pet_item() {
    // on the pet's tray: not a quest item (ISA 0x67);
    // `removeEquipment`, `CLevel::addItem` at the owner's position, `drop`.
    if (!pet.alive() || !pet.in_world()) return;
    const int slot = inventory_menu.selected_pet_slot();
    if (slot < 0 || !pet.bag().occupied(static_cast<size_t>(slot))) return;
    load_unit_types();
    const tl::core::InventorySlot held = pet.bag().slots()[static_cast<size_t>(slot)];
    if (unit_types.isa(held.item.unittype, tl::core::unit_type::kQuestItem)) {
        // INVENTED: (the quest item's message) is not read; nothing happens.
        return;
    }
    // DIVERGENCE: a worn item stays, as on the bag's tray (see swap_bag_and_pet).
    if (tl::core::Inventory::is_equip_location(static_cast<size_t>(slot))) return;
    pet.bag().remove_item(static_cast<size_t>(slot), held.count);
    const tl::core::Vec3& at = pet.position();
    drop_item(held.item, held.count, Ogre::Vector3(at.x, at.y, at.z));
    TL_LOG_INFO("pet tray: dropped %d x %s", held.count, held.item.name.c_str());
    update_menu();
}

void Renderer::Impl::socket_selected_gem() {
    // the bag's poll once the dialog is down:.
    const int gem_slot = inventory_menu.selected_slot_index();
    const int host_slot = inventory_menu.item_select_choice();
    tl::core::ItemStats* host =
        host_slot >= 0 ? player_inventory.item_in_slot(static_cast<size_t>(host_slot)) : nullptr;
    const bool have_gem = gem_slot >= 0 && player_inventory.occupied(static_cast<size_t>(gem_slot));
    if (host == nullptr || !have_gem || gem_slot == host_slot) {
        // no character, gem or item -> `ERROR` at the player.
        play_ui(tl::core::InterfaceSound::Error);
        TL_LOG_INFO("item select: closed with nothing chosen");
        update_menu();
        return;
    }
    load_unit_types();
    // `removeEquipment` takes the gem out (gems do not stack),
    // `addContainerItem` sets it in the item, and `createElementalDamages`
    // folds the item's damage effects into its bonus.
    const tl::core::ItemStats gem = player_inventory.slots()[static_cast<size_t>(gem_slot)].item;
    const int count = player_inventory.slots()[static_cast<size_t>(gem_slot)].count;
    player_inventory.remove_item(static_cast<size_t>(gem_slot), count);
    const bool set = tl::core::add_container_item(*host, gem, unit_types);
    tl::core::create_elemental_damages(*host, item_effect_context(), unit_types);
    // The equipment cache recalculated; then sample
    // 0x11 (`LAND_SOUND`) of the item's bank at the player.
    player_inventory.calculate_effect_values();
    if (subject_node != nullptr) play_sound(host->land_sound, subject_node->getPosition());
    TL_LOG_INFO("item select: '%s' set in '%s'%s (%zu of %d socket(s))", gem.name.c_str(),
                host->name.c_str(), set ? "" : " - no free socket, the gem is lost",
                host->socketed.size(), host->sockets);
    // The tray's tab follows the item and the selection lands on it.
    inventory_menu.select_slot(static_cast<size_t>(host_slot));
    sync_equipped_weapon();
    update_menu();
    update_character_preview(0.0f);
    refresh_game_hud();
}

bool Renderer::Impl::enchant_selected() {
    // A on an enchanter's or a shrine's tray (modes 0 and 1).
    const bool shrine = inventory_menu.vendor() == VendorScreen::EnchantShrine;
    if ((inventory_menu.vendor() != VendorScreen::Enchant && !shrine) ||
        inventory_menu.focused_tray() != InventoryMenu::Tray::Bag) {
        return false;
    }
    const int slot = inventory_menu.selected_slot_index();
    tl::core::ItemStats* item =
        slot >= 0 ? player_inventory.item_in_slot(static_cast<size_t>(slot)) : nullptr;
    if (item == nullptr) return false;
    const tl::core::ItemRollContext context = item_roll_context();
    // `canEnchant` refuses: an unidentified item goes to the tray's A,
    // any other does nothing.
    if (!tl::core::item_can_enchant(*item, unit_types)) return !item->identified;
    // `CEnchantMenuXenon +0xf4`: a spent shrine does nothing (`vfunc_6`).
    if (shrine && enchant_shrine_spent()) return false;
    // With an NPC, too little gold is `VOICEINSUFFICIENTGOLD` after 0.1 s and nothing else.
    if (!shrine && player_inventory.gold() <
                       tl::core::item_enchant_price(*item, stat_graphs, item_roll_globals)) {
        play_voice(tl::core::SoundSlot::VoiceInsufficientGold, 0.1f);
        TL_LOG_INFO("enchanter: '%s' costs %d, the player has %d", item->name.c_str(),
                    tl::core::item_enchant_price(*item, stat_graphs, item_roll_globals),
                    player_inventory.gold());
        return false;
    }

    // > `CEnchantMenu::performInteraction` with an NPC, the
    // shrine's without one.
    const std::string name = tl::core::item_name(*item);
    int gold = player_inventory.gold();
    const tl::core::EnchantOutcome out = tl::core::perform_enchant(
        shrine ? tl::core::EnchantService::Shrine : tl::core::EnchantService::Enchanter, *item,
        gold, static_cast<uint32_t>(player_character.level()), name, context);
    player_inventory.set_gold(gold);
    // `GOLDBUY` is the enchanter's alone: the shrine takes no gold.
    switch (out.result) {
        case tl::core::EnchantResult::Refused: play_ui(tl::core::InterfaceSound::Error); break;
        case tl::core::EnchantResult::Sockets:
        case tl::core::EnchantResult::Enchanted:
            if (!shrine) play_ui(tl::core::InterfaceSound::GoldBuy);
            play_ui(tl::core::InterfaceSound::Reveal);
            play_voice(tl::core::SoundSlot::VoiceFortuneGood, 1.0f);
            break;
        case tl::core::EnchantResult::NothingHappens:
        case tl::core::EnchantResult::Disenchanted:
            if (!shrine) play_ui(tl::core::InterfaceSound::GoldBuy);
            play_voice(tl::core::SoundSlot::VoiceFortuneBad, 1.0f);
            play_ui(tl::core::InterfaceSound::Disenchant);
            break;
    }
    if (out.served()) {
        // The tail: the item's text rebuilt, the equipment cache recalculated.
        player_inventory.calculate_effect_values();
        if (shrine) {
            // (PC case 0x16): the shrine's `interact(player)`,
            // `CTriggerUnit::interact` -> `trigger`;
            // then clears the player's target, which the port does not keep.
            if (tl::core::LevelScript* owner = machine_script(enchant_shrine_machine)) {
                std::vector<tl::core::ScriptEvent> events;
                owner->interact_unit_trigger(enchant_shrine_node, quest_log.state(), events);
                if (enchant_shrine_machine >= 0) {
                    nested_scripts[static_cast<size_t>(enchant_shrine_machine)].running = true;
                }
                apply_machine_events(enchant_shrine_machine, events);
                TL_LOG_INFO("enchant shrine: unit trigger %lld used, %zu event(s)%s",
                            static_cast<long long>(enchant_shrine_node), events.size(),
                            enchant_shrine_spent() ? ", spent" : "");
            }
        }
        // CEnchantMenu::performInteraction: incrementJournalStatistic(17, 1)
        count_journal(tl::core::JournalStatistic::ItemsEnchanted);
    }
    inventory_menu.set_enchant_result(out.message, out.succeeded());
    inventory_menu.set_enchant_allowed(!shrine || !enchant_shrine_spent());
    TL_LOG_INFO("%s: '%s' %s for %d gold (enchants %u, sockets %d, affixes %zu); %d left",
                shrine ? "enchant shrine" : "enchanter", item->name.c_str(),
                out.result == tl::core::EnchantResult::Refused          ? "refused"
                : out.result == tl::core::EnchantResult::NothingHappens ? "nothing happens"
                : out.result == tl::core::EnchantResult::Sockets        ? "new sockets"
                : out.result == tl::core::EnchantResult::Enchanted      ? "new enchantments"
                                                                        : "disenchanted",
                out.price, item->enchant_count, item->sockets, item->affixes.size(),
                player_inventory.gold());
    sync_equipped_weapon();
    update_menu();
    update_character_preview(0.0f);
    refresh_game_hud();
    return false;
}

void Renderer::Impl::transmute_selected() {
    const auto found = combiners.find(combining_with);
    if (found == combiners.end()) return;
    load_unit_types();
    ensure_item_index();
    load_recipes();
    if (!loot_tables_built) {
        loot_tables_built = true;
        build_spawn_context(loot_tables, /*items_by_type=*/true);
    }
    tl::core::CombineContext context;
    context.recipes = &recipes;
    context.unit_types = &unit_types;
    context.items = &items;
    context.spawn_class = loot_tables.context.get_spawn_class;
    context.create_unit = loot_unit_maker();
    // `rollSpawnClass(player, player, player level +0xf0, -1, 0)`.
    context.player.level = player_character.level();
    context.player_level = player_character.level();
    context.spawn_context = &loot_tables.context;
    context.game_rng = &tl::core::game_random();

    const tl::core::CombineOutcome out =
        tl::core::transmute(found->second.inventory, player_inventory, context);
    if (out.sample == tl::core::kCombineNoSample) {
        TL_LOG_INFO("transmute: no recipe matches");
        return;
    }
    // `Reveal` on the interface bank, `VOICEFORTUNEGOOD` 0.1 s on.
    play_ui(tl::core::InterfaceSound::Reveal);
    if (out.owner_sample) play_voice(tl::core::SoundSlot::VoiceFortuneGood, 0.1f);
    // A leftover the bag would not take: `CLevel::addItem` at the player and
    // `drop()`.
    for (const tl::core::InventorySlot& left : out.dropped) {
        drop_item_near_player(left.item, left.count);
    }
    // CCombineMenu::performInteraction: incrementJournalStatistic(16, 1)
    if (out.journal_counted) count_journal(tl::core::JournalStatistic::ItemsTransmuted);
    std::string made;
    for (int slot : out.result_slots) {
        if (!made.empty()) made += ", ";
        made += found->second.inventory.slots()[static_cast<size_t>(slot)].item.name;
    }
    TL_LOG_INFO("transmute: %zu ingredient(s) used up, %zu returned to the floor, made '%s'%s",
                out.consumed.size(), out.dropped.size(), made.c_str(),
                out.unknown_spawn_class ? " (unknown spawn class)" : "");
    sync_equipped_weapon();
    update_menu();
    update_character_preview(0.0f);
    refresh_game_hud();
}

// PORT: a vendor screen's inventory, with the item events wired.
tl::core::Inventory* Renderer::Impl::housing_inventory_for(VendorScreen screen) {
    tl::core::Inventory* housing = housing_inventory_of(screen);
    // An item taken out of it is told so (`bag_item_removed`).
    if (housing != nullptr) housing->set_item_events(&housing_item_events);
    return housing;
}

tl::core::Inventory* Renderer::Impl::housing_inventory_of(VendorScreen screen) {
    switch (screen) {
        case VendorScreen::Combine: {
            const auto found = combiners.find(combining_with);
            return found != combiners.end() ? &found->second.inventory : nullptr;
        }
        // page 7's stash is the player's `+0x9f4`, page 8's `CSharedStash`.
        case VendorScreen::Stash: return &player_stash;
        case VendorScreen::SharedStash:
            return config.shared_stash != nullptr ? &config.shared_stash->inventory() : nullptr;
        default: return nullptr;
    }
}

void Renderer::Impl::swap_selected() {
    tl::core::Inventory* housing = housing_inventory_for(inventory_menu.vendor());
    if (housing == nullptr) return;
    load_unit_types();
    const bool combine = inventory_menu.vendor() == VendorScreen::Combine;
    const char* const what = combine ? "transmute" : "stash";
    if (inventory_menu.focused_tray() == InventoryMenu::Tray::Vendor) {
        // on the combiner's tray (ISA 0x86) or a stash's:
        //
        const int slot = inventory_menu.selected_vendor_slot();
        if (slot < 0) return;
        const std::string name = housing->slots()[static_cast<size_t>(slot)].item.name;
        tl::core::InventorySlot dropped;
        const tl::core::TrayReturn where =
            tl::core::return_to_player(*housing, static_cast<size_t>(slot), player_inventory,
                                       pet.alive() ? &pet.bag() : nullptr, unit_types, dropped);
        switch (where) {
            case tl::core::TrayReturn::None: return;
            case tl::core::TrayReturn::Bag:
            case tl::core::TrayReturn::PetBag: play_ui(tl::core::InterfaceSound::ItemTake); break;
            case tl::core::TrayReturn::Dropped:
                drop_item_near_player(dropped.item, dropped.count);
                break;
        }
        TL_LOG_INFO("%s: '%s' back to the %s", what, name.c_str(),
                    where == tl::core::TrayReturn::Bag      ? "bag"
                    : where == tl::core::TrayReturn::PetBag ? "pet's bag"
                                                            : "floor");
    } else {
        // on the bag: into the combiner's or the stash's.
        const int slot = inventory_menu.selected_slot_index();
        if (slot < 0 || !player_inventory.occupied(static_cast<size_t>(slot))) return;
        const std::string name = player_inventory.slots()[static_cast<size_t>(slot)].item.name;
        const tl::core::TrayMove moved = tl::core::move_to_inventory(
            player_inventory, static_cast<size_t>(slot), *housing, combine, unit_types);
        if (moved == tl::core::TrayMove::Nothing) return;
        play_ui(moved == tl::core::TrayMove::Moved ? tl::core::InterfaceSound::ItemTake
                                                   : tl::core::InterfaceSound::Error);
        const char* const target = combine ? "transmuter" : "stash";
        TL_LOG_INFO("%s: '%s' %s the %s", what, name.c_str(),
                    moved == tl::core::TrayMove::Moved ? "put in" : "refused by", target);
    }
    sync_equipped_weapon();
    update_menu();
    update_character_preview(0.0f);
    refresh_game_hud();
}

void Renderer::Impl::restore_combiner(const tl::formats::CharacterSaveState& saved) {
    // `CCharacter::applySaveState`: saved items back in.
    load_unit_types();
    const tl::core::UnitStats stats = stats_for_unit(saved.name);
    if (!unit_types.isa(unit_types.id_of(stats.unittype), tl::core::unit_type::kCombiner)) return;
    CombinerState state;
    state.unit = saved.name;
    const std::string key = level_dungeon + ":" + std::to_string(level_depth);
    const auto remembered = remembered_combiners.find(key);
    if (remembered != remembered_combiners.end()) {
        const auto kept = remembered->second.find(saved.object_id);
        if (kept != remembered->second.end()) {
            state.inventory = kept->second.inventory;
            combiners[saved.object_id] = std::move(state);
            return;
        }
    }
    ensure_item_index();
    for (const tl::formats::ItemSaveState& item : saved.inventory) {
        const tl::core::ItemStats* data = items.find(item.name);
        if (data == nullptr) continue;
        const tl::core::ItemStats copy = create_item(*data, true, true);
        const int count = std::max(1, item.value);
        if (item.slot < 0 || !state.inventory.pickup_into_slot(copy, count, item.slot)) {
            state.inventory.pickup(copy, count, unit_types);
        }
    }
    combiners[saved.object_id] = std::move(state);
}

// PORT: where the camera's subject is, for the items' active range.
Ogre::Vector3 Renderer::Impl::item_view_point() const {
    if (scripted_camera.active) {
        return scripted_camera.have_follow_position ? scripted_camera.follow_position
                                                    : scripted_camera.position;
    }
    return subject_node != nullptr ? subject_node->getPosition() : Ogre::Vector3::ZERO;
}

// What `CItem::calculateActiveRange` reads
// (`ItemActiveRangeInput`): ground distance, height between, the level's ranges.
tl::core::ItemActiveRangeInput Renderer::Impl::item_range_input(const Ogre::Vector3& at,
                                                                bool interactable,
                                                                bool casts_shadows) const {
    const Ogre::Vector3 view = item_view_point();
    tl::core::ItemActiveRangeInput in;
    in.distance =
        std::sqrt((view.x - at.x) * (view.x - at.x) + 0.0f + (view.z - at.z) * (view.z - at.z));
    in.height_difference = view.y - at.y;
    in.interactable = interactable;
    in.light_fade = level_light_fade;
    in.indoor_range = level_indoor_range;
    in.outdoor_range = level_outdoor_range;
    in.trigger_near_range = level_trigger_near_range;
    in.unit_near_range = level_unit_near_range;
    in.casts_shadows = casts_shadows;
    in.unit_shadow_range = level_unit_shadow_range;
    return in;
}

// PORT: whether an item's OGRE node is in the camera's view.
bool Renderer::Impl::item_in_camera(Ogre::SceneNode* node, bool from_model) const {
    if (camera == nullptr || node == nullptr) return true;
    const Ogre::AxisAlignedBox box = culling_box(node, from_model);
    return box.isNull() || camera->isVisible(box);
}

void Renderer::Impl::draw_item_presence(Ogre::SceneNode* node, const tl::core::ItemPresence& p,
                                        const tl::core::ItemPresenceChange& change) {
    if (node == nullptr) return;
    // `CSceneNodeObject::setVisible`, `CGenericModel::setOpacity`
    // and `setCastsShadows`.
    if (change.visibility_changed) node->setVisible(p.visible);
    if (change.opacity_written) tl::render::set_node_opacity(node, p.opacity);
    if (change.shadow_set) tl::render::set_node_casts_shadows(node, change.casts_shadow);
}

std::vector<Renderer::Impl::TriggerItem> Renderer::Impl::level_trigger_items() {
    std::vector<TriggerItem> out;
    load_unit_types();
    const std::vector<MachineView> views = machines();
    for (size_t v = 0; v < views.size(); ++v) {
        const MachineView& view = views[v];
        for (const tl::core::LevelScript::UnitTriggerView& unit :
             view.script->every_unit_trigger()) {
            TriggerItem item;
            item.key = instance_key(unit.id, view.scene, view.hierarchy);
            item.machine = static_cast<int>(v) - 1;
            item.node = unit.id;
            item.node_map = view.nodes;
            if (view.nodes != nullptr) {
                const auto range = view.nodes->equal_range(unit.id);
                for (auto it = range.first; it != range.second; ++it)
                    item.nodes.push_back(it->second);
            }
            // `setUnitModelName` adds the unit to the
            // level's items (`CLevel::addItem`) when a unit file names it.
            if (item.nodes.empty() && !collision.has_group(item.key)) {
                TriggerProp named;
                trigger_prop(unit.unit, named);
                if (!named.found) continue;
            }
            tl::core::Vec3 low, high;
            if (!item.nodes.empty()) {
                item.at = item.nodes.front()->_getDerivedPosition();
            } else if (collision.group_bounds(item.key, low, high)) {
                item.at = Ogre::Vector3((low.x + high.x) * 0.5f, low.y, (low.z + high.z) * 0.5f);
            } else {
                item.at = Ogre::Vector3(unit.at.x, unit.at.y, unit.at.z);
            }
            auto found = trigger_presence.find(item.key);
            if (found == trigger_presence.end()) {
                // `addItem`: opacity 1, on
                // the near list.
                TriggerPresence fresh;
                fresh.presence.opacity = 1.0f;
                TriggerProp prop;
                trigger_prop(unit.unit, prop);
                fresh.interactable = unit_types.isa(unit_types.id_of(prop.unittype),
                                                    tl::core::unit_type::kInteractable);
                fresh.shadows = prop.shadows;
                // `CTriggerUnit::unitInit`:
                // a `LAYOUTFILE` makes the trigger's own layout (`+0x240`).
                fresh.presence.own_layout = !prop_layout_path(unit.unit).empty();
                found = trigger_presence.emplace(item.key, fresh).first;
            }
            item.state = &found->second;
            out.push_back(std::move(item));
        }
    }
    return out;
}

void Renderer::Impl::trigger_range_collision(int64_t key, bool active) {
    // `CTriggerUnit::update`: with `COLLIDEABLE`,
    // `+0x19c = +0x199 && !+0x218`.
    if (!collision.has_group(key) || trigger_not_collidable.count(key) != 0) return;
    if (!active) {
        if (collision.group_enabled(key)) {
            collision.set_group_enabled(key, false);
            trigger_range_off.insert(key);
        }
    } else if (trigger_range_off.erase(key) > 0) {
        collision.set_group_enabled(key, true);
    }
}

void Renderer::Impl::item_near_list() {
    // `CLevel::updateNearUnitList`: every item measured
    // again (`CItem::calculateActiveRange`); one past its near
    // range is hidden at once (`setVisible(false, true)`, `hideItemText`) and left off the
    // near list `updateItems` walks.
    load_unit_types();
    for (BreakableWorld& b : breakables) {
        if (b.node == nullptr) continue;
        tl::core::ItemPresenceChange change;
        const uint32_t type = unit_types.id_of(stats_for_unit(b.unit).unittype);
        b.listed = tl::core::item_near_list_entry(
            b.presence,
            item_range_input(b.node->getPosition(),
                             unit_types.isa(type, tl::core::unit_type::kInteractable),
                             b.data.shadows),
            change);
        b.state.active = b.presence.active;
        draw_item_presence(b.node, b.presence, change);
    }
    for (DroppedWorldItem& dropped : dropped_items) {
        if (dropped.node == nullptr) continue;
        tl::core::ItemPresenceChange change;
        dropped.listed = tl::core::item_near_list_entry(
            dropped.presence,
            item_range_input(dropped.node->getPosition(), false, dropped.item.shadows), change);
        draw_item_presence(dropped.node, dropped.presence, change);
    }
    // The `Unit Trigger`s (doors, chests, levers, shrines), each a `CItem`.
    const std::vector<TriggerItem> triggers = level_trigger_items();
    size_t triggers_listed = 0;
    size_t triggers_active = 0;
    for (const TriggerItem& item : triggers) {
        TriggerPresence& t = *item.state;
        tl::core::ItemPresenceChange change;
        t.listed = tl::core::item_near_list_entry(
            t.presence, item_range_input(item.at, t.interactable, t.shadows && !item.nodes.empty()),
            change);
        for (Ogre::SceneNode* node : item.nodes) draw_item_presence(node, t.presence, change);
        triggers_listed += t.listed ? 1 : 0;
        triggers_active += t.presence.active ? 1 : 0;
    }
    std::array<size_t, 3> counts{triggers_listed, triggers_active,
                                 breakables.size() + dropped_items.size() + triggers.size()};
    float nearest = -1.0f;
    const auto count = [&](bool listed, bool active, const Ogre::SceneNode* node) {
        counts[0] += listed ? 1 : 0;
        counts[1] += active ? 1 : 0;
        if (node == nullptr) return;
        const float d = item_range_input(node->getPosition(), false, false).distance;
        if (nearest < 0.0f || d < nearest) nearest = d;
    };
    for (const BreakableWorld& b : breakables) count(b.listed, b.presence.active, b.node);
    for (const DroppedWorldItem& d : dropped_items) count(d.listed, d.presence.active, d.node);
    if (counts != item_near_logged) {
        item_near_logged = counts;
        TL_LOG_INFO("items: near list %zu of %zu, %zu active, the nearest %.1f away", counts[0],
                    counts[2], counts[1], static_cast<double>(nearest));
    }
}

void Renderer::Impl::step_level_items(float delta) {
    // `CLevel::updateItems` over the near list: `update`,
    // or `deleteItem` for one marked (`+0x190`).
    load_unit_types();
    for (size_t i = 0; i < breakables.size();) {
        BreakableWorld& b = breakables[i];
        if (b.presence.deleted) {
            // `CLevel::deleteItem`: off the
            // level's lists and freed; its spawner let go of it when it broke.
            TL_LOG_INFO("breakable: %s deleted, broken and out of range", b.unit.c_str());
            remove_breakable(b);
            breakables.erase(breakables.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        if (b.listed && b.node != nullptr) {
            // `CBreakable::update`: `CItem::update`,
            // then the collision model while active.
            tl::core::ItemPresenceChange change;
            b.presence.used = b.state.used;
            const uint32_t type = unit_types.id_of(stats_for_unit(b.unit).unittype);
            tl::core::update_item(
                b.presence,
                item_range_input(b.node->getPosition(),
                                 unit_types.isa(type, tl::core::unit_type::kInteractable),
                                 b.data.shadows),
                true, item_in_camera(b.node), delta, change);
            b.state.active = b.presence.active;
            draw_item_presence(b.node, b.presence, change);
            const bool was = b.state.collision_live;
            tl::core::breakable_update(b.state);
            if (b.has_collision && was != b.state.collision_live) {
                collision.set_group_enabled(b.id, b.state.collision_live);
            }
        }
        ++i;
    }
    for (DroppedWorldItem& dropped : dropped_items) {
        if (!dropped.listed || dropped.node == nullptr) continue;
        tl::core::ItemPresenceChange change;
        tl::core::update_item(
            dropped.presence,
            item_range_input(dropped.node->getPosition(), false, dropped.item.shadows), false,
            item_in_camera(dropped.node), delta, change);
        draw_item_presence(dropped.node, dropped.presence, change);
        step_dropped_item(dropped, delta);
    }
    // `CTriggerUnit::update`: `CItem::update`, then the
    // collision model by the range. Its layouts and animation queue are the port's own.
    for (const TriggerItem& item : level_trigger_items()) {
        TriggerPresence& t = *item.state;
        if (!t.listed) continue;
        bool seen = item.nodes.empty();
        for (Ogre::SceneNode* node : item.nodes) seen = seen || item_in_camera(node, true);
        tl::core::ItemPresenceChange change;
        tl::core::update_item(
            t.presence, item_range_input(item.at, t.interactable, t.shadows && !item.nodes.empty()),
            false, seen, delta, change);
        for (Ogre::SceneNode* node : item.nodes) draw_item_presence(node, t.presence, change);
        if (change.own_layout_shown) {
            // `CLayout::setVisible(true)` on the trigger's layout: restarted only if hidden.
            for (ParticleSystem& system : particle_systems) {
                if (system.trigger_own_layout && system.owner == item.node &&
                    system.owner_nodes == item.node_map && (system.pu_dying || system.finished)) {
                    restart_particle_system(system);
                }
            }
        }
        trigger_range_collision(item.key, t.presence.active && !t.presence.used);
        // The rest of `update`: a state change that asked to be told (`+0x25d`) broadcasts
        // `Triggered` and the state.
        tl::core::LevelScript* owner = machine_script(item.machine);
        if (owner == nullptr) continue;
        std::vector<tl::core::ScriptEvent> events;
        owner->update_unit_trigger(item.node, quest_log.state(), events);
        if (events.empty()) continue;
        // A room's machine that had gone quiet has something to run again.
        if (item.machine >= 0) nested_scripts[static_cast<size_t>(item.machine)].running = true;
        apply_machine_events(item.machine, events);
    }
}

int Renderer::Impl::breakable_within_view(float max_angle, float max_distance) const {
    if (subject_node == nullptr) return -1;
    // `CLevel::findItemWithinView`, newest first, from
    // the attacker's frame.
    tl::core::ViewFrame frame;
    const Ogre::Vector3 me = subject_node->getPosition();
    const Ogre::Quaternion turn = subject_node->getOrientation();
    frame.position = tl::core::Vec3{me.x, me.y, me.z};
    frame.orientation = tl::core::OgreQuaternion{turn.w, turn.x, turn.y, turn.z};
    for (size_t i = breakables.size(); i-- > 0;) {
        const BreakableWorld& b = breakables[i];
        if (b.node == nullptr) continue;
        if (!tl::core::breakable_candidate(b.state, b.entity != nullptr && b.presence.visible)) {
            continue;
        }
        const Ogre::Vector3 p = b.node->getPosition();
        const tl::core::Vec3 at{p.x, p.y, p.z};
        if (!tl::core::item_within_view(frame, at, max_angle, max_distance)) continue;
        // `sightUnobstructed(from, to, false)`, both ends 5 up.
        tl::core::CollisionHit hit;
        if (collision.ray_collision({me.x, me.y + 5.0f, me.z}, {p.x, p.y + 5.0f, p.z}, false,
                                    hit)) {
            continue;
        }
        return static_cast<int>(i);
    }
    return -1;
}

// A prop's trap: `CSkillManager::executeSkill` on the
// prop's own manager, as `CBreakable::interact` and
// `CTriggerUnit::trigger` run it; one the player noticed
// is `queueGlobalSample(0x36)` and journal statistic 10. The caller takes the prop's
// collision out around it.
void Renderer::Impl::cast_prop_trap(const std::string& unit, const std::string& name,
                                    uint32_t level, const Ogre::Vector3& skill_at,
                                    const Ogre::Quaternion& facing, bool by_player) {
    ensure_player_skill_files();
    ensure_effect_data();
    const auto file = player_skill_files.find(tl::core::upper_ascii(name));
    if (file == player_skill_files.end()) {
        TL_LOG_WARN("prop: %s knows no skill '%s'", unit.c_str(), name.c_str());
        return;
    }
    tl::core::KnownSkill skill;
    skill.entry.name = file->second->file.string_or("NAME", name);
    skill.def = &file->second->def;
    skill.invested = level;
    skill.effective = level;
    tl::core::SkillCaster caster;
    caster.is_character = false;
    caster.unit_types = &unit_types;
    // The prop's own effect manager, which nothing reads again.
    tl::core::EffectManager own;
    tl::core::SkillCastContext context;
    context.target = &own;
    context.types = &effect_types;
    context.unit_types = &unit_types;
    context.affix = [this](const std::string& n) -> const tl::core::Affix* {
        return affixes.get_affix(n);
    };
    context.caster = &caster;
    context.on_event = [this, skill_at, facing](const tl::core::KnownSkill& cast, uint32_t, int,
                                                const tl::formats::AdmSection& event,
                                                const tl::core::SkillEventTarget&) {
        // `CSkillEvent::startEvent`: the block's layout
        // at the cast point, facing the prop's way. INVENTED: the damage shape's listener
        // (`CDamageShape +0x1c8`) is not wired, so an explosion deals its shape's damage only.
        const tl::core::SkillEventSpec spec = tl::core::read_skill_event(event);
        if (spec.file.empty()) return;
        const size_t before = nested_scripts.size();
        if (!place_layout(spec.file, skill_at, facing, /*depth=*/1)) {
            TL_LOG_WARN("prop: '%s' could not place '%s'", cast.entry.name.c_str(),
                        spec.file.c_str());
            return;
        }
        for (size_t i = before; i < nested_scripts.size(); ++i) {
            NestedScript& machine = nested_scripts[i];
            std::vector<tl::core::ScriptEvent> events;
            machine.running = true;
            machine.script.play_layout_timelines(quest_log.state(), events, /*backwards=*/false);
            if (!events.empty()) apply_script_events(events, machine.nodes);
        }
        TL_LOG_INFO("prop: '%s' places %s", cast.entry.name.c_str(), spec.file.c_str());
    };
    tl::core::SkillRunner runner;
    const bool executed = runner.execute(skill, level, caster, context);
    TL_LOG_INFO("prop: %s springs '%s': %s", unit.c_str(), skill.entry.name.c_str(),
                executed ? "cast" : "the chance failed");
    if (tl::core::breakable_trap_noticed(executed, by_player,
                                         static_cast<int32_t>(skill.def->type))) {
        // CBreakable::interact: incrementJournalStatistic(10, 1)
        play_voice(tl::core::SoundSlot::VoiceTrapSprung, 1.0f);
        count_journal(tl::core::JournalStatistic::TrapsSprung);
    }
}

bool Renderer::Impl::break_breakable(size_t index, bool by_player) {
    if (index >= breakables.size() || breakables[index].node == nullptr) return false;
    const Ogre::Vector3 position = breakables[index].node->getPosition();
    const Ogre::Quaternion facing = breakables[index].node->getOrientation();
    const Ogre::Vector3 up = facing * Ogre::Vector3::UNIT_Y;
    const bool have_breaker = by_player && subject_node != nullptr;
    const Ogre::Vector3 breaker_at = have_breaker ? subject_node->getPosition() : position;
    const tl::core::Vec3 breaker{breaker_at.x, breaker_at.y, breaker_at.z};

    // `CBreakable::interact(CCharacter*)`.
    BreakableWorld& b = breakables[index];
    const tl::core::BreakableInteraction out = tl::core::breakable_interact(
        b.state, /*in_level=*/true, {position.x, position.y, position.z}, {up.x, up.y, up.z},
        have_breaker ? &breaker : nullptr, static_cast<uint32_t>(b.data.skills.size()),
        tl::core::game_random());
    if (!out.broke) return false;
    // `+0x1f0` cleared, then `removeFromAvoidanceMap` (vtable +0x268).
    breakable_avoidance(b, false);

    // The skill, cast with the breakable's own collision out and put back.
    const std::string unit = b.unit;
    const int64_t id = b.id;
    const bool has_collision = b.has_collision;
    const tl::core::BreakableData data = b.data;
    if (out.skill >= 0 && static_cast<size_t>(out.skill) < data.skills.size()) {
        if (has_collision) collision.set_group_enabled(id, false);
        const std::string& name = data.skills[static_cast<size_t>(out.skill)];
        cast_prop_trap(unit, name, data.skill_levels[static_cast<size_t>(out.skill)],
                       Ogre::Vector3(out.skill_at.x, out.skill_at.y, out.skill_at.z), facing,
                       have_breaker);
        if (has_collision) collision.set_group_enabled(id, true);
    }

    // The particle, `+0.1` along the up vector, started.
    if (!data.interact_particle.empty()) {
        std::string particle = preloaded_particle_file(data.interact_particle);
        if (particle.empty()) particle = data.interact_particle;
        std::vector<Ogre::SceneNode*> holders = place_particle_layout(
            particle, Ogre::Vector3(out.particle_at.x, out.particle_at.y, out.particle_at.z),
            Ogre::Quaternion::IDENTITY, nullptr);
        if (holders.empty()) {
            TL_LOG_WARN("breakable: %s has no particle '%s'", unit.c_str(),
                        data.interact_particle.c_str());
        } else {
            transient_particles.push_back(TransientParticles{std::move(holders), 0.0f});
        }
    }
    // Sample 0 of its own bank, `INTERACT_SOUND`.
    play_sound(data.interact_sound, position);

    // `CLevel::rollTreasure(breaker, null, position)`.
    if (!loot_tables_built) {
        loot_tables_built = true;
        build_spawn_context(loot_tables, /*items_by_type=*/true);
    }
    const tl::core::SpawnContext& spawn_context = loot_tables.context;
    tl::core::LootCharacter breaker_loot;
    const tl::core::LootCharacter* broken_by =
        loot_character(have_breaker ? 0 : tl::core::LevelScript::kNoAttacker, breaker_loot);
    const tl::core::LootPlacement place = loot_placement_at(position);
    const tl::core::SpawnClass* barrel =
        spawn_context.get_spawn_class ? spawn_context.get_spawn_class("BARREL_TREASURE") : nullptr;
    const tl::core::LootDrop drop = tl::core::roll_treasure(
        barrel, level_monster_level, broken_by, nullptr, tl::core::volatile_random(),
        tl::core::game_random(), spawn_context, loot_unit_maker(), &place);
    int gold = 0;
    for (int pile : drop.gold) gold += pile;

    // `setVisible(false, true)`, the collision model out, `+0x218`; found again, as the
    // skill's layout may have grown `breakables`.
    for (BreakableWorld& again : breakables) {
        if (again.id != id) continue;
        tl::core::ItemPresenceChange change;
        again.presence.used = again.state.used;
        tl::core::set_item_visible_now(again.presence, false, change);
        draw_item_presence(again.node, again.presence, change);
        if (again.has_collision) collision.set_group_enabled(id, again.state.collision_live);
    }
    TL_LOG_INFO("breakable: %s broken%s, %zu item(s) and %zu pile(s) of gold (%d)", unit.c_str(),
                have_breaker ? " by the player" : "", drop.items.size(), drop.gold.size(), gold);

    // `CItem::interact`: `questEventFire(2)` (the player
    // only), then, for a character, unit state 2 to its spawner.
    {
        tl::core::QuestEventUnit broken =
            quest_event_unit(unit, stats_for_unit(unit).unittype, position, {});
        quest_event(quest_log.unit_interacted(broken, have_breaker));
    }
    if (have_breaker) spawned_item_state(id, 2);
    return true;
}

int Renderer::Impl::breakable_index(int64_t id) const {
    for (size_t i = 0; i < breakables.size(); ++i) {
        if (breakables[i].id == id && breakables[i].node != nullptr) return static_cast<int>(i);
    }
    return -1;
}

void Renderer::Impl::remove_breakable(BreakableWorld& b) {
    // `CCharacter::notifyOfDeletion(CItem*)`: a character whose
    // target item it was lets it go.
    if (player_skill_item == b.id) player_skill_item = 0;
    breakable_avoidance(b, false);
    if (b.has_collision) collision.set_group_enabled(b.id, false);
    if (b.node != nullptr) scene->destroySceneNode(b.node);
    if (b.entity != nullptr) scene->destroyEntity(b.entity);
    b.node = nullptr;
    b.entity = nullptr;
}

/// Take life off the character.
void Renderer::Impl::hurt_player(float amount, const std::string& from, int64_t attacker,
                                 bool struck, bool shown) {
    if (amount <= 0.0f || player_health.dead) return;
    if (!(player_health.max > 0.0f)) return;
    // `applyDamage`: onto the number over the character (`+0x1ec`). INVENTED: a
    // glancing blow on the player is not told apart, so it is added too.
    if (shown) player_damage_shown.pending = amount + player_damage_shown.pending;
    // `CCharacter::applyDamage`: struck, and `+0x708` run out, the
    // `CAST SKILL ON STRUCK` procs roll (`executeProcs(0x51, attacker)`), ahead
    // of `modifyHP` and its god mode.
    if (struck && !(0.0f < player_proc_lockout)) {
        player_execute_procs(tl::core::kEffectCastSkillOnStruck, proc_target_attacker(attacker));
        if (player_health.dead) return;
    }
    // `CCharacter::modifyHP`: in god mode (vtable +0x290,
    // `CPlayer`) the health is set to `maxHP()` instead of changed.
    if (god_mode) {
        player_health.current = static_cast<float>(static_cast<int>(player_health.max));
        return;
    }
    const float before_hit = player_health.current;
    player_health.current -= amount;
    // `+0x52e`, `INVINCIBLE`: under 1 the health is put back to 1
    // and `POISON` taken off.
    if (player_invincible && player_health.current < 1.0f) {
        player_effects.remove_effect("POISON");
        player_health.current = 1.0f;
    }

    // `CCharacter::modifyHP`: `VOICEHEALTHLOW_SOUND` only on crossing 0.2 of
    // `maxHP()` downwards, half a second.
    const float low_water = player_health.max * tl::core::kVoiceHealthLowFraction;
    if (before_hit > low_water && player_health.current <= low_water) {
        play_voice(tl::core::SoundSlot::VoiceHealthLow, tl::core::kVoiceHealthLowDelay);
    }
    TL_LOG_INFO("combat: %s hits the character for %.1f, %.1f of %.1f left", from.c_str(),
                static_cast<double>(amount),
                static_cast<double>(std::max(0.0f, player_health.current)),
                static_cast<double>(player_health.max));

    // `CCharacter::modifyHP` at 0: `die` (vtable +0x330), `CPlayer::die` for the player.
    if (player_health.current <= 0.0f) {
        // Under 0, `forceDisplayOfDamageAbsorbed` first.
        if (player_health.current < 0.0f) display_player_damage(tl::core::kForcedDamageDisplay);
        player_health.current = 0.0f;
        kill_player(attacker);
    }
    // `applyDamage`: struck by an attacker, the
    // character's friends near it turn on the attacker (`CLevel::reactToAreaDamage`); its
    // own `reactToDamage` does nothing.
    if (struck && attacker != 0 && subject_node != nullptr) {
        if (tl::core::LevelScript* by = script_owning(attacker)) {
            const Ogre::Vector3 here = subject_node->getPosition();
            std::vector<tl::core::ScriptEvent> events;
            by->react_to_area_damage(attacker, by, nullptr, 0,
                                     tl::core::Vec3{here.x, here.y, here.z}, player_stats.radius,
                                     quest_log.state(), events);
            if (NodeMap* nodes = nodes_of(by); nodes != nullptr && !events.empty()) {
                apply_script_events(events, *nodes);
            }
        }
    }
    if (0.0f < player_health.current && attacker != 0) {
        // `applyDamage`: still alive, the character answers the
        // attacker with what it reflects.
        reflect_damage(player_total(), 0, "the character", amount, attacker);
    }
}

// CPlayer::die
void Renderer::Impl::kill_player(int64_t attacker) {
    // `alive()`, and `CGameClient +0x38d0 == 1`: a game being played.
    if (player_death.dying || !player_control) return;
    // Dying of its effects, the attacker is whom they credit (`updateHP`).
    if (player_hurt_by_effects) attacker = player_effect_killer;

    // CCharacter::die, the parts a player reaches: no
    // `DEATH_FLIGHT` and no burst. INVENTED gap: `stopAllSkills`
    // and `fireSkillsOnDeath` are not run for the player.
    player_death.dying = true;  // setAIState(5)
    // CCharacter::die: incrementJournalStatistic(5, 1)
    count_journal(tl::core::JournalStatistic::Deaths);
    player_health.current = 0.0f;  // modifyHP(-HP)
    player_health.dead = true;
    swing = Swing{};                            // +0x378, +0x380, +0x267
    player_active_skill.animation_left = 0.0f;  // +0x380
    player_active_skill.casting = false;        // +0x267
    skill_playback = SkillPlayback{};
    if (subject_node != nullptr) {  // sample 0xc
        play_sound(player_stats.sounds[tl::core::SoundSlot::Death], subject_node->getPosition());
    }
    player_target_node = 0;   // setTarget(0)
    player.stop_following();  // CPath::Clear
    for (MachineView& view : machines()) view.script->set_player_alive(false);
    // `executeProcs(0x69, attacker)`: the `CAST SKILL ON DEATH` effects.
    player_execute_procs(tl::core::kEffectCastSkillOnDeath, proc_target_attacker(attacker));
    // the death particle, by "Show Blood".
    if (subject_node != nullptr) play_death_particle(player_stats, subject_node->getPosition());

    // `findRandomAnimation("DIE")`: the first match, then
    // each later one on `randomBetweenVolatile(0, 1000) < 500`. None: `clearAnimations`.
    player_death.animation.clear();
    if (subject != nullptr && subject->getMesh()) {
        for (const std::string& stem : manifest_for(subject->getMesh()->getName()).order()) {
            const std::string upper = tl::core::upper_ascii(stem);
            if (upper.compare(0, 3, "DIE") != 0) continue;
            if (player_death.animation.empty() ||
                tl::core::volatile_random().between(0.0f, 1000.0f) < 500.0f) {
                player_death.animation = stem;
            }
        }
    }
    if (!player_death.animation.empty() && !link_subject_animation(player_death.animation)) {
        player_death.animation.clear();
    }
    TL_LOG_WARN("combat: the character is down (%s)", player_death.animation.empty()
                                                          ? "no DIE animation"
                                                          : player_death.animation.c_str());
    // `unitBroadcastMessage(1)`; dying of its effects, `updateHP` sends it
    // after its own 4 instead (`update_player_meters`).
    if (!player_hurt_by_effects)
        unit_observers.broadcast({nullptr, 0}, tl::core::unit_state::kDied);

    // A hardcore character is saved dead: `saveCharacter(true, true)`.
    if (config.hardcore && config.save_path != nullptr) save_character(*config.save_path, true);
}

// CPlayer::updateAnimation and CPlayer::update.
void Renderer::Impl::step_player_death(float delta) {
    if (!player_control || !player_death.dying) return;
    bool falling = false;
    if (!player_death.animation.empty() && subject != nullptr) {
        const std::string name = animation_named(subject, player_death.animation);
        if (!name.empty()) {
            Ogre::AnimationState* state = subject->getAnimationState(name);
            if (state->getEnabled() && !state->hasEnded()) {
                player_death.timer = tl::core::kDeathScreenDelay;
                falling = true;
            }
        }
    }
    // `CCharacter::completeDeath`: every weapon trail
    // off, and no outline.
    if (!falling) {
        player_trails.trails.hide_all();
        if (const RenderBehind* behind = render_behind_of(subject);
            behind != nullptr && behind->on) {
            set_subject_render_behind(false, false);
        }
    }
    player_death.timer -= delta;
    if (!(player_death.timer <= 0.0f)) return;
    open_death_screen();
    player_death.timer = tl::core::kDeathTimerParked;
}

// CGameUI::toggleDeath: unpause, close every menu, open `CDieMenu`.
void Renderer::Impl::open_death_screen() {
    if (death_screen_up) return;
    if (inventory_menu.visible()) inventory_menu.hide();
    sync_stats_page();
    if (quest_dialog.visible()) close_quest_offer();
    // CDieMenu::setOpen takes the automap down.
    if (automap_initialized) automap.hide();
    death_screen_up = true;
    death_selected = 0;
    refresh_death_screen();
    if (death_screen.ready()) {
        death_screen.show();
    } else {
        TL_LOG_WARN("death screen: no console interface data mounted; the pad still chooses");
    }
    const tl::core::DeathPenalty f =
        tl::core::death_screen_figures(stat_graphs, player_character.level(),
                                       player_character.fame_level(), player_inventory.gold());
    TL_LOG_INFO("death screen%s: here -%d xp -%d fame, entrance -%d gold, town free",
                config.hardcore ? " (hardcore)" : "", f.experience, f.fame, f.gold);
}

// PORT: the death screen's text (`death_screen_figures`, `resurrect_*`) handed to it.
void Renderer::Impl::refresh_death_screen() {
    const tl::core::DeathPenalty f =
        tl::core::death_screen_figures(stat_graphs, player_character.level(),
                                       player_character.fame_level(), player_inventory.gold());
    DeathScreenState state;
    state.title = tl::core::death_screen_title(config.hardcore);
    state.choices[0] = tl::core::resurrect_here_text(f);
    state.choices[1] = tl::core::resurrect_entrance_text(f);
    state.choices[2] = tl::core::resurrect_town_text();
    state.hardcore = config.hardcore;
    state.selected = death_selected;
    death_screen.set_state(state);
}

// CDieMenu::onClick, CDieMenu::update
// and CGameClient::setGameState(5).
void Renderer::Impl::choose_resurrection(tl::core::ResurrectChoice choice) {
    if (!death_screen_up || config.hardcore) return;
    const tl::core::DeathPenalty taken =
        tl::core::death_penalty(choice, stat_graphs, player_character.level(),
                                player_character.fame_level(), player_inventory.gold());
    death_screen_up = false;
    death_screen.hide();

    // `giveGold(-gold)`, `awardFame(-fame)`, `awardExperience(-experience)`, in that order.
    player_inventory.give_gold(-taken.gold);
    award_player_fame(-taken.fame);
    award_player_experience(-taken.experience, false);
    TL_LOG_INFO("resurrect (choice %d): lost %d xp, %d fame, %d gold", static_cast<int>(choice),
                taken.experience, taken.fame, taken.gold);

    if (choice == tl::core::ResurrectChoice::Town && !level_is_town) {
        // `CPlayer::levelResetting`, then `warpLevels("Town", -99)` (main.cpp's,
        // `Renderer::dungeon_transition`); `TOWNPORTALENTER` (`CGameClient::create`).
        //
        level_resetting_player(choice);
        play_ui(tl::core::InterfaceSound::TownPortalEnter);
        death_town_pending = true;
        dungeon_transition_pending = true;
        return;
    }
    // `CLevel::restartLevel`: `levelResetting` on every character, newest first
    // (`LevelScript::restart_level`). INVENTED: rooms last placed first, the level's last.
    std::vector<MachineView> views = machines();
    for (auto view = views.rbegin(); view != views.rend(); ++view) {
        std::vector<tl::core::ScriptEvent> events;
        view->script->restart_level(quest_log.state(), events);
        if (!events.empty()) apply_script_events(events, *view->nodes);
    }
    level_resetting_player(choice);
    if (automap_initialized && config.minimap) automap.show();
}

// CPlayer::levelResetting
void Renderer::Impl::level_resetting_player(tl::core::ResurrectChoice choice) {
    player_health.current = player_health.max;  // modifyHP(maxHP)
    player_health.dead = false;
    player_character.set_mp(player_character.max_mp(), player_character.max_mp());
    // `CGameUI +0x1928 == 1`: the level's start (`CLevel +0x140`), dropped to the
    // ground, turned toward `+0x164`.
    if (choice == tl::core::ResurrectChoice::Entrance && have_player_start) {
        float surface = player_start.y;
        collision.ground_under(player_start.x, player_start.z, player_start.y + 100.0f, surface);
        player.place({player_start.x, surface, player_start.z}, player_start_yaw);
        step_player(0.0f, tl::core::InputState{});
        // The start stands inside the entrance's triggers: each machine is told the player
        // is already there (`setPosition` enters none).
        const Ogre::Vector3 at(player_start.x, surface, player_start.z);
        for (const MachineView& view : machines()) {
            view.script->arrive_at(tl::core::Vec3{at.x, at.y, at.z});
        }
    }
    // Each pet `teleportToMaster(count * 0.25)` and healed. INVENTED: put beside the player.
    if (pet.alive() && pet_node_id != 0) {
        float hp = 0.0f;
        float max = 0.0f;
        script.unit_health(pet_node_id, hp, max);
        script.set_unit_hp(pet_node_id, max);
        place_pet_beside_player();
    }
    // setAIState(0) and `IDLE`.
    player_death.dying = false;
    player_death.animation.clear();
    player_animation.clear();
    for (MachineView& view : machines()) view.script->set_player_alive(true);
    // With an effect manager: four affixes deleted by name,
    // three of them the monsters' spellings (the console's,
    // kept); then every effect not `SAVE`d (`removeNonSavedEffects`).
    for (const char* affix : {"DAMAGEEFFECTFROZENPLAYER", "DAMAGEEFFECTONFIRE",
                              "DAMAGEEFFECTELECTRICITY", "DAMAGEEFFECTPOISONED"}) {
        player_effects.delete_affix(affix);
    }
    player_effects.remove_non_saved_effects();
    const tl::core::Vec3 at = player.position();
    TL_LOG_INFO("resurrected with %.0f hit points at (%.2f, %.2f, %.2f)",
                static_cast<double>(player_health.current), at.x, at.y, at.z);
}

/// Land every hunting unit's swing on its manifest's `HIT` frame, against the `Hunt`
/// target (either side: the player or any unit). The blow is spotted by the animation's
/// time passing the frame, one hit per loop, so `ATTACKSPEED` sets the rate.
void Renderer::Impl::step_monster_swings(float delta) {
    (void)delta;
    // `CCharacter::updateAttack` runs on each `HIT` event; the unit's machine
    // decides what the blow does.
    std::vector<tl::core::ScriptEvent> events;
    for (auto& entry : scene_units) {
        SceneUnit& unit = entry.second;
        if (unit.playing_state == nullptr || !unit.playing_state->getEnabled()) continue;
        if (unit.node == 0 || script.unit_is_dead(unit.node)) continue;

        const float hit_at = manifest_for(unit.mesh).seconds_of(unit.playing, "HIT", -1.0f);
        const float now = unit.playing_state->getTimePosition();
        const float was = unit.last_time;
        unit.last_time = now;
        if (hit_at < 0.0f) continue;
        // Wrapped round the loop, or stepped past the frame this frame.
        const bool passed =
            now < was ? (was < hit_at || now >= hit_at) : (was < hit_at && now >= hit_at);
        if (!passed) continue;
        if (tl::core::LevelScript* owner = script_owning(unit.node)) {
            owner->attack_hit(unit.node, quest_log.state(), events);
            if (owner == &script) {
                if (!events.empty()) apply_script_events(events, scripted_nodes);
            } else {
                for (size_t i = 0; i < nested_scripts.size(); ++i) {
                    // By index: the events can link a layout, a push_back that
                    // invalidates a deque's iterators.
                    NestedScript& machine = nested_scripts[i];
                    if (&machine.script != owner || events.empty()) continue;
                    apply_script_events(events, machine.nodes);
                }
            }
            events.clear();
        }
    }
}

/// Take the subject apart: the body, everything worn on it, its weapon and the particles
/// it carries (the character creation screen swaps bodies without a restart).
void Renderer::Impl::clear_subject() {
    // First: `fidgets` holds `Ogre::Entity*` keys and `Ogre::AnimationState*` values that
    // are about to be freed.
    fidgets.clear();
    // The level-up particle rides the player's node, like the unit it was made for.
    clear_level_up_particle();

    // The trails let go of the weapon's tag point and the skeleton's bones.
    player_trails.trails.hide_all();
    clear_worn_attachments();
    if (weapon_entity != nullptr && subject != nullptr) {
        subject->detachObjectFromBone(weapon_entity);
        scene->destroyEntity(weapon_entity);
    }
    weapon_entity = nullptr;
    has_weapon = false;
    clear_weapon_element();
    destroy_fishing_gear();
    fishing_state = tl::core::FishingState::Off;
    for (Ogre::SceneNode*& hand : hand_nodes) {
        if (hand != nullptr) scene->destroySceneNode(hand);
        hand = nullptr;
    }

    // The theme particles hang off the subject's node and bones.
    clear_unit_themes(player_themes);

    // The animations' particles and the model's own (equip_carried_particles),
    // with the nodes that follow their bones.
    clear_particle_triggers();

    for (Ogre::Entity* piece : equipment) {
        if (piece == nullptr) continue;
        if (Ogre::SceneNode* node = piece->getParentSceneNode()) node->detachObject(piece);
        scene->destroyEntity(piece);
    }
    equipment.clear();
    worn_pieces.fill(nullptr);
    worn_signature.clear();

    if (subject != nullptr) {
        if (Ogre::SceneNode* node = subject->getParentSceneNode()) node->detachObject(subject);
        scene->destroyEntity(subject);
        subject = nullptr;
    }

    linked_paths.clear();
    playing.clear();
    subject_dir.clear();
    player_stats = tl::core::UnitStats{};
    // The model's list and anything playing on it went with the entity.
    player_catalog.clear();
    player_catalog_mesh.clear();
    skill_playback = SkillPlayback{};
}

void Renderer::Impl::fire_particle_trigger(const std::string& animation, size_t index,
                                           const tl::core::AnimationTag& tag) {
    if (subject == nullptr || tag.layout.empty()) return;
    Ogre::SceneNode* model = subject->getParentSceneNode();
    if (model == nullptr) return;

    ParticleTrigger* trigger = nullptr;
    for (ParticleTrigger& candidate : particle_triggers) {
        if (candidate.animation == animation && candidate.tag == index) trigger = &candidate;
    }
    const Ogre::Bone* bone = nullptr;
    if (subject->hasSkeleton() && !tag.bone.empty() && subject->getSkeleton()->hasBone(tag.bone)) {
        bone = subject->getSkeleton()->getBone(tag.bone);
    }

    if (trigger == nullptr) {
        ParticleTrigger made;
        made.animation = animation;
        made.tag = index;
        made.bone = tag.bone;
        made.offset = Ogre::Vector3(tag.offset_x, tag.offset_y, tag.offset_z);
        made.attaches = tag.attaches;
        made.stop_on_death = tag.name == "SPAWNPARTICLE_STOP_ON_DEATH";
        // `configureParticleTrigger`: an attached
        // trigger's particle follows the bone, else the character.
        if (made.attaches) made.follower = model->createChildSceneNode();
        std::string reference = tag.layout;
        for (char& c : reference) {
            if (c == '\\') c = '/';
        }
        // Started below, with every later key's start.
        made.holders =
            place_particle_layout(reference, Ogre::Vector3::ZERO, Ogre::Quaternion::IDENTITY,
                                  made.follower, ParticleStart::Held);
        for (Ogre::SceneNode* holder : made.holders) made.inside.push_back(holder->getPosition());
        particle_triggers.push_back(std::move(made));
        trigger = &particle_triggers.back();
        update_particle_triggers();
        TL_LOG_INFO("animation particle: '%s' key %zu releases %s on '%s'%s (%zu system node(s))",
                    animation.c_str(), index, reference.c_str(), tag.bone.c_str(),
                    trigger->attaches ? ", attached" : "", trigger->holders.size());
    }

    // `CCharacter::updateAnimation`, types 4 and 5: an
    // attached particle at the key's offset under its bone's node, else placed in the world;
    // then `CParticle::Start`.
    Ogre::Vector3 at = trigger->offset;
    if (!(trigger->attaches && trigger->follower != nullptr)) {
        const Ogre::Vector3 local =
            (bone != nullptr ? bone->_getDerivedPosition() : Ogre::Vector3::ZERO) + trigger->offset;
        at = model->_getFullTransform() * local;
    }
    for (size_t i = 0; i < trigger->holders.size(); ++i) {
        trigger->holders[i]->setPosition(at + trigger->inside[i]);
    }
    std::set<uint32_t> started;
    for (ParticleSystem& system : particle_systems) {
        Ogre::SceneNode* holder = system.holder();
        if (std::find(trigger->holders.begin(), trigger->holders.end(), holder) ==
            trigger->holders.end()) {
            continue;
        }
        restart_particle_system(system);
        if (system.cparticle != 0) started.insert(system.cparticle);
    }
    for (const uint32_t id : started) cparticle_started(id);
}

// PORT: the player's hand nodes and particle followers moved onto their bones.
void Renderer::Impl::update_particle_triggers() {
    if (subject == nullptr || !subject->hasSkeleton()) return;
    // The hand nodes (+0x2e8, +0x2f8): `getBones`' bone, `tag_` first, else `Bip01`.
    static const char* const kHandBones[2][2] = {{"tag_righthand", "Bip01 R Hand"},
                                                 {"tag_lefthand", "Bip01 L Hand"}};
    for (int hand = 0; hand < 2; ++hand) {
        if (hand_nodes[hand] == nullptr) continue;
        const Ogre::Bone* bone = nullptr;
        for (const char* name : kHandBones[hand]) {
            if (bone == nullptr && subject->getSkeleton()->hasBone(name)) {
                bone = subject->getSkeleton()->getBone(name);
            }
        }
        if (bone == nullptr) continue;
        hand_nodes[hand]->setPosition(bone->_getDerivedPosition());
        hand_nodes[hand]->setOrientation(bone->_getDerivedOrientation());
    }
    for (ParticleTrigger& trigger : particle_triggers) {
        if (trigger.follower == nullptr || !subject->getSkeleton()->hasBone(trigger.bone)) continue;
        const Ogre::Bone* bone = subject->getSkeleton()->getBone(trigger.bone);
        trigger.follower->setPosition(bone->_getDerivedPosition());
        trigger.follower->setOrientation(bone->_getDerivedOrientation());
    }
}

// PORT: the player's particle triggers destroyed.
void Renderer::Impl::clear_particle_triggers() {
    for (ParticleTrigger& trigger : particle_triggers) {
        destroy_particle_holders(trigger.holders);
        if (trigger.follower != nullptr) scene->destroySceneNode(trigger.follower);
    }
    particle_triggers.clear();
}

size_t Renderer::Impl::equip_carried_particles() {
    if (subject == nullptr || !subject->getMesh()) return 0;

    const std::string mesh_path = subject->getMesh()->getName();
    const tl::core::AnimationManifest& manifest = manifest_for(mesh_path);
    if (manifest.empty()) return 0;

    // The manifest keys on the skeleton's stem, which is the mesh's.
    std::string stem = mesh_path;
    const size_t slash = stem.find_last_of('/');
    if (slash != std::string::npos) stem = stem.substr(slash + 1);
    const size_t dot = stem.find_last_of('.');
    if (dot != std::string::npos) stem.resize(dot);

    size_t released = 0;
    for (const tl::core::AnimationTag& tag : manifest.tags_for(stem)) {
        if (tag.name != "SPAWNPARTICLE" && tag.name != "SPAWNPARTICLE_STOP_ON_DEATH") continue;
        if (tag.layout.empty() || tag.bone.empty()) continue;
        // A `UNITTHEME` key runs only while the unit has the theme (`unitThemeUpdated`).
        //
        if (!tag.unit_theme.empty() && player_theme_names.count(tag.unit_theme) == 0) continue;

        // The manifest spells its paths with backslashes.
        std::string reference = tag.layout;
        for (char& c : reference) {
            if (c == '\\') c = '/';
        }

        if (!subject->hasSkeleton() || !subject->getSkeleton()->hasBone(tag.bone)) {
            TL_LOG_WARN("carried particle '%s' wants bone '%s', which this rig has not",
                        reference.c_str(), tag.bone.c_str());
            continue;
        }

        // The model's own keys are triggers (`CCharacter::loadModel`),
        // each hung by `configureParticleTrigger`
        // under a node following its bone (`updateBonePositions`).
        // Not a TagPoint: ParticleUniverse places from the entity's node.
        Ogre::SceneNode* model = subject->getParentSceneNode();
        if (model == nullptr) continue;
        const size_t index = static_cast<size_t>(&tag - manifest.tags_for(stem).data());
        const bool placed =
            std::any_of(particle_triggers.begin(), particle_triggers.end(),
                        [&](const ParticleTrigger& trigger) {
                            return trigger.animation == stem && trigger.tag == index;
                        });
        if (placed) continue;
        ParticleTrigger made;
        made.animation = stem;
        made.tag = index;
        made.bone = tag.bone;
        made.offset = Ogre::Vector3(tag.offset_x, tag.offset_y, tag.offset_z);
        made.stop_on_death = tag.name == "SPAWNPARTICLE_STOP_ON_DEATH";
        made.unit_theme = tag.unit_theme;
        made.follower = model->createChildSceneNode();
        const size_t before = particle_systems.size();
        made.holders = place_particle_layout(reference, Ogre::Vector3::ZERO,
                                             Ogre::Quaternion::IDENTITY, made.follower);
        for (Ogre::SceneNode* holder : made.holders) {
            made.inside.push_back(holder->getPosition());
            holder->setPosition(made.offset + holder->getPosition());
        }
        released += particle_systems.size() - before;
        particle_triggers.push_back(std::move(made));
        TL_LOG_INFO("carried particle: %s on '%s' (%zu system(s))", reference.c_str(),
                    tag.bone.c_str(), particle_systems.size() - before);
    }
    if (released > 0) update_particle_triggers();
    return released;
}

bool Renderer::Impl::equip_starting_weapon() {
    if (!subject || !subject->hasSkeleton()) return false;
    if (player_stats.right_hand.empty() && player_stats.left_hand.empty()) return false;

    ensure_item_index();

    const bool right = !player_stats.right_hand.empty();
    const std::string wanted = right ? player_stats.right_hand : player_stats.left_hand;
    // The copy the inventory made of it, when it holds one in a hand.
    for (const size_t hand : {static_cast<size_t>(tl::core::equip_location::kRightHand),
                              static_cast<size_t>(tl::core::equip_location::kLeftHand)}) {
        const tl::core::ItemStats* held = player_inventory.item_in_slot(hand);
        if (held != nullptr && held->name == wanted) return equip_weapon_item(*held);
    }
    const tl::core::ItemStats* item = items.find(wanted);
    if (item == nullptr) {
        TL_LOG_WARN(
            "the character is meant to hold '%s' and nothing in the archive is called "
            "that",
            wanted.c_str());
        return false;
    }
    // No inventory yet (the menu's): a copy as `createEquipmentAndEquip`
    // makes one.
    return equip_weapon_item(create_item(*item, false, false));
}

// PORT: a level unit's rolled hands equipped on its OGRE entity once its machine is built.
void Renderer::Impl::sync_unit_equipment() {
    for (auto& entry : scene_units) {
        SceneUnit& drawn = entry.second;
        if (drawn.equipment_synced || drawn.entity == nullptr) continue;
        const tl::core::UnitStats stats = stats_for_unit(entry.first);
        if (stats.spawn_right_hand.empty() && stats.spawn_left_hand.empty()) {
            // Nothing rolled: what it holds went on as it was placed.
            drawn.equipment_synced = true;
            continue;
        }
        const Ogre::SceneNode* parent = drawn.entity->getParentSceneNode();
        for (MachineView& view : machines()) {
            NodeMap* nodes = nodes_of(view.script);
            if (nodes == nullptr) continue;
            bool mine = false;
            const auto range = nodes->equal_range(drawn.node);
            for (auto it = range.first; it != range.second; ++it)
                mine = mine || it->second == parent;
            std::string right, left;
            // Not made yet (a layout's script is built after its meshes): later.
            if (!mine || !view.script->unit_hands(drawn.node, right, left)) continue;
            drawn.equipment_synced = true;
            equip_unit_items(drawn.entity, entry.first, stats.right_hand.empty() ? right : "",
                             stats.left_hand.empty() ? left : "");
            break;
        }
    }
}

void Renderer::Impl::equip_unit_items(Ogre::Entity* entity, const std::string& unit,
                                      const std::string& right_item, const std::string& left_item) {
    if (entity == nullptr || !entity->hasSkeleton() || unit.empty()) return;
    if (right_item.empty() && left_item.empty()) return;
    const tl::core::UnitStats stats = stats_for_unit(unit);
    ensure_item_index();
    const std::string unit_dir = dir_of_asset(entity->getMesh()->getName());
    for (const bool right : {true, false}) {
        const std::string& wanted = right ? right_item : left_item;
        if (wanted.empty()) continue;
        const tl::core::ItemStats* item = items.find(wanted);
        if (item == nullptr || item->mesh.empty()) {
            TL_LOG_WARN("%s: item '%s' %s", unit.c_str(), wanted.c_str(),
                        item == nullptr ? "is not in the item index" : "names no mesh");
            continue;
        }
        // `CEquipment::attachToGivenLocation` at location
        // 0 or 1: the hand's bone, a shield's `tag_leftarm`, nothing for a wardrobe item,
        // the wearer's `WEAPON_SCALE` or `SHIELD_SCALE`.
        load_unit_types();
        const tl::core::EquipmentAttachment where = tl::core::equipment_attachment(
            *item, right ? 0 : 1, unit, stats.weapon_scale, stats.shield_scale, unit_types);
        if (!where.attach) continue;
        const char* bone = where.bone.c_str();
        if (!entity->getSkeleton()->hasBone(bone)) {
            TL_LOG_WARN("%s: no '%s' bone for '%s'", unit.c_str(), bone, wanted.c_str());
            continue;
        }
        const std::string dir = dir_of_asset(item->mesh);
        std::string stem = item->mesh;
        const size_t slash = stem.find_last_of('/');
        if (slash != std::string::npos) stem = stem.substr(slash + 1);
        // The item's directory is matched without case (`Shields` against `shields`).
        std::string path;
        if (const std::vector<Ogre::FileInfo>* found = archive_entries(stem + ".MESH")) {
            for (const Ogre::FileInfo& info : *found) {
                if (lowered(info.path) == lowered(dir)) path = info.path + info.basename;
            }
        }
        if (path.empty()) {
            TL_LOG_WARN("%s: no '%s.MESH' under '%s'", unit.c_str(), stem.c_str(), dir.c_str());
            continue;
        }
        parse_sibling_material(path);
        Ogre::MeshPtr mesh;
        try {
            mesh = Ogre::MeshManager::getSingleton().load(path, kGameGroup);
        } catch (const Ogre::Exception& e) {
            TL_LOG_WARN("could not load '%s': %s", path.c_str(), e.getDescription().c_str());
            continue;
        }
        auto& materials = Ogre::MaterialManager::getSingleton();
        for (const Ogre::SubMesh* sub : mesh->getSubMeshes()) {
            qualify_texture_paths(materials.getByName(sub->getMaterialName(), kGameGroup), dir,
                                  unit_dir);
        }
        Ogre::Entity* held = scene->createEntity(mesh);
        rebind_materials(held);
        // `CCharacter::setRimlight` lights the equipped items too
        // (`CEquipment` vtable +0x150).
        apply_rim_light(held, rim_light_texture);
        Ogre::TagPoint* tag = entity->attachObjectToBone(bone, held);
        if (const RenderBehind* behind = render_behind_of(entity)) {
            set_render_behind(held, behind->on, behind->hostile);  // `CEquipment::setRenderBehind`
        }
        // What the weapon trails ask of the hands (`getEquipmentEquippedAt`).
        UnitHeldItems& hands = unit_held_items[entity];
        hands.held[right ? 0 : 1] = held;
        hands.unittype[right ? 0 : 1] = item->unittype;
        const float scale = where.scale;
        tag->setScale(scale, scale, scale);
        TL_LOG_INFO("%s holds %s in the %s hand (on '%s', scale %.2f)", unit.c_str(),
                    item->display_name.c_str(), right ? "right" : "left", bone,
                    static_cast<double>(scale));
        // The hand's node is `getBones`', scaled by `WEAPON_SCALE` whatever the
        // item is.
        attach_unit_weapon_element(entity, *item, bone,
                                   stats.weapon_scale > 0.0f ? stats.weapon_scale : 1.0f);
    }
}

void Renderer::Impl::attach_unit_weapon_element(Ogre::Entity* entity,
                                                const tl::core::ItemStats& item,
                                                const std::string& bone, float scale) {
    Ogre::SceneNode* model = entity->getParentSceneNode();
    if (model == nullptr) return;
    // `createParticles`: only a weapon (ISA 8) carries an
    // element.
    load_unit_types();
    const uint32_t type = unit_types.id_of(item.unittype);
    if (!unit_types.isa(type, tl::core::unit_type::kWeapon)) return;
    const tl::core::EffectManager::Context& effects = item_effect_context();
    // INVENTED: a placed unit's hands are item names, not `CEquipment` instances
    // (`createDefaultEquipment`); the element's share is asked of
    // the item's `DAMAGE_<TYPE>` split and its authored effects (`createElementalDamages`).
    tl::core::ItemStats held = item;
    held.damage_types.clear();
    held.damage_bonus = {};
    held.inherent_damage = {};
    for (const int element : {tl::core::kDamageElectric, tl::core::kDamageFire,
                              tl::core::kDamageIce, tl::core::kDamagePoison}) {
        const int share = held.damage_split[static_cast<size_t>(element)];
        if (0 < share) tl::core::add_inherent_damage(held, element, share);
    }
    tl::core::create_elemental_damages(held, effects, unit_types);
    const std::string name = tl::core::elemental_particle_name(
        unit_types.isa(type, tl::core::unit_type::kPistol),
        [&](int damage_type) { return tl::core::weapon_damage_bonus(held, damage_type, effects); });
    if (name.empty()) return;
    const std::string file = preloaded_particle_file(name);
    if (file.empty()) {
        TL_LOG_WARN("'%s': no particle '%s' in media/particles/preload.dat",
                    item.display_name.c_str(), name.c_str());
        return;
    }
    // `getBones`: the hand's node, a child of the unit's,
    // scaled by `WEAPON_SCALE`, on the weapon's bone.
    UnitWeaponElement made;
    made.entity = entity;
    made.bone = bone;
    made.hand = model->createChildSceneNode();
    made.hand->setScale(scale, scale, scale);
    const Ogre::Bone* hand_bone = entity->getSkeleton()->getBone(bone);
    made.hand->setPosition(hand_bone->_getDerivedPosition());
    made.hand->setOrientation(hand_bone->_getDerivedOrientation());
    made.holders =
        place_particle_layout(file, Ogre::Vector3::ZERO, Ogre::Quaternion::IDENTITY, made.hand);
    TL_LOG_INFO("  its %s carries %s: %s (%zu holder(s))", item.display_name.c_str(), name.c_str(),
                file.c_str(), made.holders.size());
    unit_weapon_elements.push_back(std::move(made));
}

// PORT: a unit's weapon particles destroyed with it.
void Renderer::Impl::clear_unit_weapon_elements(Ogre::Entity* entity) {
    for (size_t i = 0; i < unit_weapon_elements.size();) {
        UnitWeaponElement& held = unit_weapon_elements[i];
        const bool gone =
            entity != nullptr
                ? held.entity == entity
                : std::none_of(scene_units.begin(), scene_units.end(),
                               [&held](const auto& e) { return e.second.entity == held.entity; });
        if (!gone) {
            ++i;
            continue;
        }
        destroy_particle_holders(held.holders);
        if (held.hand != nullptr && scene != nullptr) scene->destroySceneNode(held.hand);
        unit_weapon_elements.erase(unit_weapon_elements.begin() + static_cast<long>(i));
    }
}

bool Renderer::Impl::equip_weapon_item(const tl::core::ItemStats& item, int location) {
    if (!subject || !subject->hasSkeleton()) return false;

    // A copy: its combat stats were rolled when it was made (`create_item`).
    weapon = item;
    has_weapon = true;

    // `CEquipment::attachToGivenLocation`: the
    // location's own bone, so a left-hand-only weapon is drawn in the left hand.
    const bool right = location >= 0 ? location == tl::core::equip_location::kRightHand
                                     : (lowered(weapon.unittype).find("bow") == std::string::npos);
    const tl::core::Hand hand = right ? tl::core::Hand::Right : tl::core::Hand::Left;
    attack_class = tl::core::attack_animation_class(weapon.unittype, hand);
    attack_location =
        right ? tl::core::equip_location::kRightHand : tl::core::equip_location::kLeftHand;

    const char* bone = right ? "tag_righthand" : "tag_lefthand";
    if (!subject->getSkeleton()->hasBone(bone)) {
        TL_LOG_WARN("no '%s' bone on the character; '%s' cannot be held", bone,
                    weapon.display_name.c_str());
        return false;
    }

    if (weapon.mesh.empty()) {
        TL_LOG_WARN("'%s' names no mesh", weapon.display_name.c_str());
        return false;
    }
    const std::string dir = dir_of_asset(weapon.mesh);
    std::string stem = weapon.mesh;
    const size_t slash = stem.find_last_of('/');
    if (slash != std::string::npos) stem = stem.substr(slash + 1);
    const std::string path = archive_path_in_dir(stem + ".MESH", dir);
    if (path.empty()) {
        TL_LOG_WARN("no '%s.MESH' under '%s'", stem.c_str(), dir.c_str());
        return false;
    }

    parse_sibling_material(path);

    Ogre::MeshPtr mesh;
    try {
        mesh = Ogre::MeshManager::getSingleton().load(path, kGameGroup);
    } catch (const Ogre::Exception& e) {
        TL_LOG_WARN("could not load '%s': %s", path.c_str(), e.getDescription().c_str());
        return false;
    }
    auto& materials = Ogre::MaterialManager::getSingleton();
    for (const Ogre::SubMesh* sub : mesh->getSubMeshes()) {
        qualify_texture_paths(materials.getByName(sub->getMaterialName(), kGameGroup), dir,
                              subject_dir);
    }

    if (weapon_entity != nullptr) {
        // `CCharacter::equipmentUnequipped`:
        // a weapon (ISA 8) going puts the hands' trails off.
        player_trails.trails.hide_hands();
        subject->detachObjectFromBone(weapon_entity);
        scene->destroyEntity(weapon_entity);
        weapon_entity = nullptr;
    }
    weapon_entity = scene->createEntity(mesh);
    rebind_materials(weapon_entity);
    // `CCharacter::setRimlight` hands its texture to every equipped
    // item's `setRimlight` (`CEquipment` vtable +0x150).
    apply_rim_light(weapon_entity, rim_light_texture);
    Ogre::TagPoint* tag = subject->attachObjectToBone(bone, weapon_entity);
    // `CCharacter::equipmentEquipped`: `setRenderBehind(+0x4a1)`,
    // which reaches the new item (`CEquipment::setRenderBehind`).
    if (const RenderBehind* behind = render_behind_of(subject)) {
        set_render_behind(weapon_entity, behind->on, behind->hostile);
    }
    const float scale = player_stats.weapon_scale > 0.0f ? player_stats.weapon_scale : 1.0f;
    tag->setScale(scale, scale, scale);

    TL_LOG_INFO("holding %s (%s) in the %s hand, %s at %.2f scale", weapon.display_name.c_str(),
                weapon.unittype.c_str(), right ? "right" : "left", path.c_str(),
                static_cast<double>(scale));
    TL_LOG_INFO("  swings on '%s', %.0f-%.0f%% of BASE_WEAPON_DAMAGE at level %d",
                attack_class.c_str(), static_cast<double>(weapon.min_damage),
                static_cast<double>(weapon.max_damage), weapon.level);
    attach_weapon_element(right);
    return true;
}

void Renderer::Impl::attach_weapon_element(bool right) {
    clear_weapon_element();
    if (subject == nullptr || !subject->hasSkeleton()) return;
    Ogre::SceneNode* model = subject->getParentSceneNode();
    if (model == nullptr) return;

    // `createParticles`, its second half: only a weapon
    // (ISA 8) carries an element; the drop particle (no owner, +0x290) is not for a hand.
    load_unit_types();
    const uint32_t type = unit_types.id_of(weapon.unittype);
    if (!unit_types.isa(type, tl::core::unit_type::kWeapon)) return;
    const tl::core::EffectManager::Context& effects = item_effect_context();
    const std::string name = tl::core::elemental_particle_name(
        unit_types.isa(type, tl::core::unit_type::kPistol), [&](int damage_type) {
            return tl::core::weapon_damage_bonus(weapon, damage_type, effects);
        });
    if (name.empty()) return;
    const std::string file = preloaded_particle_file(name);
    if (file.empty()) {
        TL_LOG_WARN("'%s': no particle '%s' in media/particles/preload.dat",
                    weapon.display_name.c_str(), name.c_str());
        return;
    }

    // `getBones`: the hand's node, a child of
    // the unit's, scaled by `WEAPON_SCALE`.
    const int hand = right ? 0 : 1;
    if (hand_nodes[hand] == nullptr) {
        hand_nodes[hand] = model->createChildSceneNode();
        const float scale = player_stats.weapon_scale > 0.0f ? player_stats.weapon_scale : 1.0f;
        hand_nodes[hand]->setScale(scale, scale, scale);
    }
    // `attachToGivenLocation`, locations 0 and 1: the
    // particle's node moved under the hand's, at `createParticles`' origin.
    weapon_element_hand = hand;
    weapon_element_particles = place_particle_layout(file, Ogre::Vector3::ZERO,
                                                     Ogre::Quaternion::IDENTITY, hand_nodes[hand]);
    update_particle_triggers();
    TL_LOG_INFO("  carries %s: %s on the %s hand (%zu holder(s))", name.c_str(), file.c_str(),
                right ? "right" : "left", weapon_element_particles.size());
}

void Renderer::Impl::clear_weapon_element() {
    destroy_particle_holders(weapon_element_particles);
    weapon_element_particles.clear();
}

void Renderer::Impl::unequip_weapon() {
    clear_weapon_element();
    // `CCharacter::equipmentUnequipped`, for a weapon.
    player_trails.trails.hide_hands();
    if (weapon_entity != nullptr && subject != nullptr) {
        subject->detachObjectFromBone(weapon_entity);
        scene->destroyEntity(weapon_entity);
        weapon_entity = nullptr;
    }
    has_weapon = false;
    attack_location = -1;
    attack_class = "Attack";
}

// PORT: the player's held items as `HeldHands` reads them (`calculateCombatStats`).
tl::core::HeldHands Renderer::Impl::player_held_hands() {
    load_unit_types();
    tl::core::HeldHands hands;
    hands.using_weapon = has_weapon;
    const auto& slots = player_inventory.slots();
    const auto read = [&](int location, tl::core::HeldHand& hand) {
        if (static_cast<size_t>(location) >= slots.size() || slots[location].count <= 0) return;
        const tl::core::ItemStats& item = slots[static_cast<size_t>(location)].item;
        const uint32_t type = unit_types.id_of(item.unittype);
        hand.held = true;
        hand.weapon = unit_types.isa(type, tl::core::unit_type::kWeapon);
        hand.ranged = unit_types.isa(type, tl::core::unit_type::kRangedWeapon);
        hand.melee = unit_types.isa(type, tl::core::unit_type::kMelee);
        // `calculateCombatStats` gives either hand's attack the item's own `RANGE`.
        hand.range = item.range;
    };
    read(tl::core::equip_location::kRightHand, hands.right);
    read(tl::core::equip_location::kLeftHand, hands.left);
    return hands;
}

void Renderer::Impl::select_player_attack(int mode) {
    // the player's attack with more than 0.2 s of swing left is kept.
    if (swing.active && 0.2f < (swing.length - swing.elapsed) / swing_rate()) return;
    const tl::core::HeldHands hands = player_held_hands();
    const tl::core::AttackHand chosen = tl::core::select_attack_hand(hands, mode, alternate_hand);
    if (chosen == tl::core::AttackHand::Own) {
        if (has_weapon) TL_LOG_INFO("the character attacks bare-handed");
        has_weapon = false;
        attack_location = -1;
        attack_class = "Attack";
        return;
    }
    const bool right = chosen == tl::core::AttackHand::Right;
    const int location =
        right ? tl::core::equip_location::kRightHand : tl::core::equip_location::kLeftHand;
    const tl::core::ItemStats& item = player_inventory.slots()[static_cast<size_t>(location)].item;
    if (has_weapon && attack_location == location && weapon.name == item.name) return;
    weapon = item;
    has_weapon = true;
    attack_location = location;
    attack_class = tl::core::attack_animation_class(
        weapon.unittype, right ? tl::core::Hand::Right : tl::core::Hand::Left);
    TL_LOG_INFO("the character attacks with %s in the %s hand ('%s')", weapon.display_name.c_str(),
                right ? "right" : "left", attack_class.c_str());
}

void Renderer::Impl::choose_player_attack_hand() {
    const tl::core::HeldHands hands = player_held_hands();
    switch (tl::core::attack_hand_step(hands)) {
        case tl::core::AttackHandStep::SelectAny:
            select_player_attack(0);
            return;
        case tl::core::AttackHandStep::Keep: return;  // equal ranges, nothing selected
        case tl::core::AttackHandStep::Mixed: break;
    }
    // the attack in use as `selectAttack(0)` left it.
    tl::core::AttackDescription attack =
        has_weapon ? tl::core::AttackDescription{}
                   : tl::core::own_attack(player_stats, player_stats.level, config.difficulty,
                                          /*player=*/true, stat_graphs);
    if (has_weapon) attack.range = weapon.range;
    const float ranged = tl::core::ranged_range(player_stats, attack, hands);
    const float melee = tl::core::melee_range(player_stats, attack, hands);
    if (melee == ranged) return;
    // with no melee weapon arc, the ranged hand (`selectAttack(2)`);
    // else the melee hand when a foe stands within its reach plus a metre, in the weapon's
    // arc from 1.5 out (`findCharacterWithinView`), else the ranged hand.
    load_unit_types();
    const bool right = tl::core::attack_melee_hand_is_right(hands);
    const tl::core::ItemStats& held =
        player_inventory
            .slots()[static_cast<size_t>(right ? tl::core::equip_location::kRightHand
                                               : tl::core::equip_location::kLeftHand)]
            .item;
    const uint32_t type = unit_types.id_of(held.unittype);
    const float arc =
        tl::core::console_weapon_arc(unit_types.isa(type, tl::core::unit_type::kSword),
                                     unit_types.isa(type, tl::core::unit_type::kAxe),
                                     unit_types.isa(type, tl::core::unit_type::kMace),
                                     unit_types.isa(type, tl::core::unit_type::kPolearm),
                                     unit_types.isa(type, tl::core::unit_type::kStaff));
    int mode = 2;
    if (arc != 0.0f && subject_node != nullptr) {
        ViewUnit found;
        if (find_character_within_view(player.position(), subject_node->getOrientation(), 1.5f, arc,
                                       melee + 1.0f, /*alignment=*/5, /*nearest=*/true, ViewUnit{},
                                       found)) {
            mode = 1;
        }
    }
    select_player_attack(mode);
}

void Renderer::Impl::sync_equipped_weapon() {
    // The right hand (`equip_location::kRightHand`, slot 0), then the left (`kLeftHand`):
    // the two locations `CInventory` lets a `Weapon` into.
    const auto& slots = player_inventory.slots();
    const tl::core::ItemStats* eq_weapon = nullptr;
    int weapon_location = -1;
    if (slots[tl::core::equip_location::kRightHand].count > 0) {
        eq_weapon = &slots[tl::core::equip_location::kRightHand].item;
        weapon_location = tl::core::equip_location::kRightHand;
    } else if (slots[tl::core::equip_location::kLeftHand].count > 0) {
        eq_weapon = &slots[tl::core::equip_location::kLeftHand].item;
        weapon_location = tl::core::equip_location::kLeftHand;
    }

    if (eq_weapon != nullptr) {
        equip_weapon_item(*eq_weapon, weapon_location);
    } else {
        unequip_weapon();
    }
    sync_worn_attachments(weapon_location);
    // `CCharacter::updateAI`'s `selectAttack(0)` settles the hand.
    select_player_attack(0);
    // `CCharacter::equipmentEquipped` / `equipmentUnequipped`
    // call `CWardrobe::update`.
    dress_player();
}

void Renderer::Impl::clear_worn_attachments() {
    for (Ogre::Entity* worn : worn_attachments) {
        if (subject != nullptr) subject->detachObjectFromBone(worn);
        if (scene != nullptr) scene->destroyEntity(worn);
    }
    worn_attachments.clear();
}

void Renderer::Impl::sync_worn_attachments(int weapon_location) {
    // `CEquipment::equipped` runs `CEquipment::attachToGivenLocation`
    // per item put on, unless the wardrobe draws it; the
    // held weapon is `equip_weapon_item`'s. `detachFromLocation` undoes it.
    clear_worn_attachments();
    worn_left_hand = nullptr;
    if (subject == nullptr || !subject->hasSkeleton()) return;
    load_unit_types();
    const auto& slots = player_inventory.slots();
    for (int location = 0; location < 12; ++location) {
        if (location == weapon_location) continue;
        if (static_cast<size_t>(location) >= slots.size() || slots[location].count <= 0) continue;
        const tl::core::ItemStats& item = slots[static_cast<size_t>(location)].item;
        const tl::core::EquipmentAttachment where = tl::core::equipment_attachment(
            item, location, class_profile.name, player_stats.weapon_scale,
            player_stats.shield_scale, unit_types);
        if (!where.attach) continue;
        if (!subject->getSkeleton()->hasBone(where.bone)) {
            TL_LOG_WARN("no '%s' bone on the character for '%s'", where.bone.c_str(),
                        item.display_name.c_str());
            continue;
        }
        Ogre::Entity* worn = create_item_entity(item);
        if (worn == nullptr) continue;
        apply_rim_light(worn, rim_light_texture);  // `CCharacter::setRimlight`, as the weapon
        Ogre::TagPoint* tag = subject->attachObjectToBone(where.bone, worn);
        if (const RenderBehind* behind = render_behind_of(subject)) {
            set_render_behind(worn, behind->on, behind->hostile);
        }
        tag->setScale(where.scale, where.scale, where.scale);
        worn_attachments.push_back(worn);
        if (location == tl::core::equip_location::kLeftHand) worn_left_hand = worn;
        TL_LOG_INFO("the character wears %s on '%s' (location %d, scale %.2f)",
                    item.display_name.c_str(), where.bone.c_str(), location,
                    static_cast<double>(where.scale));
    }
}

// PORT: an item's mesh loaded and its OGRE entity made.
Ogre::Entity* Renderer::Impl::create_item_entity(const tl::core::ItemStats& item) {
    if (item.mesh.empty() || scene == nullptr) return nullptr;

    const std::string dir = dir_of_asset(item.mesh);
    std::string stem = item.mesh;
    const size_t slash = stem.find_last_of('/');
    if (slash != std::string::npos) stem = stem.substr(slash + 1);
    const std::string path = archive_path_in_dir(stem + ".MESH", dir);
    if (path.empty()) {
        TL_LOG_WARN("item '%s' names no mesh under '%s'", item.display_name.c_str(), dir.c_str());
        return nullptr;
    }

    parse_sibling_material(path);
    Ogre::MeshPtr mesh;
    try {
        mesh = Ogre::MeshManager::getSingleton().load(path, kGameGroup);
    } catch (const Ogre::Exception& e) {
        TL_LOG_WARN("could not load item mesh '%s': %s", path.c_str(), e.getDescription().c_str());
        return nullptr;
    }
    auto& materials = Ogre::MaterialManager::getSingleton();
    for (const Ogre::SubMesh* sub : mesh->getSubMeshes()) {
        qualify_texture_paths(materials.getByName(sub->getMaterialName(), kGameGroup), dir, dir);
    }

    Ogre::Entity* entity = scene->createEntity(mesh);
    rebind_materials(entity);
    return entity;
}

/// The Y legend's "Drop": the item leaves from where the character stands. INVENTED: the
/// caller setting the position (`+0x84`) before `drop` was not read.
void Renderer::Impl::drop_item_near_player(const tl::core::ItemStats& item, int count) {
    if (subject_node == nullptr) return;
    drop_item(item, count, subject_node->getPosition());
}

/// A mesh-less item (no item index entry) is not dropped.
void Renderer::Impl::drop_item(const tl::core::ItemStats& item, int count,
                               const Ogre::Vector3& placed) {
    Ogre::Entity* entity = create_item_entity(item);
    if (entity == nullptr) return;

    // `addItem`: `setPosition`, then `CEquipment::setActiveInLevel`
    // grounds it before `drop` takes its position.
    const Ogre::Vector3 at = snap_item_to_ground(placed);

    // `CEquipment::drop`. Draws on `g_RandVolatile`, in
    // order: heading, landing spot, the arc's rise.
    load_unit_types();
    const uint32_t type = unit_types.id_of(item.unittype);
    const float heading = tl::core::volatile_random().between(0.0f, 360.0f);
    const tl::core::DropTiltRotation tilt =
        tl::core::drop_tilt_rotation(tl::core::drop_tilt(unit_types, type));
    Ogre::Quaternion tilted = Ogre::Quaternion::IDENTITY;
    if (tilt.axis == 0)
        tilted = Ogre::Quaternion(Ogre::Radian(tilt.radians), Ogre::Vector3::UNIT_X);
    if (tilt.axis == 2)
        tilted = Ogre::Quaternion(Ogre::Radian(tilt.radians), Ogre::Vector3::UNIT_Z);
    // `matrixRotationY(heading)` times the tilt: laid down, then turned.
    const Ogre::Quaternion rest =
        Ogre::Quaternion(Ogre::Radian(heading * 0.017453292f), Ogre::Vector3::UNIT_Y) * tilted;

    const tl::core::Vec3 from{at.x, at.y, at.z};
    play_sound(item.fall_sound, at);  // slot 0x10, before the spot is chosen

    tl::core::Vec3 land = tl::core::random_open_item_position(
        from, 0.0f, tl::core::kDropTossRadius,
        [&](const tl::core::Vec3& p) {
            return !passability.ready() || passability.map_passable(p);
        },
        tl::core::volatile_random());
    // `CLevel::floorHeight`: a ray from 400 down to -100,
    // 0 when it meets nothing.
    const float floor = collision.floor_height(land.x, land.z);
    land.y = floor + tl::core::kDropFloorLift;
    if (unit_types.isa(type, tl::core::unit_type::kWeapon))
        land.y = land.y + tl::core::kDropWeaponLift;

    DroppedWorldItem dropped;
    dropped.item = item;
    dropped.count = count;
    dropped.entity = entity;
    dropped.arc = tl::core::make_drop_arc(from, land, tl::core::volatile_random());
    dropped.flying = true;
    dropped.travelled = 0.0f;
    dropped.spins = tl::core::drop_spins(unit_types, type);
    dropped.rest = rest;

    Ogre::SceneNode* node = scene->getRootSceneNode()->createChildSceneNode(at, rest);
    node->attachObject(entity);
    dropped.node = node;
    // `CLevel::addItem` puts it in the level before `drop` tosses it, so its particle
    // rides the toss.
    start_item_particles(dropped);
    // `addItem`: `+0x204` to 1 (`updateOpacity(0, true)`).
    dropped.presence.opacity = 1.0f;
    dropped_items.push_back(dropped);
    TL_LOG_INFO("dropped %d x %s: tossed from (%.2f, %.2f, %.2f) to (%.2f, %.2f, %.2f), %.2f along",
                count, item.display_name.c_str(), static_cast<double>(from.x),
                static_cast<double>(from.y), static_cast<double>(from.z),
                static_cast<double>(land.x), static_cast<double>(land.y),
                static_cast<double>(land.z), static_cast<double>(dropped.arc.length));
}

Ogre::Vector3 Renderer::Impl::snap_item_to_ground(const Ogre::Vector3& at) {
    // `CItem::snapToGround`: a ray from 100 above zero to
    // 100 below (`CLevel::rayCollision`); a hit not on `nocollide` (material 100) is the
    // spot. Else `randomOpenPosition(here, 4)` and again, thirty times, then the last point.
    const tl::core::Vec3 rest = tl::core::snap_item_to_ground(
        {at.x, at.y, at.z},
        [&](const tl::core::Vec3& from, const tl::core::Vec3& to, tl::core::Vec3& point,
            uint32_t& material) {
            tl::core::CollisionHit hit;
            if (!collision.ray_collision(from, to, false, hit)) return false;
            point = hit.point;
            material = static_cast<uint32_t>(hit.material);
            return true;
        },
        [&](const tl::core::Vec3& p) {
            return !passability.ready() ||
                   (passability.map_passable(p) && passability.position_passable(p));
        },
        tl::core::volatile_random());
    return Ogre::Vector3(rest.x, rest.y, rest.z);
}

void Renderer::Impl::place_gold(const tl::core::GoldPile& pile, const Ogre::Vector3& placed,
                                const tl::core::ItemStats* data, bool drop_sound) {
    if (scene == nullptr) return;
    // `CItemGold::loadModel` with `media/models/gold/gold.MESH`.
    tl::core::ItemStats item;
    if (data != nullptr) item = *data;
    item.mesh = "media/models/gold/gold";
    // `CItemGold +0x210`: the amount, a space, the translated `Gold`.
    item.display_name = std::to_string(pile.amount) + " Gold";
    Ogre::Entity* entity = create_item_entity(item);
    if (entity == nullptr) return;

    // `CItemGold::setActiveInLevel`: grounded, sparkling.
    const Ogre::Vector3 at = snap_item_to_ground(placed);
    const Ogre::Quaternion rest(Ogre::Radian(pile.heading * 0.017453292f), Ogre::Vector3::UNIT_Y);
    Ogre::SceneNode* node = scene->getRootSceneNode()->createChildSceneNode(at, rest);
    node->attachObject(entity);
    // `setScale(0.01)`; `CItemGold::update` grows it.
    node->setScale(0.01f, 0.01f, 0.01f);

    DroppedWorldItem dropped;
    dropped.item = item;
    dropped.count = 1;
    dropped.entity = entity;
    dropped.node = node;
    dropped.rest = rest;
    dropped.is_gold = true;
    dropped.pile = pile;
    dropped.grow_left = 0.35f;
    dropped.presence.opacity = 1.0f;  // `addItem`
    const std::string sparkle = preloaded_particle_file("GOLD_SPARKLE");
    if (!sparkle.empty()) {
        dropped.sparkle = place_particle_layout(sparkle, at, Ogre::Quaternion::IDENTITY, nullptr);
    }
    dropped_items.push_back(dropped);
    // `addItem` plays the drop sound and `rollMoney` again.
    if (drop_sound) play_sound("GoldPickup", at);
    TL_LOG_INFO("gold: %d on the ground at (%.2f, %.2f, %.2f)", pile.amount,
                static_cast<double>(at.x), static_cast<double>(at.y), static_cast<double>(at.z));
}

void Renderer::Impl::take_gold(size_t index) {
    if (index >= dropped_items.size() || !dropped_items[index].is_gold) return;
    const DroppedWorldItem dropped = dropped_items[index];
    const Ogre::Vector3 at = dropped.node->getPosition();
    // `CCharacter::getItem`: `CItemGold::playTakeSound`,
    // `giveGold`, the amount floating up,
    // `CLevel::deleteItem`.
    play_sound("GoldPickup", at);
    player_inventory.give_gold(dropped.pile.amount);
    // CCharacter::giveGold: incrementJournalStatistic(1, amount)
    if (dropped.pile.amount > 0)
        count_journal(tl::core::JournalStatistic::GoldGathered, dropped.pile.amount);
    add_combat_text(tl::core::CombatText::Gold, at, dropped.pile.amount);
    TL_LOG_INFO("gold: picked up %d, %d in the bag", dropped.pile.amount, player_inventory.gold());

    const int64_t gold_unit = dropped_items[index].unit_id;
    remove_ground_item(dropped_items[index]);
    dropped_items.erase(dropped_items.begin() + static_cast<long>(index));
    if (gold_unit != 0) spawned_item_state(gold_unit, 3);
    if (item_targeted == static_cast<int>(index)) {
        item_targeted = -1;
    } else if (item_targeted > static_cast<int>(index)) {
        --item_targeted;
    }
    update_menu();
    refresh_game_hud();
}

void Renderer::Impl::auto_pickup_gold() {
    // `CGameClient::autoPickupGold`, every frame: the
    // player alive and moving (`+0x264`), every gold within 3 across the floor.
    if (!player_control || player_health.dead || subject_node == nullptr) return;
    if (!player.moving() && !player.following()) return;
    const Ogre::Vector3 me = subject_node->getPosition();
    for (size_t i = 0; i < dropped_items.size();) {
        const DroppedWorldItem& dropped = dropped_items[i];
        if (dropped.is_gold && dropped.node != nullptr) {
            const Ogre::Vector3 there = dropped.node->getPosition();
            const float dx = there.x - me.x;
            const float dz = there.z - me.z;
            if (std::sqrt(dx * dx + 0.0f + dz * dz) < 3.0f) {
                take_gold(i);
                continue;
            }
        }
        ++i;
    }
}

void Renderer::Impl::place_ground_item(const tl::core::ItemStats& item, int count,
                                       const Ogre::Vector3& at, const Ogre::Quaternion& orientation,
                                       const tl::core::QuestUnitTag& tag) {
    Ogre::Entity* entity = create_item_entity(item);
    if (entity == nullptr) return;
    // `CItem::snapToGround`, which
    // `CQuestUnitData::createUnit`, `restoreLevelState`
    // and `setActiveInLevel` end an item with.
    const Ogre::Vector3 rest_at = snap_item_to_ground(at);

    DroppedWorldItem placed;
    placed.item = item;
    placed.count = count;
    placed.entity = entity;
    placed.flying = false;
    placed.rest = orientation;
    placed.quest_tag = tag;
    Ogre::SceneNode* node = scene->getRootSceneNode()->createChildSceneNode(rest_at, orientation);
    node->attachObject(entity);
    placed.node = node;
    // `CLevel::addItem(.., false)`, no toss:
    // `CEquipment::activateDropParticles` (vtable +0x368,
    // called) starts the drop particle (`+0x3d0`) on the item's node.
    start_item_particles(placed);
    placed.presence.opacity = 1.0f;  // `addItem`
    dropped_items.push_back(placed);
    TL_LOG_INFO("item: %d x %s on the ground at (%.2f, %.2f, %.2f)%s%s", count,
                item.display_name.c_str(), static_cast<double>(rest_at.x),
                static_cast<double>(rest_at.y), static_cast<double>(rest_at.z),
                tag.empty() ? "" : " for quest ", tag.quest.c_str());
}

// PORT: the level's quest units placed through `QuestLog::populate_level`.
void Renderer::Impl::populate_quests() {
    load_quest_book();
    quest_log.populate_level(
        level_dungeon, level_depth,
        [this](const tl::core::QuestEntry& entry, const tl::core::QuestObjective& objective,
               const tl::core::QuestUnitTag& tag) {
            create_quest_unit(objective, tag,
                              entry.definition != nullptr ? entry.definition->guid : -1);
        });
}

// `CLevel::addCharacter(c, p, false)` ends every
// character it adds with `CCharacter::dropToGround(4.0)`;
// `CLevel::addUnit`. INVENTED: `dropToGround` is not read; 4.0
// is the ray's start above the point, and nothing under it keeps its height.
Ogre::Vector3 Renderer::Impl::dropped_to_ground(const Ogre::Vector3& at) const {
    Ogre::Vector3 place = at;
    float surface = 0.0f;
    if (collision.ground_under(at.x, at.z, at.y + 4.0f, surface)) place.y = surface;
    return place;
}

void Renderer::Impl::create_quest_unit(const tl::core::QuestObjective& objective,
                                       const tl::core::QuestUnitTag& tag, int64_t quest_guid) {
    // `spawnUnit` places nothing without the resolved
    // unit (`getBaseUnitDataGroup`).
    if (objective.resolved.empty()) {
        TL_LOG_WARN("quest '%s': objective '%s' resolved to no unit; nothing placed",
                    tag.quest.c_str(), objective.unit_name().c_str());
        return;
    }

    // `createUnit`: kind 0 the `Quest Boss` markers, kind
    // 1 the `Quest Item` ones; one picked by `randomBetween(1, count) - 1` on `g_Rand`.
    const std::vector<QuestMarker>& markers =
        objective.kind == 1 ? quest_item_markers : quest_boss_markers;
    Ogre::Vector3 at;
    Ogre::Quaternion facing = Ogre::Quaternion::IDENTITY;
    if (!markers.empty()) {
        const float pick =
            tl::core::game_random().between(1.0f, static_cast<float>(markers.size()));
        size_t index = static_cast<size_t>(static_cast<int64_t>(pick - 1.0f));
        if (index >= markers.size()) index = 0;  // `TArrayList`'s out-of-range read
        at = markers[index].position;
        facing = markers[index].orientation;
    } else {
        // INVENTED: with no marker the original picks a level unit and a free spot within
        // 10 of it (`CLevel::randomOpenPosition`); the walkable ground nearest a random
        // placed unit stands in.
        std::vector<Ogre::Vector3> near;
        for (const auto& entry : scene_units) {
            if (entry.second.node != 0 && entry.second.entity != nullptr) {
                if (Ogre::SceneNode* n = entry.second.entity->getParentSceneNode()) {
                    near.push_back(n->getPosition());
                }
            }
        }
        if (near.empty() || !nav.ready()) {
            TL_LOG_WARN("quest '%s': nowhere to place '%s'", tag.quest.c_str(),
                        objective.unit_name().c_str());
            return;
        }
        const int pick =
            tl::core::game_random().integer_between(0, static_cast<int>(near.size()) - 1);
        const Ogre::Vector3 from = near[static_cast<size_t>(pick)];
        const tl::core::Vec3 land = tl::core::random_open_item_position(
            {from.x, from.y, from.z}, 0.0f, 10.0f,
            [&](const tl::core::Vec3& p) {
                return !passability.ready() || passability.map_passable(p);
            },
            tl::core::game_random());
        at = Ogre::Vector3(land.x, land.y, land.z);
    }

    const int64_t node = spawn_quest_unit(objective, tag, quest_guid, at, facing);
    if (node == 0) return;
    // `createUnit`: `UNIQUE_MONSTER_NAME` names the character (`+0x4c0`).
    if (!objective.unique_monster_name.empty()) unit_names[node] = objective.unique_monster_name;
    const auto named = unit_names.find(node);
    TL_LOG_INFO("quest '%s': %s%s%s placed at (%.2f, %.2f, %.2f)%s", tag.quest.c_str(),
                objective.resolved.c_str(), named != unit_names.end() ? " as " : "",
                named != unit_names.end() ? named->second.c_str() : "", static_cast<double>(at.x),
                static_cast<double>(at.y), static_cast<double>(at.z),
                objective.make_champion ? ", a champion" : "");
}

int64_t Renderer::Impl::spawn_quest_unit(const tl::core::QuestObjective& objective,
                                         const tl::core::QuestUnitTag& tag, int64_t quest_guid,
                                         const Ogre::Vector3& at_in,
                                         const Ogre::Quaternion& facing) {
    // `CResourceManager::createUnit(+0x20, CLevel +0x1a8, false, false)`: item or character.
    ensure_item_index();
    if (const tl::core::ItemStats* item = items.find(objective.resolved)) {
        // `createUnit` -> `createItem`; the quest's
        // `QUEST_GUID` and the objective's id stamped, an equipment identified
        // (`+0x348`).
        tl::core::ItemStats stamped = create_item(*item, false, true);
        stamped.quest_guid = quest_guid;
        stamped.quest_unit_id = objective.id;
        stamped.identified = true;
        place_ground_item(stamped, 1, at_in, facing, tag);
        return 0;
    }
    // `CLevel::addUnit` at the level's level, then
    // `addCharacter`: dropped to the ground (a marker's Y is where the designer clicked).
    const Ogre::Vector3 at = dropped_to_ground(at_in);
    if (spawn_unit(objective.resolved, at, /*owner=*/0, 0, false, false, facing) == nullptr) {
        TL_LOG_WARN("quest '%s': could not place '%s'", tag.quest.c_str(),
                    objective.resolved.c_str());
        return 0;
    }
    const int64_t node = kSpawnedNodeBase + static_cast<int64_t>(spawn_serial);
    quest_unit_tags[node] = tag;
    // `spawnUnit`: a `MAKECHAMPION` objective's character is made a champion,
    // `randomIntegerBetween(0, 2)` on `g_Rand` after the unit is in the level.
    // INVENTED: `makeChampion` is this engine's champion path (`apply_champion_curves`).
    // `createPathfinder` rolls `MINION_SPAWNCLASS` on
    // `g_Rand` first, before `makeChampion`'s draw.
    const std::vector<std::string> minions =
        roll_unit_minions(objective.resolved, level_monster_level);
    if (objective.make_champion) {
        const int level = level_monster_level + tl::core::game_random().integer_between(0, 2);
        unit_spawn_level[node] = SpawnLevel{level, true};
        if (!objective.champion_name.empty()) unit_names[node] = objective.champion_name;
    }
    // A character of the level like any other, run by its AI.
    const Ogre::Vector3 ahead = facing * Ogre::Vector3::UNIT_Z;
    script.add_spawned_unit(node, objective.resolved, tl::core::Vec3{at.x, at.y, at.z},
                            /*hunts=*/false, std::atan2(ahead.x, ahead.z));
    apply_spawn_level(node);
    place_unit_minions(node, /*owner=*/0, minions);
    return node;
}

// PORT: a unit as the quest log's events read it.
tl::core::QuestEventUnit Renderer::Impl::quest_event_unit(const std::string& name,
                                                          const std::string& unittype,
                                                          const Ogre::Vector3& at,
                                                          const tl::core::QuestUnitTag& tag) {
    load_unit_types();
    tl::core::QuestEventUnit unit;
    unit.name = name;
    unit.tag = tag;
    const uint32_t type = unit_types.id_of(unittype);
    unit.isa = [this, type](uint32_t want) { return unit_types.isa(type, want); };
    unit.position = tl::core::Vec3{at.x, at.y, at.z};
    return unit;
}

// PORT: an item's quest tag from its quest GUID, through the port's quest book.
tl::core::QuestUnitTag Renderer::Impl::quest_tag_of_item(const tl::core::ItemStats& item) {
    if (item.quest_guid == -1) return {};
    load_quest_book();
    tl::core::QuestUnitTag tag;
    tag.objective = item.quest_unit_id;
    if (const tl::core::QuestDefinition* quest = quest_book.find_by_guid(item.quest_guid)) {
        tag.quest = quest->name;
        return tag;
    }
    // A guid the book has not got is still another quest's.
    tag.quest = "#" + std::to_string(item.quest_guid);
    return tag;
}

// PORT: a quest tag's GUID, through the port's quest book.
int64_t Renderer::Impl::quest_guid_of(const tl::core::QuestUnitTag& tag) {
    if (tag.empty()) return -1;
    load_quest_book();
    const tl::core::QuestDefinition* quest = quest_book.find(tag.quest);
    return quest != nullptr ? quest->guid : -1;
}

tl::core::QuestUnitWorld Renderer::Impl::quest_unit_world() {
    tl::core::QuestUnitWorld world;
    world.type_id = [this](const std::string& name) {
        load_unit_types();
        return unit_types.id_of(name);
    };
    // `CLevel::getUnitsByUnitType`: characters, then
    // items, each that `ISA(type)`. INVENTED: the port's order: each machine's characters
    // as made (no player), ground items as placed, then breakables; no unit triggers.
    world.units_of_type = [this](uint32_t type) {
        load_unit_types();
        std::vector<tl::core::QuestUnitWorld::LevelUnit> out;
        const auto isa = [this, type](const std::string& unittype) {
            return unit_types.isa(unit_types.id_of(unittype), type);
        };
        for (tl::core::LevelScript* machine : level_machines) {
            for (int64_t node : machine->character_ids()) {
                const std::string unit = machine->unit_of(node);
                if (!isa(stats_for_unit(unit).unittype)) continue;
                tl::core::QuestUnitWorld::LevelUnit found;
                found.handle = node;
                const auto tagged = quest_unit_tags.find(node);
                if (tagged != quest_unit_tags.end()) found.tag = tagged->second;
                out.push_back(found);
            }
        }
        for (size_t i = 0; i < dropped_items.size(); ++i) {
            const DroppedWorldItem& d = dropped_items[i];
            if (!isa(d.item.unittype)) continue;
            tl::core::QuestUnitWorld::LevelUnit found;
            found.handle = static_cast<int64_t>(i);
            found.item = true;
            found.tag = quest_tag_of_item(d.item);
            out.push_back(found);
        }
        for (const BreakableWorld& b : breakables) {
            if (!isa(stats_for_unit(b.unit).unittype)) continue;
            tl::core::QuestUnitWorld::LevelUnit found;
            found.handle = -1;  // a breakable carries no quest
            found.item = true;
            out.push_back(found);
        }
        return out;
    };
    // `populate`'s stamp: the quest's GUID and the objective's id.
    world.tag_unit = [this](const tl::core::QuestUnitWorld::LevelUnit& unit,
                            const tl::core::QuestUnitTag& tag) {
        if (!unit.item) {
            quest_unit_tags[unit.handle] = tag;
            return;
        }
        if (unit.handle < 0 || static_cast<size_t>(unit.handle) >= dropped_items.size()) return;
        DroppedWorldItem& d = dropped_items[static_cast<size_t>(unit.handle)];
        d.item.quest_guid = quest_guid_of(tag);
        d.item.quest_unit_id = tag.objective;
        d.quest_tag = tag;
        // `+0x348` and `destroyItemText`: identified, its label made again
        // from the item as it now is.
        d.item.identified = true;
    };
    // `CQuest +0x10`: the dungeon's kept template for the quest's floor.
    world.choose_spawners = [](const tl::core::QuestEntry& entry, int32_t seed_depth,
                               std::vector<std::string>& chosen) {
        if (entry.definition == nullptr) return false;
        tl::core::Dungeon* dungeon = loaded_dungeon_manager().dungeon_by_name(
            tl::core::upper_ascii(entry.definition->dungeon));
        if (dungeon == nullptr) return false;
        const tl::core::DungeonFiles files = archive_dungeon_files();
        // Its own generator: with no template the original returns before `resetGameSeed`.
        tl::core::RunicRandom floor_rand(0u);
        tl::core::reset_game_seed(tl::core::game_seed(), entry.depth, floor_rand);
        tl::core::LevelTemplate* found = dungeon->get_level_template_for_depth(
            static_cast<uint32_t>(std::max(0, entry.depth)), files, floor_rand);
        // The loop keeps no template that says `NO_RANDOM_QUESTS`.
        if (found == nullptr || found->no_random_quests) return false;
        tl::core::RunicRandom& g_rand = tl::core::game_random();
        tl::core::reset_game_seed(tl::core::game_seed(), seed_depth, g_rand);
        tl::core::choose_unit_spawners(*found, archive_spawn_class_lookup(), g_rand);
        chosen.assign(found->chosen_spawn_classes.begin(), found->chosen_spawn_classes.end());
        return true;
    };
    auto context = [this]() -> const tl::core::SpawnContext& {
        if (!loot_tables_built) {
            loot_tables_built = true;
            build_spawn_context(loot_tables, /*items_by_type=*/true);
        }
        return loot_tables.context;
    };
    world.has_spawn_class = [context](const std::string& name) {
        const tl::core::SpawnContext& c = context();
        return c.get_spawn_class && c.get_spawn_class(name) != nullptr;
    };
    world.roll_spawn_class = [context](const std::string& name, int32_t level, int32_t level_bonus,
                                       int32_t rarity_override) {
        const tl::core::SpawnContext& c = context();
        const tl::core::SpawnClass* table = c.get_spawn_class ? c.get_spawn_class(name) : nullptr;
        if (table == nullptr) return std::string();
        std::vector<std::string> units;
        table->roll_spawn_class(units, nullptr, nullptr, nullptr, level, 0xffffffffu, level_bonus,
                                rarity_override, 0, 0, tl::core::game_random(), c);
        return units.empty() ? std::string() : units[0];
    };
    world.random_open_position = [this](const tl::core::Vec3& at, float radius) {
        // `CLevel::randomOpenPosition(at, radius, false)`.
        return tl::core::random_open_position(
            at, 0.0f, radius,
            [this](const tl::core::Vec3& p) {
                return !passability.ready() ||
                       (passability.map_passable(p) && passability.position_passable(p));
            },
            tl::core::volatile_random());
    };
    world.spawn_unit = [this](const tl::core::QuestEntry& entry,
                              const tl::core::QuestObjective& objective,
                              const tl::core::QuestUnitTag& tag, const tl::core::Vec3& at) {
        const int64_t guid = entry.definition != nullptr ? entry.definition->guid : -1;
        spawn_quest_unit(objective, tag, guid, Ogre::Vector3(at.x, at.y, at.z),
                         Ogre::Quaternion::IDENTITY);
        TL_LOG_INFO("quest '%s': %s spawned at (%.2f, %.2f, %.2f) for an objective done",
                    tag.quest.c_str(), objective.resolved.c_str(), static_cast<double>(at.x),
                    static_cast<double>(at.y), static_cast<double>(at.z));
    };
    return world;
}

// PORT: the bags' item events wired to the renderer.
void Renderer::Impl::wire_bag_item_events() {
    bag_item_events.added = [this](tl::core::ItemStats& item) { bag_item_added(item); };
    bag_item_events.removed = [this](const tl::core::ItemStats& item) { bag_item_removed(item); };
    housing_item_events.removed = bag_item_events.removed;
    player_inventory.set_item_events(&bag_item_events);
    pet.bag().set_item_events(&bag_item_events);
}

Ogre::Vector3 Renderer::Impl::bag_event_position() const {
    if (bag_event_at_set) return bag_event_at;
    return subject_node != nullptr ? subject_node->getPosition() : Ogre::Vector3::ZERO;
}

// `CEquipment::addedToInventory`, the quest half:
// `questEventFire(0, player, item)`; an untagged item counted on pickup is stamped with
// its quest (`unitPickedUp`). The event 5 for an item held elsewhere (`+0x240`)
// is raised by the item leaving its bag first.
void Renderer::Impl::bag_item_added(tl::core::ItemStats& item) {
    const tl::core::QuestUnitTag carried = quest_tag_of_item(item);
    tl::core::QuestEventUnit taken =
        quest_event_unit(item.name, item.unittype, bag_event_position(), carried);
    taken.shown_name = tl::core::item_name(item);  // `getItemName()`
    quest_event(quest_log.item_picked_up(taken));
    if (!taken.tag.empty() && taken.tag.quest != carried.quest) {
        item.quest_guid = quest_guid_of(taken.tag);
    }
}

// `CEquipment::removedFromInventory`: `questEventFire(5, player, item)`,
// whatever takes the item out.
void Renderer::Impl::bag_item_removed(const tl::core::ItemStats& item) {
    tl::core::QuestEventUnit gone =
        quest_event_unit(item.name, item.unittype, bag_event_position(), quest_tag_of_item(item));
    quest_event(quest_log.item_dropped(gone));
}

void Renderer::Impl::quest_event(const std::vector<const tl::core::QuestDefinition*>& finished) {
    // `CGameUI::refreshQuestMenu` (X360 inline in `questEventUpdate`):
    // the open quest tab rebuilds its rows.
    if (quest_log.take_quest_menu_refresh()) inventory_menu.refresh_quest_tab();
    // `unitDefeated` and `unitPickedUp`: the
    // progress lines, for 1.0.
    for (const tl::core::QuestProgressText& text : quest_log.take_progress_texts()) {
        set_big_text(tl::core::strip_markup(text.big), 1.0f);
        set_small_text(text.small, 1.0f);
        TL_LOG_INFO("quest progress text: '%s' / '%s'", big_banner.text.c_str(),
                    small_banner.text.c_str());
        refresh_game_hud();
    }
    // `CQuestManager::questEventUpdate`: a quest this
    // event completed gets the "Quest Complete" banner and sample 0x20.
    for (const tl::core::QuestDefinition* quest : finished) {
        TL_LOG_INFO("quest '%s': objectives complete", quest->name.c_str());
        announce_quest(*quest, /*complete=*/true);
        // `SHOWTIPS` (`+0x2c`) or `CANABANDON` (`+0x208`).
        if (quest->show_tips || quest->can_abandon) {
            queue_tip(tl::core::ContextTip::QuestComplete);
        }
    }
    if (!finished.empty()) refresh_game_hud();
}

// `CEquipment::updateDrop`: 20 units of path a second,
// `LAND_SOUND` at the end. A weapon or shield turns once round its Z, before its rest.
void Renderer::Impl::step_dropped_item(DroppedWorldItem& dropped, float delta) {
    if (dropped.is_gold && dropped.node != nullptr) {
        // `CItemGold::update`: the sparkle (`+0x280`)
        // follows the wish to be seen; the pile grows as `min(1, 1 - left / 0.35)`.
        if (!dropped.presence.want_visible) {
            for (Ogre::SceneNode* holder : dropped.sparkle) holder->setVisible(false);
            return;
        }
        if (!dropped.presence.visible) return;
        for (Ogre::SceneNode* holder : dropped.sparkle) holder->setVisible(true);
        if (0.0f < dropped.grow_left) {
            dropped.grow_left = dropped.grow_left - delta;
            float grown = 1.0f - dropped.grow_left / 0.35f;
            if (!(grown <= 1.0f)) grown = 1.0f;
            const float size = grown * dropped.pile.scale;
            dropped.node->setScale(size, size, size);
        }
        return;
    }
    if (!dropped.flying || dropped.node == nullptr) return;
    // `CLevel::updateDroppingItems`, `updateDrop` inline;
    // the step along the path is one `fmadds`.
    float along = std::fma(delta, tl::core::kDropSpeed, dropped.travelled);
    dropped.travelled = along;
    if (dropped.arc.length <= along) {
        dropped.travelled = dropped.arc.length;
        dropped.flying = false;
        along = dropped.travelled;
    }
    const tl::core::Vec3 p = tl::core::spline_position_at_distance(dropped.arc, along);
    dropped.node->setPosition(p.x, p.y, p.z);
    if (dropped.spins) {
        // DIVERGENCE: a path of no length counts as flown; the console turns the item by
        // a NaN, which OGRE cannot hold.
        float progress = dropped.arc.length > 0.0f ? along / dropped.arc.length : 1.0f;
        if (progress > 1.0f) progress = 1.0f;
        dropped.node->setOrientation(
            dropped.rest *
            Ogre::Quaternion(Ogre::Radian(progress * 6.2831855f), Ogre::Vector3::UNIT_Z));
    }
    if (!dropped.flying) {
        dropped.node->setOrientation(dropped.rest);
        play_sound(dropped.item.land_sound, Ogre::Vector3(p.x, p.y, p.z));
        // How it lies: the model's height across the floor once laid down.
        dropped.node->_update(true, false);
        const Ogre::AxisAlignedBox box = dropped.entity->getWorldBoundingBox(true);
        TL_LOG_INFO("  %s landed at (%.2f, %.2f, %.2f), %.2f high by %.2f x %.2f",
                    dropped.item.display_name.c_str(), static_cast<double>(p.x),
                    static_cast<double>(p.y), static_cast<double>(p.z),
                    static_cast<double>(box.getSize().y), static_cast<double>(box.getSize().x),
                    static_cast<double>(box.getSize().z));
    }
}

void Renderer::Impl::start_item_particles(DroppedWorldItem& dropped) {
    if (dropped.is_gold || dropped.node == nullptr) return;
    load_unit_types();
    const tl::core::ItemStats& item = dropped.item;
    const uint32_t type = unit_types.id_of(item.unittype);
    // `CEquipment::createParticles`: the drop particle
    // by kind, then a weapon's element, each on the model's node.
    const bool quest =
        unit_types.isa(type, tl::core::unit_type::kQuestItem) || item.quest_guid != -1;
    std::vector<std::string> names;
    names.push_back(tl::core::drop_particle_name(item.drop_particle, quest,
                                                 unit_types.isa(type, tl::core::unit_type::kUnique),
                                                 tl::core::item_is_magical(item, unit_types)));
    if (unit_types.isa(type, tl::core::unit_type::kWeapon)) {
        const tl::core::EffectManager::Context& effects = item_effect_context();
        const std::string element = tl::core::elemental_particle_name(
            unit_types.isa(type, tl::core::unit_type::kPistol), [&](int damage_type) {
                return tl::core::weapon_damage_bonus(item, damage_type, effects);
            });
        if (!element.empty()) names.push_back(element);
    }
    for (const std::string& name : names) {
        // `CResourceManager::createParticle`: a preloaded name, or an authored path.
        std::string file = preloaded_particle_file(name);
        if (file.empty()) file = name;
        std::vector<Ogre::SceneNode*> holders = place_particle_layout(
            file, Ogre::Vector3::ZERO, Ogre::Quaternion::IDENTITY, dropped.node);
        if (holders.empty()) {
            TL_LOG_WARN("item '%s': no particle '%s'", item.name.c_str(), name.c_str());
            continue;
        }
        dropped.particles.insert(dropped.particles.end(), holders.begin(), holders.end());
    }
}

void Renderer::Impl::remove_ground_item(DroppedWorldItem& dropped) {
    // `CEquipment::setActiveInLevel(false)`: the drop
    // particle stops, the label goes (`hideItemText`).
    destroy_particle_holders(dropped.particles);
    destroy_particle_holders(dropped.sparkle);
    if (item_highlighted == dropped.entity) item_highlighted = nullptr;
    if (dropped.node != nullptr) scene->destroySceneNode(dropped.node);
    if (dropped.entity != nullptr) scene->destroyEntity(dropped.entity);
    dropped.node = nullptr;
    dropped.entity = nullptr;
}

void Renderer::Impl::load_unit_themes() {
    if (unit_themes_loaded) return;
    unit_themes_loaded = true;
    // `CUnitThemes::reload` over `media/unitthemes/*.dat`.
    // No two share a `NAME`, so the listing's order decides nothing.
    std::vector<tl::formats::AdmFile> files;
    for (const std::string& path : archive_paths_under("media/unitthemes/", ".dat.adm")) {
        const std::vector<uint8_t> bytes = read_resource(path);
        if (bytes.empty()) continue;
        tl::formats::AdmFile file = tl::formats::parse_adm(bytes.data(), bytes.size());
        if (file.ok) files.push_back(std::move(file));
    }
    unit_themes.reload(files);
    TL_LOG_INFO("unit themes: %zu", unit_themes.size());
}

void Renderer::Impl::init_unit_themes(UnitThemeState& state, const tl::core::UnitStats& stats) {
    // `CBaseUnit::unitInitThemes`, from
    // `CCharacter::unitInit`; here when first wanted.
    if (state.initialised) return;
    state.initialised = true;
    load_unit_themes();
    state.themes.init_themes(stats.themes_root, &unit_themes);
}

// PORT: a unit's theme particles destroyed.
void Renderer::Impl::clear_unit_themes(UnitThemeState& state) {
    for (UnitThemeState::Drawn& drawn : state.drawn) {
        destroy_particle_holders(drawn.holders);
        if (drawn.tag != nullptr) scene->destroySceneNode(drawn.tag);
    }
    state = UnitThemeState{};
}

void Renderer::Impl::unit_theme_updated(UnitThemeState& state, Ogre::Entity* entity,
                                        uint32_t unit_type, const tl::core::UnitTheme* theme,
                                        bool removed) {
    // The player's model keys of this theme start and stop with it
    // (`CCharacter::unitThemeUpdated`).
    if (&state == &player_themes && theme != nullptr) {
        std::string name = theme->name;
        for (char& c : name) {
            if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
        }
        if (removed) {
            player_theme_names.erase(name);
            for (size_t i = 0; i < particle_triggers.size();) {
                ParticleTrigger& trigger = particle_triggers[i];
                if (trigger.unit_theme != name) {
                    ++i;
                    continue;
                }
                destroy_particle_holders(trigger.holders);
                if (trigger.follower != nullptr) scene->destroySceneNode(trigger.follower);
                particle_triggers.erase(particle_triggers.begin() + static_cast<long>(i));
            }
        } else if (player_theme_names.insert(name).second) {
            equip_carried_particles();
        }
    }
    Ogre::SceneNode* unit_node = entity != nullptr ? entity->getParentSceneNode() : nullptr;
    if (unit_node == nullptr) return;
    Ogre::SkeletonInstance* skeleton = entity->hasSkeleton() ? entity->getSkeleton() : nullptr;

    const tl::core::ThemeParticleUnit unit = trigger_unit(
        state, entity, unit_type, theme != nullptr ? theme->name : std::string(), true);
    const std::vector<tl::core::ThemeParticleStep> steps =
        state.triggers.unit_theme_updated(theme, removed, unit, tl::core::volatile_random());
    configure_trigger_nodes(state, entity);

    for (const tl::core::ThemeParticleStep& step : steps) {
        const tl::core::ThemeParticleTrigger& trigger = state.triggers.triggers()[step.trigger];
        UnitThemeState::Drawn& drawn = state.drawn[static_cast<size_t>(trigger.particle)];
        if (step.start) {
            Ogre::Bone* bone =
                trigger.bone_found && skeleton != nullptr && skeleton->hasBone(trigger.bone)
                    ? skeleton->getBone(trigger.bone)
                    : nullptr;
            if (drawn.tag != nullptr && trigger.attaches && bone != nullptr) {
                drawn.tag->setPosition(bone->_getDerivedPosition());
                if (!trigger.follows) drawn.tag->setOrientation(bone->_getDerivedOrientation());
            } else if (drawn.tag != nullptr && !trigger.attaches) {
                // The unit's transform on the bone, or on the trigger's offset (+0x28).
                const Ogre::Vector3 local =
                    bone != nullptr
                        ? bone->_getDerivedPosition()
                        : Ogre::Vector3(trigger.offset_x, trigger.offset_y, trigger.offset_z);
                drawn.tag->setPosition(unit_node->_getFullTransform() * local);
            }
        }
        // `CParticle::Start` / `Stop` are the port's particle engine's: a stopped theme
        // particle is hidden, and shown when the theme returns.
        for (Ogre::SceneNode* holder : drawn.holders) holder->setVisible(step.start);
    }
}

tl::core::ThemeParticleUnit Renderer::Impl::trigger_unit(UnitThemeState& state,
                                                         Ogre::Entity* entity, uint32_t unit_type,
                                                         const std::string& label, bool light_map) {
    load_unit_types();
    Ogre::SceneNode* unit_node = entity->getParentSceneNode();
    Ogre::SkeletonInstance* skeleton = entity->hasSkeleton() ? entity->getSkeleton() : nullptr;

    tl::core::ThemeParticleUnit unit;
    unit.isa = [this, unit_type](const std::string& name) {
        return unit_types.isa(unit_type, unit_types.id_of(name));
    };
    unit.has_skeleton = skeleton != nullptr;
    unit.bone_count = skeleton != nullptr ? skeleton->getNumBones() : 0;
    unit.bone_name = [skeleton](uint16_t index) -> std::string {
        if (skeleton == nullptr || index >= skeleton->getNumBones()) return std::string();
        return skeleton->getBone(index)->getName();
    };
    unit.has_bone = [skeleton](const std::string& name) {
        return skeleton != nullptr && skeleton->hasBone(name);
    };
    // `CParticlePreloader::GetParticle`: the layout placed at the unit's node, stopped.
    unit.get_particle = [this, &state, unit_node, label](const std::string& asked) -> int {
        // A key spells its layout with backslashes.
        std::string file = asked;
        for (char& c : file) {
            if (c == '\\') c = '/';
        }
        std::vector<Ogre::SceneNode*> holders = place_particle_layout(
            file, Ogre::Vector3::ZERO, Ogre::Quaternion::IDENTITY, unit_node, ParticleStart::Held);
        if (holders.empty()) {
            TL_LOG_WARN("unit '%s': no particle '%s'", label.c_str(), file.c_str());
            return -1;
        }
        for (Ogre::SceneNode* holder : holders) holder->setVisible(false);
        // A theme's `IS LIGHT` particles reach the light map like any (place_particle()).
        UnitThemeState::Drawn drawn;
        drawn.holders = std::move(holders);
        state.drawn.push_back(std::move(drawn));
        return static_cast<int>(state.drawn.size() - 1);
    };
    return unit;
}

void Renderer::Impl::configure_trigger_nodes(UnitThemeState& state, Ogre::Entity* entity) {
    // `configureParticleTrigger`, the node half: an
    // attached particle on a bone rides a node under the unit's (+0x38), without a bone the
    // unit's node; one that does not attach stands in the world. At the trigger's offset.
    Ogre::SceneNode* unit_node = entity->getParentSceneNode();
    for (const tl::core::ThemeParticleTrigger& trigger : state.triggers.triggers()) {
        if (trigger.particle < 0) continue;
        UnitThemeState::Drawn& drawn = state.drawn[static_cast<size_t>(trigger.particle)];
        if (drawn.configured) continue;
        drawn.configured = true;
        if (!trigger.attaches || trigger.bone_found) {
            drawn.tag = trigger.attaches ? unit_node->createChildSceneNode()
                                         : scene->getRootSceneNode()->createChildSceneNode();
            for (Ogre::SceneNode* holder : drawn.holders) {
                if (Ogre::Node* up = holder->getParent()) up->removeChild(holder);
                drawn.tag->addChild(holder);
            }
        }
        const Ogre::Vector3 offset(trigger.offset_x, trigger.offset_y, trigger.offset_z);
        if (offset != Ogre::Vector3::ZERO) {
            for (Ogre::SceneNode* holder : drawn.holders) holder->translate(offset);
        }
    }
}

void Renderer::Impl::load_model_triggers(UnitThemeState& state, Ogre::Entity* entity,
                                         const std::string& mesh, uint32_t unit_type) {
    if (state.model_loaded) return;
    state.model_loaded = true;
    if (entity == nullptr || entity->getParentSceneNode() == nullptr) return;
    // The base skeleton is the one named as the mesh (`CGenericModel::loadAnimations`),
    // its animation the one called `BIND` (+0x5a).
    const std::vector<tl::core::AnimationTag>* keys = manifest_for(mesh).tags_of_file(mesh);
    if (keys == nullptr || keys->empty()) return;
    bool bind = false;
    if (entity->hasSkeleton()) {
        const Ogre::SkeletonInstance* skeleton = entity->getSkeleton();
        for (unsigned short i = 0; i < skeleton->getNumAnimations(); ++i) {
            if (tl::core::upper_ascii(skeleton->getAnimation(i)->getName()) == "BIND") bind = true;
        }
    }
    const size_t before = state.triggers.triggers().size();
    const tl::core::ThemeParticleUnit unit = trigger_unit(state, entity, unit_type, mesh, false);
    state.triggers.load_model(*keys, bind, &unit_themes, unit, tl::core::volatile_random());
    configure_trigger_nodes(state, entity);
    const size_t made = state.triggers.triggers().size() - before;
    if (made > 0) {
        TL_LOG_INFO("unit '%s': %zu particle(s) on its model%s", mesh.c_str(), made,
                    bind ? ", worn while shown" : "");
    }
}

// `CParticle::looping`: any emitter that does not expire
// or loops for ever (loop count -1, `+0x200`; X360 `+0x12c`).
bool Renderer::Impl::cparticle_looping(const UnitThemeState& state, int particle) const {
    if (particle < 0 || static_cast<size_t>(particle) >= state.drawn.size()) return false;
    const UnitThemeState::Drawn& drawn = state.drawn[static_cast<size_t>(particle)];
    for (const ParticleSystem& system : particle_systems) {
        if (system.pu.system == nullptr || std::find(drawn.holders.begin(), drawn.holders.end(),
                                                     system.holder()) == drawn.holders.end()) {
            continue;
        }
        if (cparticle_system_loops(system.pu)) return true;
    }
    return false;
}

void Renderer::Impl::apply_trigger_steps(UnitThemeState& state,
                                         const std::vector<tl::core::ThemeParticleStep>& steps) {
    for (const tl::core::ThemeParticleStep& step : steps) {
        const tl::core::ThemeParticleTrigger& trigger = state.triggers.triggers()[step.trigger];
        UnitThemeState::Drawn& drawn = state.drawn[static_cast<size_t>(trigger.particle)];
        if (step.park) {
            // `setVisible` hiding: `setPosition((0,
            // -99999, 0))`.
            const Ogre::Vector3 away(0.0f, -99999.0f, 0.0f);
            if (drawn.tag != nullptr) {
                drawn.tag->setPosition(away);
            } else {
                for (Ogre::SceneNode* holder : drawn.holders) holder->setPosition(away);
            }
            continue;
        }
        // `CParticle::Start`: systems from their start.
        // `Stop(true)` is gone at once, `Stop(false)` releases no more; hidden stops at once.
        bool shown = false;
        for (Ogre::SceneNode* holder : drawn.holders) {
            for (Ogre::MovableObject* object : holder->getAttachedObjects()) {
                shown = shown || object->getVisible();
            }
        }
        const bool at_once = !step.start && (step.at_once || !shown);
        if (step.start || at_once) {
            for (Ogre::SceneNode* holder : drawn.holders) holder->setVisible(step.start);
        }
        std::set<uint32_t> started;
        for (ParticleSystem& system : particle_systems) {
            if (std::find(drawn.holders.begin(), drawn.holders.end(), system.holder()) ==
                drawn.holders.end()) {
                continue;
            }
            if (step.start) {
                restart_particle_system(system);
                if (system.cparticle != 0) started.insert(system.cparticle);
            } else if (!at_once) {
                stop_particle_emission(system);
            }
        }
        for (const uint32_t id : started) cparticle_started(id);
    }
}

void Renderer::Impl::update_theme_particle_positions(UnitThemeState& state, Ogre::Entity* entity) {
    // `CCharacter::updateBonePositions` over `+0x738`: an
    // attached particle's node follows its bone, turning unless it `FOLLOWS`; an unattached
    // one that `FOLLOWS` moves to the bone's place, unturned.
    Ogre::SceneNode* unit_node = entity != nullptr ? entity->getParentSceneNode() : nullptr;
    if (unit_node == nullptr || !entity->hasSkeleton()) return;
    Ogre::SkeletonInstance* skeleton = entity->getSkeleton();
    for (const tl::core::ThemeParticleTrigger& trigger : state.triggers.triggers()) {
        if (trigger.particle < 0 || !trigger.bone_found) continue;
        UnitThemeState::Drawn& drawn = state.drawn[static_cast<size_t>(trigger.particle)];
        if (drawn.tag == nullptr || !skeleton->hasBone(trigger.bone)) continue;
        Ogre::Bone* bone = skeleton->getBone(trigger.bone);
        if (trigger.attaches) {
            drawn.tag->setPosition(bone->_getDerivedPosition());
            if (!trigger.follows) drawn.tag->setOrientation(bone->_getDerivedOrientation());
        } else if (trigger.follows) {
            drawn.tag->setPosition(unit_node->_getFullTransform() * bone->_getDerivedPosition());
        }
    }
}

void Renderer::Impl::step_unit_themes() {
    load_unit_types();
    load_unit_themes();
    // `CGameClient::update` refreshes the NPC icons when
    // the cinematic state changes.
    const bool cinematic = ui_in_cinematic();
    if (cinematic != npc_icons_cinematic) {
        npc_icons_cinematic = cinematic;
        update_npc_icons();
    }
    if (npc_icons_dirty && player_control && subject != nullptr) {
        npc_icons_dirty = false;
        update_npc_icons();
    }
    // The player's themes once it is in control (`load_player_stats` reads `[THEMES]`).
    if (subject != nullptr && player_control) {
        init_unit_themes(player_themes, player_stats);
        const uint32_t type = unit_types.id_of(player_stats.unittype);
        player_themes.themes.update(player_effects.themes(), [&](const tl::core::UnitTheme* theme,
                                                                 bool removed) {
            TL_LOG_INFO("player: theme %s %s", theme != nullptr ? theme->name.c_str() : "(none)",
                        removed ? "off" : "on");
            unit_theme_updated(player_themes, subject, type, theme, removed);
        });
        update_theme_particle_positions(player_themes, subject);
    }
    // `CBaseUnit::update`, the theme half: each placed
    // unit's own `CEffectManager` (`+0x1b8`) themes; none without a manager.
    static const std::vector<const tl::core::UnitTheme*> kNoEffectThemes;
    std::map<const SceneUnit*, const std::vector<const tl::core::UnitTheme*>*> effect_themes;
    const std::vector<MachineView> views = machines();
    for (const MachineView& view : views) {
        for (const int64_t node : view.script->units_with_effects()) {
            const tl::core::EffectManager* effects = view.script->unit_effects(node);
            const SceneUnit* drawn = scene_unit_of(view.script, node);
            if (effects != nullptr && drawn != nullptr) effect_themes[drawn] = &effects->themes();
        }
    }
    for (auto& entry : scene_units) {
        SceneUnit& su = entry.second;
        if (su.entity == nullptr) continue;
        const tl::core::UnitStats* stats = unit_book.find(entry.first);
        const uint32_t type = unit_types.id_of(stats != nullptr ? stats->unittype : std::string());
        // `CCharacter::unitInit`: `loadModel` makes the
        // base skeleton's triggers, then `unitInitThemes`.
        load_model_triggers(su.themes, su.entity, su.mesh, type);
        if (!su.themes.initialised) {
            init_unit_themes(su.themes, stats != nullptr ? *stats : tl::core::UnitStats{});
        }
        bool dead = false;
        int32_t visible = -1;
        int32_t ai_state = -1;
        for (const MachineView& view : views) {
            if (view.script->unit_is_dead(su.node)) dead = true;
            if (visible < 0) {
                visible = view.script->unit_visible(su.node);
                ai_state = view.script->unit_ai_state(su.node);
            }
        }
        // A unit no machine runs (the menu's): its node's own flag.
        if (visible < 0) visible = su.entity->getVisible() ? 1 : 0;
        if ((visible == 1) != su.themes.shown) {
            // `CCharacter::setVisible(bool, bool)`; the
            // port's changes are immediate (`LevelScript::set_character_visible`): `Stop(true)`.
            su.themes.shown = visible == 1;
            apply_trigger_steps(su.themes, su.themes.triggers.set_visible(
                                               su.themes.shown, true, ai_state,
                                               [&su](const tl::core::UnitTheme* theme) {
                                                   return su.themes.themes.has_unit_theme(theme);
                                               },
                                               [this, &su](int particle) {
                                                   return cparticle_looping(su.themes, particle);
                                               }));
        }
        if (dead) {
            // `CCharacter::die`: every trigger (`+0x738`)
            // that stops on death is `CParticle::Stop`ped, and a dead unit's themes are not
            // updated; what does not stop stays on the body.
            if (!su.themes.stopped_by_death) {
                su.themes.stopped_by_death = true;
                apply_trigger_steps(su.themes, su.themes.triggers.die());
            }
            update_theme_particle_positions(su.themes, su.entity);
            // `CCharacter::completeDeath`, once the fall
            // is over: `DESTROY_ON_DEATH` marks it for deletion (`+0x190`); trails off.
            // INVENTED: its other two reasons (`attachesToMaster`, effect 0x81) are unread.
            if (su.playing_state == nullptr || su.playing_state->hasEnded()) {
                if (const auto trails = unit_trails.find(su.entity); trails != unit_trails.end()) {
                    trails->second.trails.hide_all();
                }
                // And `setRenderBehind(false)`: no outline on a body.
                if (const RenderBehind* behind = render_behind_of(su.entity);
                    behind != nullptr && behind->on) {
                    set_render_behind(su.entity, false, behind->hostile);
                }
            }
            if (stats != nullptr && stats->destroy_on_death &&
                (su.playing_state == nullptr || su.playing_state->hasEnded())) {
                tl::core::LevelScript* owner = script_owning(su.node);
                if (owner != nullptr && owner->mark_unit_for_deletion(su.node)) {
                    TL_LOG_INFO("unit '%s': its death is complete and it is destroyed",
                                entry.first.c_str());
                }
            }
            continue;
        }
        // `CCharacter::makeChampion` adds the `CHAMPION`
        // theme (the glow), from `populateSectionOfLevel`,
        // `unitInit` and `spawnUnit`.
        //
        if (!su.themes.champion_checked) {
            su.themes.champion_checked = true;
            if (spawn_marked_champion(su.node)) {
                su.themes.themes.add_unit_theme("CHAMPION", &unit_themes);
            }
        }
        const auto held = effect_themes.find(&su);
        su.themes.themes.update(held != effect_themes.end() ? *held->second : kNoEffectThemes,
                                [&](const tl::core::UnitTheme* theme, bool removed) {
                                    TL_LOG_INFO("unit '%s': theme %s %s", entry.first.c_str(),
                                                theme != nullptr ? theme->name.c_str() : "(none)",
                                                removed ? "off" : "on");
                                    unit_theme_updated(su.themes, su.entity, type, theme, removed);
                                });
        update_theme_particle_positions(su.themes, su.entity);
    }
}

bool Renderer::Impl::ui_in_cinematic() const {
    // `CGameUI::getUIIsInCinematic`: `+0x10bc`, the
    // cinematic menu open, or an interactive menu counted by `setInteractiveMenuVisible`
    // (`CQuestDialogMenu::setOpen`).
    // INVENTED: its other tests are not read; the boxed line stands in.
    return tl::core::LevelScript::client_paused() || cinematic_open || 0 < interactive_menus ||
           quest_dialog.visible() || script_dialog_open();
}

void Renderer::Impl::update_npc_icons() {
    // `CLevel::updateNPCIcons`: needs a player in level.
    if (subject == nullptr || !player_control) return;
    load_unit_types();
    load_unit_themes();
    load_quest_book();
    std::vector<MachineView> views = machines();
    const tl::core::UnitStats none;
    for (auto& entry : scene_units) {
        SceneUnit& su = entry.second;
        if (su.entity == nullptr) continue;
        const tl::core::UnitStats* stats = unit_book.find(entry.first);
        const tl::core::UnitStats& data = stats != nullptr ? *stats : none;
        init_unit_themes(su.themes, data);
        // `getEnabled() || !+0x70d`: a disabled, hidden unit is passed over; `+0x190`
        // (pending deletion) stands for a dead unit.
        bool enabled = true;
        bool dead = false;
        for (const MachineView& view : views) {
            if (!view.script->node_enabled(su.node)) enabled = false;
            if (view.script->unit_is_dead(su.node)) dead = true;
        }
        const bool shown = enabled || su.entity->isVisible();
        tl::core::update_npc_icon(quest_log, entry.first, data.quests,
                                  unit_types.id_of(data.unittype), unit_types,
                                  tl::core::volatile_random(), ui_in_cinematic(), shown, dead,
                                  su.themes.themes, &unit_themes);
    }
}

void Renderer::Impl::step_ground_labels() {
    if (!game_hud.ready()) return;
    std::vector<tl::render::GroundLabel> labels;
    // hides every label while a menu is up.
    // INVENTED: its two client flags (`+0x1811`, `+0x1812`) are not decoded; play stands in.
    const bool open = player_control && !inventory_menu.visible() && camera != nullptr &&
                      subject_node != nullptr && !player_death.dying;
    if (!open) {
        game_hud.set_ground_labels(labels);
        return;
    }
    load_unit_types();
    const Ogre::Matrix4 view = camera->getViewMatrix();
    const Ogre::Matrix4 projection = camera->getProjectionMatrix();
    const Ogre::Vector3 lift = camera->getDerivedUp() * tl::core::kItemLabelLift;
    const Ogre::Vector3 me = subject_node->getPosition();
    const float across = static_cast<float>(config.width);
    const float down = static_cast<float>(config.height);

    // `CLevel::findItemsOnscreen`: the projection inside
    // [-1, 1] on both axes (strictly past -1 across), in front of the camera.
    auto on_screen = [&](const Ogre::Vector3& at) {
        const Ogre::Vector3 eye = view * at;
        if (!(eye.z < 0.0f)) return false;
        const Ogre::Vector3 clip = projection * eye;
        return !(clip.x <= -1.0f || 1.0f < clip.x || clip.y < -1.0f || 1.0f < clip.y);
    };
    // the anchor, lifted, projected to the screen's pixels.
    auto project = [&](const Ogre::Vector3& at, float& x, float& y) {
        const Ogre::Vector3 clip = projection * (view * (at + lift));
        x = across * 0.5f * clip.x + across * 0.5f;
        y = down * 0.5f - down * 0.5f * clip.y;
    };
    int highlighted_index = -1;
    auto add = [&](const std::string& name, const Ogre::Vector3& at, bool highlighted,
                   uint32_t type, const tl::core::ItemLabelFacts& facts) {
        tl::core::ItemLabelQuery query;
        query.highlighted = highlighted;
        query.container = unit_types.isa(type, tl::core::unit_type::kContainer);
        query.breakable = unit_types.isa(type, tl::core::unit_type::kBreakable);
        query.distance_squared = (at - me).squaredLength();
        // DIVERGENCE: no `KSETTINGS_KEYMAP_SHOWITEMS` key on a pad and no `TOGGLE DROPPED
        // ITEMS` setting; its default, 0, is used.
        query.show_items_held = false;
        query.toggle_dropped_items = 0;
        if (!tl::core::item_label_wanted(query)) return;
        tl::render::GroundLabel label;
        label.text = tl::core::item_label_text(name);
        project(at, label.at_x, label.at_y);
        const tl::core::LabelColour colour = tl::core::item_label_colour(
            tl::core::item_label_tint(facts), highlighted, item_label_colours);
        label.colour = Ogre::ColourValue(colour.r, colour.g, colour.b, colour.a);
        label.enabled = highlighted;
        if (highlighted) highlighted_index = static_cast<int>(labels.size());
        labels.push_back(label);
    };

    for (size_t i = 0; i < dropped_items.size(); ++i) {
        const DroppedWorldItem& dropped = dropped_items[i];
        // Skipped: a flying item (disabled by `drop`, vtable +0x40) or a hidden one (`+0x81`).
        if (dropped.flying || dropped.node == nullptr || !dropped.presence.visible) continue;
        const Ogre::Vector3 at = dropped.node->getPosition();
        if (!on_screen(at)) continue;
        const tl::core::ItemStats& item = dropped.item;
        const uint32_t type = unit_types.id_of(item.unittype);
        tl::core::ItemLabelFacts facts;
        if (!dropped.is_gold) {
            // A `CEquipment`; gold (`CItemGold`) is never quest, unique or magical.
            facts.quest =
                unit_types.isa(type, tl::core::unit_type::kQuestItem) || item.quest_guid != -1;
            facts.set = !item.set.empty();
            facts.unique = unit_types.isa(type, tl::core::unit_type::kUnique);
            facts.magical = tl::core::item_is_magical(item, unit_types);
            facts.random_magic_socketable =
                unit_types.isa(type, tl::core::unit_type::kRandomMagicSocketable);
            facts.magic = unit_types.isa(type, tl::core::unit_type::kMagic);
        }
        // Gold's name is `CItem::getItemName`, `+0x210`.
        const std::string& name = dropped.is_gold ? item.display_name : tl::core::item_name(item);
        add(name, at, static_cast<int>(i) == item_targeted, type, facts);
    }

    // Every `CTriggerUnit` is a `CItem`, labelled the same way; a non-takeable one only
    // while `+0x1f0` is set (cleared by `CTriggerUnit::trigger`,
    // on a one-shot's last state), never one with `NONSELECTABLE`.
    const auto triggers = [&](int machine, const tl::core::LevelScript& owner) {
        for (const tl::core::LevelScript::UnitTriggerView& view_of : owner.unit_triggers()) {
            if (!view_of.armed) continue;
            if (non_selectable_prop(view_of.unit)) continue;
            // Passed over when hidden by its range or the camera (`+0x81`).
            const auto shown = trigger_presence.find(instance_key(view_of.id, machine));
            if (shown != trigger_presence.end() && !shown->second.presence.visible) continue;
            TriggerProp prop;
            trigger_prop(view_of.unit, prop);
            if (prop.display_name.empty()) continue;  // no such unit to label
            const Ogre::Vector3 at(view_of.at.x, view_of.at.y, view_of.at.z);
            if (!on_screen(at)) continue;
            const bool highlighted =
                machine == trigger_targeted_machine && view_of.id == trigger_targeted_node;
            add(prop.display_name, at, highlighted, unit_types.id_of(prop.unittype),
                tl::core::ItemLabelFacts{});
        }
    };
    triggers(kLevelMachine, script);
    for (size_t i = 0; i < nested_scripts.size(); ++i) {
        triggers(static_cast<int>(i), nested_scripts[i].script);
    }
    for (size_t i = 0; i < open_portals.size(); ++i) {
        const OpenPortal& portal = open_portals[i];
        if (!portal.enabled) continue;
        if (!on_screen(portal.at)) continue;
        add(portal.display_name, portal.at, static_cast<int>(i) == portal_targeted,
            unit_types.id_of(portal.unittype), tl::core::ItemLabelFacts{});
    }
    // `CLevel::findItemsOnscreen` lists the `MIMICIDLE` mimics too, worded by
    // `CGameUI::updateIngameUI` with `CCharacter::getMimicName`.
    // INVENTED: the label's tint and distance rule are a chest's (`OPENABLE`).
    const auto mimics = [&](int machine, const tl::core::LevelScript& owner) {
        for (const tl::core::LevelScript::UnitTriggerView& mimic : owner.idle_mimics()) {
            const Ogre::Vector3 at(mimic.at.x, mimic.at.y, mimic.at.z);
            if (!on_screen(at)) continue;
            const std::string& dormant = stats_for_unit(mimic.unit).dormant_name;
            add(dormant.empty() ? std::string("Chest") : dormant, at,
                machine == mimic_targeted_machine && mimic.id == mimic_targeted_node,
                unit_types.id_of("OPENABLE"), tl::core::ItemLabelFacts{});
        }
    };
    mimics(kLevelMachine, script);
    for (size_t i = 0; i < nested_scripts.size(); ++i) {
        mimics(static_cast<int>(i), nested_scripts[i].script);
    }

    // `setItemTextHighlighted(true)` ends with `moveToFront`: drawn last.
    if (highlighted_index >= 0 && highlighted_index + 1 < static_cast<int>(labels.size())) {
        std::rotate(labels.begin() + highlighted_index, labels.begin() + highlighted_index + 1,
                    labels.end());
    }
    // More than the pool draws: keep the highlighted one among those drawn.
    const size_t pool = tl::render::GameHud::kMaxGroundLabels;
    if (highlighted_index >= 0 && labels.size() > pool) std::swap(labels[pool - 1], labels.back());
    game_hud.set_ground_labels(labels);
}

/// `CCharacter::getItemAI`'s targeting half: the nearest
/// `dropped_items` entry within `kTargetItemRadius` of a point `kTargetItemBehind` behind
/// the player. `active` false (a unit is targeted,
/// `CCharacter::setTargetItem`) clears the pick without searching.
bool Renderer::Impl::update_item_selection(bool active) {
    int chosen = -1;
    int machine_chosen = kNoMachine;
    int64_t node_chosen = 0;
    int portal_chosen = -1;
    int mimic_machine_chosen = kNoMachine;
    int64_t mimic_node_chosen = 0;
    Ogre::Entity* entity = nullptr;
    bool second = false;
    if (active && subject_node != nullptr) {
        const Ogre::Vector3 forward = subject_node->getOrientation() * Ogre::Vector3::UNIT_Z;
        const Ogre::Vector3 from =
            subject_node->getPosition() - forward * tl::core::kTargetItemBehind;
        float best = tl::core::kTargetItemRadius;
        // The cone's selection (given this point and 3.5):
        // in the ground plane, height does not count.
        const auto planar = [&](const Ogre::Vector3& at) {
            const float dx = at.x - from.x;
            const float dz = at.z - from.z;
            return std::sqrt(dx * dx + dz * dz);
        };
        for (size_t i = 0; i < dropped_items.size(); ++i) {
            // Nothing to take while it flies (`drop` / `updateDrop`, vtable +0x40).
            if (dropped_items[i].flying) continue;
            const float dist = planar(dropped_items[i].node->getPosition());
            if (dist <= best) {
                best = dist;
                chosen = static_cast<int>(i);
                entity = dropped_items[i].entity;
            }
        }
        // The props the same pass finds: every enabled `Unit Trigger`, where its node
        // stands. A `No Loop` (`+0x254` 0) one-shot past its last state is not clickable
        // (`CTriggerUnit::trigger`, clears `+0x1f0`).
        const auto consider = [&](int machine, const tl::core::LevelScript& owner,
                                  const NodeMap& nodes) {
            for (const tl::core::LevelScript::UnitTriggerView& view : owner.unit_triggers()) {
                if (!view.armed) continue;
                if (non_selectable_prop(view.unit)) continue;
                const float dist = planar(Ogre::Vector3(view.at.x, view.at.y, view.at.z));
                if (!(dist <= best)) continue;
                best = dist;
                chosen = -1;
                machine_chosen = machine;
                node_chosen = view.id;
                entity = trigger_entity(view.id, view.unit, nodes);
            }
        };
        consider(kLevelMachine, script, scripted_nodes);
        for (size_t i = 0; i < nested_scripts.size(); ++i) {
            const NestedScript& machine = nested_scripts[i];
            consider(static_cast<int>(i), machine.script, machine.nodes);
        }
        // The portal props `createPortals` added: `CTriggerUnit`s like the rest.
        for (size_t i = 0; i < open_portals.size(); ++i) {
            if (!open_portals[i].enabled) continue;
            const float dist = planar(open_portals[i].at);
            if (!(dist <= best)) continue;
            best = dist;
            chosen = -1;
            machine_chosen = kNoMachine;
            node_chosen = 0;
            portal_chosen = static_cast<int>(i);
            entity = nullptr;
        }
        // A mimic lying in wait, among the props it passes for.
        const auto consider_mimics = [&](int machine, const tl::core::LevelScript& owner) {
            for (const tl::core::LevelScript::UnitTriggerView& mimic : owner.idle_mimics()) {
                const float dist = planar(Ogre::Vector3(mimic.at.x, mimic.at.y, mimic.at.z));
                if (!(dist <= best)) continue;
                best = dist;
                chosen = -1;
                machine_chosen = kNoMachine;
                node_chosen = 0;
                portal_chosen = -1;
                mimic_machine_chosen = machine;
                mimic_node_chosen = mimic.id;
                entity = nullptr;
                for (const auto& entry : scene_units) {
                    if (entry.second.node == mimic.id) entity = entry.second.entity;
                }
            }
        };
        consider_mimics(kLevelMachine, script);
        for (size_t i = 0; i < nested_scripts.size(); ++i) {
            consider_mimics(static_cast<int>(i), nested_scripts[i].script);
        }

        // the function's last call: with nothing found, the second pick
        // over the level's items (shown, usable, active, selectable, not breakable) in the
        // character's frame; the sight line runs 5 above both, through units' models.
        if (chosen < 0 && machine_chosen == kNoMachine && portal_chosen < 0 &&
            mimic_machine_chosen == kNoMachine) {
            const Ogre::Vector3 here = subject_node->getPosition();
            const Ogre::Quaternion back = subject_node->getOrientation().Inverse();
            std::vector<tl::core::SecondPickCandidate> picks;
            struct Source {
                int dropped = -1;
                int machine = kNoMachine;
                int64_t node = 0;
                int portal = -1;
                Ogre::Entity* entity = nullptr;
            };
            std::vector<Source> sources;
            const auto add = [&](const Ogre::Vector3& at, bool fishing_hole, const Source& source) {
                const Ogre::Vector3 local = back * (at - here);
                tl::core::SecondPickCandidate pick;
                pick.local_x = local.x;
                pick.local_z = local.z;
                pick.fishing_hole = fishing_hole;
                tl::core::CollisionHit hit;
                pick.in_sight =
                    collision.empty() ||
                    !collision.ray_collision(
                        tl::core::Vec3{here.x, here.y + tl::core::kTargetSightHeight, here.z},
                        tl::core::Vec3{at.x, at.y + tl::core::kTargetSightHeight, at.z}, false,
                        hit);
                picks.push_back(pick);
                sources.push_back(source);
            };
            for (size_t i = 0; i < dropped_items.size(); ++i) {
                const DroppedWorldItem& dropped = dropped_items[i];
                if (dropped.flying || !dropped.presence.active || !dropped.presence.visible)
                    continue;
                Source source;
                source.dropped = static_cast<int>(i);
                source.entity = dropped.entity;
                add(dropped.node->getPosition(), false, source);
            }
            const auto consider_props = [&](int machine, const tl::core::LevelScript& owner,
                                            const NodeMap& nodes) {
                for (const tl::core::LevelScript::UnitTriggerView& view : owner.unit_triggers()) {
                    const int64_t key = instance_key(view.id, machine);
                    if (!view.armed) continue;
                    if (non_selectable_prop(view.unit)) continue;
                    const auto presence = trigger_presence.find(key);
                    if (presence != trigger_presence.end() &&
                        (!presence->second.presence.active || !presence->second.presence.visible)) {
                        continue;
                    }
                    Source source;
                    source.machine = machine;
                    source.node = view.id;
                    source.entity = trigger_entity(view.id, view.unit, nodes);
                    add(Ogre::Vector3(view.at.x, view.at.y, view.at.z), is_fishing_hole(view.unit),
                        source);
                }
            };
            consider_props(kLevelMachine, script, scripted_nodes);
            for (size_t i = 0; i < nested_scripts.size(); ++i) {
                consider_props(static_cast<int>(i), nested_scripts[i].script,
                               nested_scripts[i].nodes);
            }
            for (size_t i = 0; i < open_portals.size(); ++i) {
                if (!open_portals[i].enabled) continue;
                Source source;
                source.portal = static_cast<int>(i);
                add(open_portals[i].at, false, source);
            }
            const int pick = tl::core::second_pick(picks);
            if (pick >= 0) {
                const Source& source = sources[static_cast<size_t>(pick)];
                chosen = source.dropped;
                machine_chosen = source.machine;
                node_chosen = source.node;
                portal_chosen = source.portal;
                entity = source.entity;
                second = true;
            }
        }
    }

    if (entity != item_highlighted) {
        if (item_highlighted != nullptr) {
            bool still_there = entity_still_placed(item_highlighted);
            for (const DroppedWorldItem& dropped : dropped_items) {
                if (dropped.entity == item_highlighted) {
                    still_there = true;
                    break;
                }
            }
            // Not still there: picked up this frame, its entity already destroyed.
            if (still_there) highlight_entity(item_highlighted, false, false, rim_light_texture);
        }
        if (entity != nullptr) highlight_entity(entity, true, false, rim_light_texture);
        item_highlighted = entity;
    }
    if (machine_chosen != trigger_targeted_machine || node_chosen != trigger_targeted_node) {
        if (machine_chosen != kNoMachine) {
            TL_LOG_INFO("pad prop: unit trigger %lld%s", static_cast<long long>(node_chosen),
                        entity != nullptr ? "" : " (no model to light)");
        }
    }
    if (portal_chosen != portal_targeted && portal_chosen >= 0) {
        TL_LOG_INFO("pad prop: portal '%s'",
                    open_portals[static_cast<size_t>(portal_chosen)].display_name.c_str());
    }
    if (second && (chosen != item_targeted || machine_chosen != trigger_targeted_machine ||
                   node_chosen != trigger_targeted_node || portal_chosen != portal_targeted)) {
        TL_LOG_INFO("pad item: the second pick (%s)", chosen >= 0          ? "a floor item"
                                                      : portal_chosen >= 0 ? "a portal"
                                                                           : "a prop");
    }
    item_second_pick = second;
    if (mimic_node_chosen != mimic_targeted_node && mimic_node_chosen != 0) {
        TL_LOG_INFO("pad prop: a mimic lying in wait (%lld)",
                    static_cast<long long>(mimic_node_chosen));
    }
    item_targeted = chosen;
    trigger_targeted_machine = machine_chosen;
    trigger_targeted_node = node_chosen;
    portal_targeted = portal_chosen;
    mimic_targeted_machine = mimic_machine_chosen;
    mimic_targeted_node = mimic_node_chosen;
    return chosen >= 0 || machine_chosen != kNoMachine || portal_chosen >= 0 ||
           mimic_machine_chosen != kNoMachine;
}

void Renderer::Impl::target_selected_item() {
    // (A's handler): with no unit target, the selector's item into
    // `+0x1cc` and `setTargetItem`, which drops the unit
    // target; AI state 7, `getItemAI`.
    PlayerItemTarget target;
    if (portal_targeted >= 0 && static_cast<size_t>(portal_targeted) < open_portals.size()) {
        const OpenPortal& portal = open_portals[static_cast<size_t>(portal_targeted)];
        target.kind = PlayerItemTarget::Kind::Portal;
        target.portal = portal.unit;
        target.portal_at = portal.at;
    } else if (trigger_targeted_machine != kNoMachine) {
        target.kind = PlayerItemTarget::Kind::Trigger;
        target.machine = trigger_targeted_machine;
        target.node = trigger_targeted_node;
    } else if (item_targeted >= 0 && static_cast<size_t>(item_targeted) < dropped_items.size()) {
        target.kind = PlayerItemTarget::Kind::Dropped;
        target.dropped = dropped_items[static_cast<size_t>(item_targeted)].node;
    } else {
        return;
    }
    pending_talk.clear();
    clear_talk_target();
    player_item = target;
    TL_LOG_INFO("pad item: A sets the target item (%s%s)",
                target.kind == PlayerItemTarget::Kind::Dropped  ? "a floor item"
                : target.kind == PlayerItemTarget::Kind::Portal ? "a portal"
                                                                : "a prop",
                item_second_pick ? ", the second pick" : "");
}

void Renderer::Impl::step_mimic_attack(float delta) {
    const auto let_go = [this](const char* why) {
        TL_LOG_INFO("pad prop: the mimic is let go (%s)", why);
        player_item = PlayerItemTarget{};
    };
    if (!player_control || subject_node == nullptr) {
        player_item = PlayerItemTarget{};
        return;
    }
    // `CCharacter::attackAI` on `+0x340`: a dying, dead
    // or gone target is let go. INVENTED: one that woke is let go too.
    tl::core::LevelScript* owner = machine_script(player_item.machine);
    tl::core::Vec3 at;
    if (owner == nullptr || owner->unit_is_dead(player_item.node) ||
        !owner->unit_position(player_item.node, at)) {
        return let_go("gone");
    }
    if (!owner->unit_mimic_idle(player_item.node)) return let_go("awake");
    load_unit_types();
    load_stat_graphs();
    const tl::core::Vec3 me = player.position();
    const float dx = at.x - me.x;
    const float dz = at.z - me.z;
    const float d = std::sqrt(dx * dx + 0.0f + dz * dz);
    // Beyond `FOLLOW_RADIUS` (`+0x370`), with no master: stopped and let go.
    if (!(d < player_stats.follow_radius)) {
        player.stop_following();
        return let_go("past the follow radius");
    }
    // `inAttackRange(0)` with the attack in use (weapon
    // `RANGE`, or bare hands via `selectAttack`); a `RANGED WEAPON`
    // (ISA 0x23) skips the height test and adds effect 0x4e.
    select_player_attack(0);
    const tl::core::HeldHands hands = player_held_hands();
    tl::core::AttackDescription attack =
        has_weapon ? tl::core::AttackDescription{}
                   : tl::core::own_attack(player_stats, player_stats.level, config.difficulty,
                                          /*player=*/true, stat_graphs);
    if (has_weapon) attack.range = weapon.range;
    const bool ranged = has_weapon && unit_types.isa(unit_types.id_of(weapon.unittype),
                                                     tl::core::unit_type::kRangedWeapon);
    tl::core::UnitStats body = player_stats;
    body.radius = player.body().radius;
    const bool in_range = tl::core::in_attack_range(
        body, attack, me, at, owner->unit_radius(player_item.node), ranged,
        ranged ? player_total().effect_value(0x4e) : 0.0f, &hands);
    // `validLineOfSight(level, false)`: 1.5
    // above both feet, the rise clamped over 5; units' models block
    // (`LevelScript::valid_line_of_sight`).
    bool sight = true;
    {
        const tl::core::Vec3 from{me.x, me.y + 1.5f, me.z};
        tl::core::Vec3 to{at.x, at.y + 1.5f, at.z};
        const float limit = std::sqrt(to.x * to.x + from.y * from.y + to.z * to.z) / 5.0f;
        float rise = to.y - from.y;
        if (limit <= rise) rise = limit;
        if (rise <= -limit) rise = -limit;
        to.y = rise + from.y;
        tl::core::CollisionHit hit;
        sight = collision.empty() || !collision.ray_collision(from, to, true, hit);
    }
    if (!in_range || !sight) {
        // A swing with more than 0.2 s left or a cast holding: nothing; else `setAIState(4)`.
        if (swing.active && 0.2f < (swing.length - swing.elapsed) / swing_rate()) return;
        if (player_performing_skill()) return;
        if (!player.following()) {
            std::vector<tl::core::Vec3> route;
            if (nav.ready()) route = nav.find_path(me, at);
            if (route.empty()) return let_go("no route");
            player.follow(std::move(route));
        }
        return;
    }
    // In reach and sight: stopped, turned (`turnTowardPosition`), and once facing it (half
    // of `ATTACKANGLE`) `attack()`, then `setAIState(0)`.
    player.stop_following();
    player.face(tl::core::turn_toward(player.facing(), dx, dz, player_stats.turn_rate, delta));
    if (!tl::core::facing_target(player.facing(), me, at, player_stats.attack_angle)) return;
    // `CCharacter::attack` on a `MIMICIDLE` target: ranged,
    // `setDestination` to it; else `CMonster::reactToDamage` (vtable +0x350)
    // and no blow.
    if (ranged) {
        std::vector<tl::core::Vec3> route;
        if (nav.ready()) route = nav.find_path(me, at);
        if (!route.empty()) player.follow(std::move(route));
        return;
    }
    mimic_targeted_machine = player_item.machine;
    mimic_targeted_node = player_item.node;
    player_item = PlayerItemTarget{};
    disturb_targeted_mimic();
}

void Renderer::Impl::step_item_ai(float delta) {
    using Kind = PlayerItemTarget::Kind;
    if (player_item.kind == Kind::None) return;
    if (player_item.kind == Kind::Mimic) return step_mimic_attack(delta);
    const auto let_go = [this](const char* why) {
        TL_LOG_INFO("pad item: the target item is let go (%s)", why);
        player_item = PlayerItemTarget{};
    };
    if (!player_control || subject_node == nullptr) {
        player_item = PlayerItemTarget{};
        return;
    }
    load_unit_types();

    // The target as `getItemAI` reads it; one the level no longer holds is let go
    // (`notifyOfDeletion`).
    tl::core::ItemAITarget item;
    std::string unittype;
    int dropped_index = -1;
    int portal_index = -1;
    switch (player_item.kind) {
        case Kind::Dropped:
            for (size_t i = 0; i < dropped_items.size(); ++i) {
                if (dropped_items[i].node == player_item.dropped)
                    dropped_index = static_cast<int>(i);
            }
            if (dropped_index < 0) return let_go("gone");
            {
                const DroppedWorldItem& dropped = dropped_items[static_cast<size_t>(dropped_index)];
                const Ogre::Vector3 at = dropped.node->getPosition();
                item.at = tl::core::Vec3{at.x, at.y, at.z};
                // `CBaseUnit::CBaseUnit`: 0.3;
                // `CItem::unitInit` reads no `COLLISION_RADIUS`.
                item.radius = 0.3f;
                unittype = dropped.item.unittype;
            }
            break;
        case Kind::Trigger: {
            tl::core::LevelScript* owner = machine_script(player_item.machine);
            if (owner == nullptr) return let_go("gone");
            bool found = false;
            for (const tl::core::LevelScript::UnitTriggerView& view : owner->unit_triggers()) {
                if (view.id != player_item.node) continue;
                found = true;
                item.at = view.at;
                // `CTriggerUnit::unitInit`: `COLLISION_RADIUS`.
                TriggerProp prop;
                item.radius = trigger_prop(view.unit, prop) ? prop.collision_radius : 0.3f;
                unittype = prop.unittype;
            }
            if (!found) return let_go("disabled");
            break;
        }
        case Kind::Portal:
            for (size_t i = 0; i < open_portals.size(); ++i) {
                if (open_portals[i].enabled && open_portals[i].unit == player_item.portal &&
                    open_portals[i].at == player_item.portal_at) {
                    portal_index = static_cast<int>(i);
                }
            }
            if (portal_index < 0) return let_go("gone");
            {
                const OpenPortal& portal = open_portals[static_cast<size_t>(portal_index)];
                item.at = tl::core::Vec3{portal.at.x, portal.at.y, portal.at.z};
                TriggerProp prop;
                item.radius = trigger_prop(portal.unit, prop) ? prop.collision_radius : 0.3f;
                unittype = portal.unittype;
            }
            break;
        case Kind::None:
        case Kind::Mimic: return;
    }
    namespace ut = tl::core::unit_type;
    const uint32_t type = unit_types.id_of(unittype);
    item.container = unit_types.isa(type, 0x1e);
    item.breakable = unit_types.isa(type, 0x1d);
    item.interactable = unit_types.isa(type, 0x20);
    item.fishing_hole = unit_types.isa(type, ut::kFishingHole);
    item.enchant_shrine = unit_types.isa(type, ut::kEnchantShrine);
    item.stat_shrine = unit_types.isa(type, ut::kStatShrine);
    item.gem_shrine = unit_types.isa(type, ut::kGemShrine);

    tl::core::ItemAICharacter me;
    me.at = player.position();
    me.radius = player.body().radius;
    me.walking = player.following();
    // `validLineOfSight(level, true)` towards `+0x350`:
    // level, at 1.5 above the character - the item's y is never loaded.
    // Through `sightUnobstructed`, `CLevel::rayCollision`
    // skips the units' own models.
    {
        const tl::core::Vec3 from{me.at.x, me.at.y + 1.5f, me.at.z};
        const tl::core::Vec3 to{item.at.x, from.y, item.at.z};
        tl::core::CollisionHit hit;
        me.line_of_sight = collision.empty() || !collision.ray_collision(from, to, false, hit);
    }
    me.facing = tl::core::facing_target(player.facing(), me.at, item.at, player_stats.attack_angle);

    const tl::core::ItemAIStep step = tl::core::item_ai(item, me);
    using Step = tl::core::ItemAIStep;
    switch (step) {
        case Step::Wait: return;
        case Step::Walk: {
            // `setDestination(item)` and vtable +0x268; no route: `setTargetItem(nullptr)`.
            std::vector<tl::core::Vec3> route;
            if (nav.ready()) route = nav.find_path(me.at, item.at);
            if (route.empty()) return let_go("no route");
            player.follow(std::move(route));
            return;
        }
        case Step::GiveUp: return let_go("given up");
        case Step::Attack:
            // A breakable container: no item pass offers one (the breakables
            // are the swing's).
            return let_go("a breakable container");
        case Step::Turn: {
            // Stopped; `updateMotion` turns it (`turnToward`).
            player.stop_following();
            const float dx = item.at.x - me.at.x;
            const float dz = item.at.z - me.at.z;
            player.face(
                tl::core::turn_toward(player.facing(), dx, dz, player_stats.turn_rate, delta));
            return;
        }
        case Step::Take:
            player.stop_following();
            player_item = PlayerItemTarget{};
            item_targeted = dropped_index;
            if (dropped_index >= 0) {
                pick_up_targeted_item();
            } else {
                TL_LOG_INFO("pad item: a prop that is not interactable is not taken");
            }
            return;
        case Step::Interact:
        case Step::Fish:
        case Step::EnchantShrine:
        case Step::StatShrine:
        case Step::GemShrine:
            // `interact_targeted_trigger` is the rest: fishing, a shrine's state 0x16, and
            // the prop's own `interact`.
            player.stop_following();
            {
                const PlayerItemTarget used = player_item;
                player_item = PlayerItemTarget{};
                if (used.kind == Kind::Portal) {
                    portal_targeted = portal_index;
                    use_targeted_portal();
                } else if (used.kind == Kind::Trigger) {
                    trigger_targeted_machine = used.machine;
                    trigger_targeted_node = used.node;
                    interact_targeted_trigger();
                }
            }
            return;
    }
}

}  // namespace tl::render
