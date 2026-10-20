// The renderer's implementation: HUD, inventory, quests, enchanting, fishing, death screen.
// Split from renderer.cpp; the shared
// declarations are in renderer_impl.h.
#include "core/run_config.h"
#include "render/renderer_impl.h"

namespace tl::render {
bool Renderer::game_hud_ready() const { return impl_->game_hud.ready(); }

bool Renderer::dump_game_hud(const std::string& png_path) {
    Impl& m = *impl_;
    // Whatever the state is now, including a `--highlight` standing in for the
    // selector: the dump is taken before the run loop, which is what would
    // otherwise have refreshed this.
    m.refresh_game_hud();
    if (!m.game_hud.ready()) {
        TL_LOG_WARN("no game HUD to dump; see the lines above for why");
        return false;
    }
    if (m.automap_initialized && m.config.minimap) {
        m.automap.show();
        if (m.subject_node) {
            m.automap.set_player_position(m.subject_node->getPosition(),
                                          m.subject_node->getOrientation());
            m.update_automap_icons();
            m.automap.update(m.subject_node->getPosition(), 0.0f);
        }
    }
    m.refresh_game_hud();
    m.game_hud.show();
    keep_game_hud = true;
    const bool ok = dump_frame(png_path);
    keep_game_hud = false;
    // show() above lasts only for the frame dump_frame() took: with nothing controllable,
    // hide the HUD again, as the automap below does.
    if (!m.player_control) {
        m.game_hud.hide();
        if (m.automap_initialized) m.automap.hide();
    }
    return ok;
}

/// Refresh everything the open menu shows from the live game state. The stats column's
/// figures are CombatSheet (CStatsMenu::update's calls, core/character_sheet.h); armour is
/// CCharacter::AC() over Inventory::armor_bonus.
void Renderer::Impl::update_menu() {
    load_stat_graphs();

    load_unit_types();

    if (player_inventory.effect_context().types == nullptr) refresh_inventory_effects();

    // `CStatsMenu::update`'s rows: `core/character_sheet.h`. The bare hands are
    // the attack `setLevel(LEVEL, true)` made at creation, as `player_unarmed_blow`
    // has it.
    const tl::core::AttackDescription own_attack = tl::core::own_attack(
        player_stats, player_stats.level, config.difficulty, /*player=*/true, stat_graphs);
    const tl::core::CombatSheet sheet = tl::core::combat_sheet(tl::core::make_display_inputs(
        player_character, player_inventory, unit_types, player_total(), &stat_graphs, &own_attack));
    player_character.set_hp(player_health.current, player_health.max);
    // The inventory tabs sort by unit type (`inventory_filter_of`) and the
    // skills tab names weapons from it, so every mode needs the hierarchy
    // before `update`, not only the skills tab.
    load_unit_types();
    inventory_menu.set_unit_types(&unit_types);
    inventory_menu.set_tray_caption_tests([this](const tl::core::ItemStats& item, bool pet_tray) {
        return tray_caption_context(item, pet_tray);
    });
    {
        const float bonus = player_total().effect_value(tl::core::item_effect::kPriceBonus);
        inventory_menu.set_trade_figures(player_inventory.gold(), &bonus);
    }
    inventory_menu.set_tray_row_context(tray_row_context());
    if (inventory_menu.vendor() == VendorScreen::Enchant ||
        inventory_menu.vendor() == VendorScreen::EnchantShrine) {
        item_roll_context();  // `GLOBALS.DAT`, once
        inventory_menu.set_enchant_rules(&item_roll_globals);
        inventory_menu.set_enchant_allowed(inventory_menu.vendor() != VendorScreen::EnchantShrine ||
                                           !enchant_shrine_spent());
    }
    // CStatsMenuXenon::vfunc_8, the page's refresh: the Stats
    // column's, which ends.
    sync_stats_page();
    if (stats_page.open) {
        inventory_menu.set_stats_view(
            tl::core::refresh_stats_page(stats_page, player_character, player_total(),
                                         player_inventory, unit_types, &stat_graphs, &own_attack),
            stats_page.selected);
    }
    inventory_menu.update(player_inventory, player_character, player_total(), sheet, stat_graphs,
                          pet, config.travel != nullptr ? config.travel->play_time : 0.0f,
                          &quest_log);
    if (inventory_menu.menu_mode() == MenuMode::Skills) {
        refresh_skill_effects();
        inventory_menu.update_skills_tab(player_skills, player_character);
    }
    // The details windows follow whatever the screen now shows.
    if (item_tip_shown && inventory_menu.visible() &&
        inventory_menu.menu_mode() == MenuMode::Inventory) {
        show_item_tooltips();
    }
}

// PORT: a headless verification switch (`--inventory-tab`).
void Renderer::set_inventory_tab(int tab) { impl_->inventory_menu.set_current_tab(tab); }

// PORT: a headless verification switch (`--inventory-dump`): the screen drawn to a PNG.
bool Renderer::dump_inventory(const std::string& png_path) {
    Impl& m = *impl_;
    if (!m.inventory_menu.ready()) {
        TL_LOG_WARN("no inventory menu to dump; see the lines above for why");
        return false;
    }
    m.inventory_menu.set_menu_mode(MenuMode::Inventory);
    m.update_menu();
    m.inventory_menu.show();
    m.update_character_preview(0.0f);
    m.refresh_game_hud();
    m.game_hud.show();
    keep_inventory = true;
    keep_game_hud = true;
    const bool ok = dump_frame(png_path);
    keep_inventory = false;
    keep_game_hud = false;
    return ok;
}

// PORT: a headless verification switch (`--talk-to`): the conversation drawn to a PNG.
bool Renderer::dump_talk(const std::string& png_path) {
    Impl& m = *impl_;
    // A second of the run loop's camera (`step_player` moves it once a frame),
    // so the close-up has eased in by the time the frame is taken.
    for (int i = 0; i < 60; ++i) {
        m.camera_delta = 1.0f / 60.0f;
        m.follow_subject();
    }
    m.camera_delta = 0.0f;
    const bool menu = m.inventory_menu.visible();
    if (menu) {
        m.update_menu();
        m.update_character_preview(0.0f);
    }
    // The HUD and minimap as the run loop would have them: out of the way
    // while a conversation holds the camera (step_hud_visibility()).
    const bool conversation = m.quest_dialog.visible() || !m.talk_unit.empty() ||
                              m.script_dialog_open() || m.hud.letterboxed();
    if (m.automap_initialized && m.config.minimap && !conversation) {
        m.automap.show();
        if (m.subject_node) m.automap.update(m.subject_node->getPosition(), 0.0f);
    }
    m.refresh_game_hud();
    if (m.game_hud.ready() && !conversation) m.game_hud.show();
    if (conversation) {
        m.game_hud.hide();
        if (m.automap_initialized) m.automap.hide();
    }
    keep_inventory = menu;
    keep_game_hud = m.game_hud.ready() && !conversation;
    const bool ok = dump_frame(png_path);
    keep_inventory = false;
    keep_game_hud = false;
    return ok;
}

// PORT: a headless verification switch (`--quest-dump`): the journal drawn to a PNG.
bool Renderer::dump_quests(const std::string& png_path) {
    Impl& m = *impl_;
    if (!m.inventory_menu.ready()) {
        TL_LOG_WARN("no quest menu to dump; see the lines above for why");
        return false;
    }
    m.load_quest_book();
    m.inventory_menu.show();
    m.inventory_menu.set_menu_mode(MenuMode::Quest);
    m.update_menu();
    m.refresh_game_hud();
    m.game_hud.show();
    keep_inventory = true;
    keep_game_hud = true;
    const bool ok = dump_frame(png_path);
    keep_inventory = false;
    keep_game_hud = false;
    return ok;
}

/// The `PORTRAIT` key on a unit's own record, as CQuestDialogMenu::setNPC reads it for
/// CGameUI::getImageFromImageSet. None authored, none drawn.
std::string Renderer::Impl::portrait_of(const std::string& unit) {
    // `unit_book` is keyed by the lower-cased name and only holds what a level
    // has already placed, so a quest giver who is not in this level - or who is
    // and has not been asked about yet - has to be indexed first.
    const std::string key = lowered(unit);
    index_unit(key);
    const tl::core::UnitStats* stats = unit_book.find(key);
    return stats != nullptr ? stats->portrait : std::string();
}

/// What `unit` says when the player walks up. CGameUI::updateIngameUI, AI state 0x1e:
/// only with a quest (CQuestManager::getQuestForNPC) does it call
/// CQuestDialogMenu::setNPC(unit, false, nullptr), which
/// shows getDialogsForNPC's first line: FLOATYTEXT over the head, else the quest window.
bool Renderer::Impl::open_quest_offer(const std::string& unit) {
    load_quest_book();
    load_stat_graphs();
    close_quest_offer();

    const std::vector<std::string> unit_quests = stats_for_unit(unit).quests;
    if (quest_log.quest_for_npc(unit, unit_quests, tl::core::volatile_random()) == nullptr) {
        return false;
    }
    const tl::core::QuestDefinition* quest = nullptr;
    tl::core::QuestSection section = tl::core::QuestSection::Intro;
    const tl::core::DialogLine* line =
        quest_log.dialog_for_npc(unit, unit_quests, tl::core::volatile_random(), &quest, &section);
    if (line == nullptr || quest == nullptr || line->texts.empty()) {
        TL_LOG_WARN("Error occured getting NPC dialog for character : %s", unit.c_str());
        return false;
    }

    // CQuestDialogMenu::setNPC picks the line by its quest. A quest not yet held
    // stands in with the seed accept_offered_quest() will use.
    const tl::core::QuestEntry* held_entry = quest_log.find(quest->name);
    tl::core::QuestEntry offered;
    if (held_entry == nullptr) {
        offered.definition = quest;
        offered.level = quest_level_of(*quest);
        offered.seed = offer_seed(*quest);
        if (const tl::core::QuestLog::Offered* rolled = quest_log.offered(*quest)) {
            offered.depth = rolled->depth;
        }
    }
    offered.dungeon_display_name = quest_log.dungeon_display_name(*quest);
    const std::string picked = tl::core::get_dialog(
        *line, held_entry != nullptr ? held_entry : &offered, tl::core::game_random());

    if (line->floaty) {
        // `setNPC`'s floaty branch hands the line's items over at once.
        hand_dialog_items(*quest, *line);
        floaty_line = tl::core::strip_dialog_directives(tl::core::strip_markup(picked));
        floaty_unit = unit;
        floaty_remaining = tl::core::kFloatySeconds;
        // `setNPC(unit, true, ..)` keeps the unit (+0xc0) without turning it off;
        // the menu's next `update` closes it with `OK`.
        talk_floaty_close = TalkFloatyClose{};
        talk_floaty_close.pending = true;
        talk_floaty_close.fresh = true;
        talk_floaty_close.machine = unit_machine(unit, talk_floaty_close.node);
        talk_floaty_close.complete = section == tl::core::QuestSection::Complete;
        talk_floaty_close.quest = quest->name;
        play_dialog_sound(line->sound);
        if (line->look_at_player) speaker_looks_at_player(unit);
        update_dialog_tags(unit, picked);
        TL_LOG_INFO("npc: %s says %s over their head about '%s'", unit.c_str(),
                    tl::core::quest_section_name(section), quest->name.c_str());
        return true;
    }
    if (!quest_dialog.ready()) return false;

    QuestOfferState state;
    state.portrait = portrait_of(unit);
    state.text = tl::core::strip_dialog_directives(picked);

    // **Only an `[INTRO]` is an offer.** A quest already taken, or finished,
    // has nothing to accept or decline - `CQuestDialogMenu` shows its single
    // `OK` for those, which is the pair of buttons the layout also authors.
    state.offer = section == tl::core::QuestSection::Intro;
    state.force_accept = state.offer && quest->force_accept;

    // What accepting would pay. The offer has to show the *same* figures the
    // log will hold, so the reward is rolled here off the seed the quest will
    // be accepted with rather than after the fact.
    const uint32_t seed = static_cast<uint32_t>(offer_seed(*quest));
    if (const tl::core::QuestEntry* held = quest_log.find(quest->name)) {
        state.reward = held->reward;
    } else if (state.offer) {
        state.reward = tl::core::calculate_quest_reward(quest->reward_spec, quest_level_of(*quest),
                                                        seed, stat_graphs);
    }

    // What the quest hands over, if the item book knows it.
    state.reward_item = quest_reward_item(*quest, quest_level_of(*quest), offer_seed(*quest));
    if (!state.reward_item.empty()) {
        ensure_item_index();
        if (const tl::core::ItemStats* item = items.find(state.reward_item)) {
            state.reward_icon = item->icon;
        }
    }

    quest_dialog.set_state(state);
    quest_dialog.show();
    set_quest_dialog_interactive(true);
    // `CQuestDialogMenu::setNPC` plays the chosen dialog's recording.
    play_dialog_sound(line->sound);
    // The player is now in AI state 0x1f with this unit as the target: the
    // camera's close-up (see `update_game_camera`).
    talk_unit = unit;
    talk_camera_mod = Ogre::Vector3(line->camera_mod_x, line->camera_mod_y, line->camera_mod_z);
    offered_quest = quest;
    offered_by = unit;
    offered_by_node = nearest_unit_node(unit);
    offered_makes_pet = line->make_pet_on_accept;
    offered_line = line;
    offered_section = section;
    // `setNPC`: the boxed line turns its unit off until the menu
    // lets it go.
    talk_menu_machine = unit_machine(unit, talk_menu_node);
    if (tl::core::LevelScript* owner = machine_script(talk_menu_machine)) {
        owner->menu_opened_on(talk_menu_node);
    }
    if (line->look_at_player) speaker_looks_at_player(unit);
    update_dialog_tags(unit, picked);
    TL_LOG_INFO("quest offer: %s says %s about '%s'%s", unit.c_str(),
                tl::core::quest_section_name(section), quest->name.c_str(),
                state.offer ? " (accept or decline)" : "");
    return true;
}

// CCharacter::approachAI: a quest giver is talked to
// (0x1e); else the first type with a screen names the AI state (enchanter 0x15, retirement
// 0x1b, gem recoverer 0x19, gem destroyer 0x1a, combiner 0x11, merchant 0x13, stash 0x24,
// shared stash 0x25); any other NPC is talked to. CGameUI::updateIngameUI
// opens what the state names.
void Renderer::Impl::interact_with(const std::string& unit) {
    load_quest_book();
    load_unit_types();
    const tl::core::UnitStats stats = stats_for_unit(unit);
    const uint32_t type = unit_types.id_of(stats.unittype);
    namespace ut = tl::core::unit_type;

    const bool has_quest =
        quest_log.quest_for_npc(unit, stats.quests, tl::core::volatile_random()) != nullptr;
    if (!has_quest) {
        if (unit_types.isa(type, ut::kEnchanter))
            return open_vendor_screen(unit, VendorScreen::Enchant);
        if (unit_types.isa(type, ut::kRetirementCharacter)) {
            // `updateIngameUI`'s state 0x1b: everything closed,
            // then the menu only for a player of `RETIREMENT_AGE`; otherwise
            // the modal, and the player's AI state 2.
            item_roll_context();
            if (!tl::core::can_retire(player_character.level(), item_roll_globals.retirement_age)) {
                close_quest_offer();
                open_modal(tl::core::kRetireTitle,
                           tl::core::cannot_retire_text(item_roll_globals.retirement_age));
                TL_LOG_INFO("retire: '%s' refuses level %d (age %d)", unit.c_str(),
                            player_character.level(), item_roll_globals.retirement_age);
                return;
            }
            return open_vendor_screen(unit, VendorScreen::Retire);
        }
        if (unit_types.isa(type, ut::kSocketableRecoverer)) {
            return open_vendor_screen(unit, VendorScreen::DestroyGems);
        }
        if (unit_types.isa(type, ut::kSocketableDestroyer)) {
            return open_vendor_screen(unit, VendorScreen::RecoverGems);
        }
        if (unit_types.isa(type, ut::kCombiner))
            return open_vendor_screen(unit, VendorScreen::Combine);
        if (unit_types.isa(type, ut::kMerchant))
            return open_vendor_screen(unit, VendorScreen::Merchant);
        if (unit_types.isa(type, ut::kStash)) return open_vendor_screen(unit, VendorScreen::Stash);
        if (unit_types.isa(type, ut::kSharedStash)) {
            return open_vendor_screen(unit, VendorScreen::SharedStash);
        }
        if (!unit_types.isa(type, ut::kNpc)) {
            TL_LOG_INFO("'%s' (%s) is nobody to talk to", unit.c_str(), stats.unittype.c_str());
            return;
        }
    }
    // State 0x1e.
    if (!open_quest_offer(unit)) {
        TL_LOG_INFO("'%s' has nothing to say", unit.c_str());
    }
}

// CGameUI::updateIngameUI's vendor handlers (merchant, combiner,
// enchanter, stashes): all else closed, the
// NPC's NPCGREET (slot 0x26, after 0.1 s), and the inventory menu with the vendor's screen.
void Renderer::Impl::open_vendor_screen(const std::string& unit, VendorScreen screen) {
    close_quest_offer();
    const std::string greet = stats_for_unit(unit).sounds[tl::core::SoundSlot::NpcGreet];
    queue_global_sample(tl::core::SoundSlot::NpcGreet, greet, 0.0f, 0.1f);
    trading_with = 0;
    combining_with = 0;
    inventory_menu.set_vendor_stock(nullptr, false);
    if (screen == VendorScreen::Combine) {
        // `CMerchantMenuXenon::vfunc_6`: the vendor's tray
        // shows the Transmuter's own inventory, the page is type 5.
        if (CombinerState* combiner = combiner_named(unit)) {
            for (const auto& entry : combiners) {
                if (&entry.second == combiner) combining_with = entry.first;
            }
            inventory_menu.set_vendor_stock(&combiner->inventory, false);
        } else {
            TL_LOG_WARN("npc: '%s' is not placed as a combiner", unit.c_str());
        }
    }
    if (screen == VendorScreen::Merchant) {
        if (MerchantState* merchant = merchant_named(unit)) {
            for (const auto& entry : merchants) {
                if (&entry.second == merchant) trading_with = entry.first;
            }
            inventory_menu.set_vendor_stock(&merchant->inventory, merchant->gambler);
        } else {
            TL_LOG_WARN("npc: '%s' has no stock", unit.c_str());
        }
    }
    if (screen == VendorScreen::Stash || screen == VendorScreen::SharedStash) {
        // the stash tray's owner is the player and its
        // inventory the player's `+0x9f4` (page 7) or `CSharedStash +8`
        // (page 8), whichever chest was used.
        inventory_menu.set_vendor_stock(housing_inventory_for(screen), false);
    }
    inventory_menu.open_vendor(screen);
    if (!inventory_menu.visible()) return;
    play_ui(tl::core::InterfaceSound::MenuOpen);
    if (screen == VendorScreen::Stash || screen == VendorScreen::SharedStash) {
        animate_stash(unit, true);
    }
    update_menu();
    update_character_preview(0.0f);
    TL_LOG_INFO("npc: '%s' opens the %d screen", unit.c_str(), static_cast<int>(screen));
}

bool Renderer::Impl::is_enchant_shrine(const std::string& unit) {
    TriggerProp prop;
    trigger_prop(unit, prop);
    if (prop.unittype.empty()) return false;
    load_unit_types();
    return unit_types.isa(prop.unittype, tl::core::unit_type::kEnchantShrine);
}

bool Renderer::Impl::enchant_shrine_spent() const {
    // `+0x1f0`, cleared when the one-shot reached its last state.
    const tl::core::LevelScript* owner =
        enchant_shrine_machine == kLevelMachine
            ? &script
            : (enchant_shrine_machine >= 0 &&
                       static_cast<size_t>(enchant_shrine_machine) < nested_scripts.size()
                   ? &nested_scripts[static_cast<size_t>(enchant_shrine_machine)].script
                   : nullptr);
    if (owner == nullptr) return false;
    for (const tl::core::LevelScript::UnitTriggerView& view : owner->every_unit_trigger()) {
        if (view.id == enchant_shrine_node) return !view.armed;
    }
    return false;
}

// CGameUI::updateIngameUI, AI state 0x16 with a target item:
// other menus closed, CEnchantMenuXenon::vfunc_6(0), the shrine,
// mode 1, and the player to AI state 0x1c. No NPCGREET.
void Renderer::Impl::open_enchant_shrine(int machine, int64_t node) {
    close_quest_offer();
    trading_with = 0;
    combining_with = 0;
    inventory_menu.set_vendor_stock(nullptr, false);
    enchant_shrine_machine = machine;
    enchant_shrine_node = node;
    inventory_menu.open_vendor(VendorScreen::EnchantShrine);
    if (!inventory_menu.visible()) return;
    play_ui(tl::core::InterfaceSound::MenuOpen);
    update_menu();
    update_character_preview(0.0f);
    TL_LOG_INFO("enchant shrine: unit trigger %lld opens the shrine's page",
                static_cast<long long>(node));
}

// A `Unit Trigger`'s prop that ISAs `FISHING HOLE` (0x7a): the test
// `CCharacter::getItemAI` makes before it
// calls `startFishing` instead of `interact` (core/fishing.h).
bool Renderer::Impl::is_fishing_hole(const std::string& unit) {
    TriggerProp prop;
    trigger_prop(unit, prop);
    if (prop.unittype.empty()) return false;
    load_unit_types();
    return unit_types.isa(prop.unittype, tl::core::unit_type::kFishingHole);
}

// CCharacter::getItemAI on a FISHING HOLE:
// CPlayer::startFishing. INVENTED: turned to the hole
// at once instead of waiting for facingTarget.
void Renderer::Impl::start_fishing(const Ogre::Vector3& at) {
    fishing_hole_at = at;
    const tl::core::Vec3 me = player.position();
    const float dx = at.x - me.x;
    const float dz = at.z - me.z;
    if (dx * dx + dz * dz > 1e-6f) player.place(me, std::atan2(dx, dz));

    // `CCharacter::startFishing`: unless the
    // character is already in 0x20..0x22, AI state 0x20, `FISHING_CAST` blended
    // in (0.2, speed 1) unless it plays already, and `FISHING_IDLE` queued
    // behind it.
    if (fishing_state == tl::core::FishingState::Off) {
        fishing_state = tl::core::FishingState::Cast;
        if (!fishing_animation_playing("FISHING_CAST")) {
            play_fishing_animation("FISHING_CAST", 1.0f, true, "FISHING_IDLE");
        }
    }
    // The rest of `CPlayer::startFishing`, whatever the state: the hooked fish
    // hidden and the bobber armed.
    build_fishing_gear();
    if (hooked_fish != nullptr) hooked_fish->setVisible(false);
    tl::core::start_fishing(bobber, tl::core::volatile_random());
    TL_LOG_INFO("fishing: cast at (%.2f, %.2f, %.2f), speed %.3f", static_cast<double>(at.x),
                static_cast<double>(at.y), static_cast<double>(at.z),
                static_cast<double>(bobber.speed));
}

void Renderer::Impl::stop_fishing() {
    // `setAIState(2)`: `CPlayer::setAIState` hides the rod node when
    // leaving 0x20..0x22; the weapons come back with `IDLE`'s `SHOWWEAPONS`
    // (and `fishingAICatch`'s own `+0x705` branch).
    fishing_state = tl::core::FishingState::Off;
    fishing_animation.clear();
    fishing_queued.clear();
    fishing_animation_once = false;
    fishing_animation_rate = 1.0f;
    fishing_screen.hide();
    player_animation.clear();
    follow_player_animation();
    update_fishing_gear();
}

// PORT: the fishing animations on the port's OGRE entities; which animation
// plays when is `core/fishing`'s (`CPlayer::fishingAI` and its states).
void Renderer::Impl::play_fishing_animation(const std::string& name, float rate, bool once,
                                            const std::string& then) {
    // Beside the body like every other animation (`FISHING_CAST.SKELETON` ...),
    // linked the first time it is asked for.
    if (animation_named(subject, name).empty()) link_subject_animation(name);
    fishing_animation = name;
    fishing_animation_rate = rate;
    fishing_animation_once = once;
    fishing_queued = then;
    player_animation.clear();
    follow_player_animation();
    if (!once) return;
    for (Ogre::Entity* entity : animated()) {
        const std::string here = animation_named(entity, name);
        if (!here.empty()) entity->getAnimationState(here)->setLoop(false);
    }
}

bool Renderer::Impl::fishing_animation_playing(const std::string& name) const {
    // `CGenericModel::animationPlayingSubstring`
    // for the one the fishing states play: a
    // looping one plays for as long as it is on.
    if (subject == nullptr || lowered(fishing_animation) != lowered(name)) return false;
    const std::string here = animation_named(subject, name);
    if (here.empty()) return false;
    const Ogre::AnimationState* state = subject->getAnimationState(here);
    if (!state->getEnabled()) return false;
    return state->getLoop() || state->getTimePosition() < state->getLength();
}

void Renderer::Impl::step_fishing(float delta, bool catch_press, bool stop_press) {
    using State = tl::core::FishingState;
    // `queueBlendAnimation`: the queued animation once the one-shot has run out.
    if (fishing_animation_once && !fishing_queued.empty() &&
        !fishing_animation_playing(fishing_animation)) {
        play_fishing_animation(fishing_queued, 1.0f, false, "");
    }

    switch (fishing_state) {
        case State::Off: return;
        case State::Cast:
            // `CCharacter::fishingAICast`:
            // 0x21 once the cast is over.
            if (!fishing_animation_playing("FISHING_CAST")) fishing_state = State::Waiting;
            break;
        case State::Waiting: {
            // CFishingMenu::onClick: false is catchFish (vtable +0x16c),
            // true stops unless in 0x22. INVENTED: the button-to-call dispatch.
            if (stop_press) {
                TL_LOG_INFO("fishing: stopped");
                stop_fishing();
                return;
            }
            if (catch_press) {
                catch_fish();
                break;
            }
            // `CPlayer::fishingAI`.
            const bool animating = fishing_animation_playing("FISHING_CAST") ||
                                   fishing_animation_playing("FISHING_BITE");
            if (tl::core::step_fishing(bobber, delta, animating, tl::core::volatile_random())) {
                // `CCharacter::fishingSplash`:
                // unless it plays already,
                // `FISHING_BITE` at half speed, `FISHING_IDLE` queued.
                if (!fishing_animation_playing("FISHING_BITE")) {
                    play_fishing_animation("FISHING_BITE", 0.5f, true, "FISHING_IDLE");
                }
                TL_LOG_INFO("fishing: a bite, %.3f s to catch it",
                            static_cast<double>(bobber.bite));
            }
            break;
        }
        case State::Catch:
            // `CPlayer::fishingAICatch`:
            // nothing while `FISHING_CATCH` plays.
            if (!fishing_animation_playing("FISHING_CATCH")) land_catch();
            break;
    }
}

void Renderer::Impl::catch_fish() {
    // `CPlayer::catchFish`: unless already in
    // 0x22, hooked when inside a bite, AI state 0x22 and `FISHING_CATCH`
    // blended in (0.2, speed 1). The `IDLE` it queues behind is what
    // `stop_fishing` returns to.
    if (fishing_state == tl::core::FishingState::Catch) return;
    tl::core::catch_fish(bobber);
    fishing_state = tl::core::FishingState::Catch;
    fishing_screen.hide();
    play_fishing_animation("FISHING_CATCH", 1.0f, true, "");
    TL_LOG_INFO("fishing: catch, %s", bobber.hooked ? "hooked" : "too early or too late");
}

void Renderer::Impl::land_catch() {
    // `CPlayer::fishingAICatch`.
    std::string text;
    if (!bobber.hooked) {
        text = tl::core::fishing_catch_text(false, false, std::string());
    } else {
        // The level's fish class, or `FISH_SPAWN` for a level with no template
        // (`+0x1d8` null); one roll, the player as character and finder, level
        // -1, flags 0 (`createUnitsBySpawnClass`).
        const std::string class_name =
            level_fish_spawn_class.empty() ? std::string("FISH_SPAWN") : level_fish_spawn_class;
        if (!loot_tables_built) {
            loot_tables_built = true;
            build_spawn_context(loot_tables, /*items_by_type=*/true);
        }
        const tl::core::SpawnContext& context = loot_tables.context;
        const tl::core::SpawnClass* spawn_class =
            context.get_spawn_class ? context.get_spawn_class(class_name) : nullptr;
        tl::core::SpawnCharacterContext me;
        me.level = player_character.level();
        me.is_hardcore = config.hardcore;
        // INVENTED: magic find (`getEffectValue(0x1b, 7)` on the player and the
        // pets) is not summed; 0 stands in, as for the quest rewards.
        std::vector<tl::core::ItemStats> made;
        if (spawn_class != nullptr) {
            made = tl::core::create_units_by_spawn_class(*spawn_class, 1, &me, &me, -1, 0,
                                                         tl::core::game_random(), context,
                                                         loot_unit_maker());
        } else {
            TL_LOG_WARN("fishing: no spawn class '%s'", class_name.c_str());
        }
        load_unit_types();
        bool made_a_fish = false;
        std::string name;
        for (const tl::core::ItemStats& item : made) {
            name = tl::core::item_name(item);
            if (unit_types.isa(item.unittype, tl::core::unit_type::kFish)) made_a_fish = true;
            // `CInventory::pickupEquipment(item, true)`, and the level at the
            // player's feet when the bag has no room.
            if (player_inventory.pickup(item, 1, unit_types) < 0) drop_item_near_player(item, 1);
            TL_LOG_INFO("fishing: '%s' from '%s'", item.name.c_str(), class_name.c_str());
        }
        text = tl::core::fishing_catch_text(true, made_a_fish, name);
        // CPlayer::fishingAICatch: incrementJournalStatistic(14, 1)
        count_journal(tl::core::JournalStatistic::FishCaught);
        update_menu();
        refresh_game_hud();
    }
    open_modal(tl::core::fishing_dialog_title(), text);
    TL_LOG_INFO("fishing: \"%s\"", text.c_str());
    stop_fishing();
}

void Renderer::Impl::open_modal(const std::string& title, const std::string& text) {
    // the buttons hidden, Ok shown, title and text set;
    // CModalMenu::setContents(false, text, 0, 0) inlined, Ok only.
    if (!modal_screen.ready()) return;
    modal_screen.set_caption("Title", title);
    modal_screen.set_caption("Dialog", text);
    modal_screen.set_hidden("Accept", true);
    modal_screen.set_hidden("Decline", true);
    modal_screen.set_hidden("Ok", false);
    modal_screen.refresh();
    modal_screen.show();
}

namespace {

/// The debug menu's rows, top to bottom.
enum DebugRow {
    kDebugGodspeed = 0,
    kDebugStats,
    kDebugGpuSkinning,
    kDebugGlThread,
    kDebugRenderSystem,
    kDebugLevel,
    kDebugLeave,
    kDebugRowCount
};
constexpr int kDebugLevelTarget = 100;

}  // namespace

void Renderer::Impl::set_godspeed(bool on) {
    god_mode = on;
    ensure_effect_data();
    // `CBaseUnit::removeAffix` / `addAffix`
    // on the player's manager, level 1 and
    // duration -1.
    const bool had = player_effects.delete_affix("PLAYER_SPEED_UP");
    bool has = false;
    if (on) has = player_effects.add_affix_by_name("PLAYER_SPEED_UP", 1, &affixes, -1.0f);
    const float running =
        tl::core::character_speed(player_stats.movement.running_speed, player_total());
    TL_LOG_INFO("debug: GODSPEED %s (god mode %s; speed boost %s; running speed %.2f)",
                on ? "on" : "off", god_mode ? "on" : "off",
                has   ? "added"
                : had ? "removed"
                : on  ? "NOT FOUND"
                      : "was not on",
                static_cast<double>(running));
}

// PORT: the debug FPS overlay, the port's own diagnostic.
void Renderer::Impl::set_stats_overlay(bool on) {
    stats_overlay_on = on;
    stats_frames = 0;
    stats_window_us = frame_clock.timer != nullptr ? frame_clock.timer->getMicroseconds() : 0;
    hud.set_stats(on ? "-- FPS" : "");
    hud.set_stats_visible(on);
    TL_LOG_INFO("debug: FPS overlay %s", on ? "on" : "off");
}

// PORT: the debug FPS overlay, the port's own diagnostic.
void Renderer::Impl::step_stats_overlay() {
    // L3 / 'F' on any screen (core/input.h), and config.txt keeps the choice.
    if (tl::core::take_fps_toggle()) {
        set_stats_overlay(!stats_overlay_on);
        (void)tl::core::save_run_config_value("fps", stats_overlay_on);
    }
    if (!stats_overlay_on || frame_clock.timer == nullptr) return;
    ++stats_frames;
    const unsigned long long now = frame_clock.timer->getMicroseconds();
    const unsigned long long elapsed = now - stats_window_us;
    if (elapsed < 500000) return;
    const double fps = static_cast<double>(stats_frames) * 1.0e6 / static_cast<double>(elapsed);
    const unsigned width = window != nullptr ? window->getWidth() : 0;
    const unsigned height = window != nullptr ? window->getHeight() : 0;
    char text[64];
    std::snprintf(text, sizeof text, "%.0f FPS  %ux%u", fps, width, height);
    hud.set_stats(text);
    stats_frames = 0;
    stats_window_us = now;
}

void Renderer::Impl::level_up_to(int level) {
    const int from = player_character.level();
    while (player_character.level() < level) {
        const int before = player_character.level();
        const uint64_t gate = player_character.experience_gate();
        const uint64_t held = player_character.experience();
        if (gate <= held) break;
        if (!award_player_experience(static_cast<int>(gate - held), player_health.dead) ||
            player_character.level() == before) {
            break;
        }
    }
    // Once, not once per level: one chime, one banner, one particle.
    if (player_character.level() != from) player_levelled_up();
    TL_LOG_INFO("debug: LEVEL %d asked, level %d -> %d", level, from, player_character.level());
}

void Renderer::Impl::outfit_debug_staff() {
    ensure_item_index();
    load_unit_types();
    if (player_inventory.effect_context().types == nullptr) refresh_inventory_effects();

    // The highest item level at or under the character's; the first by `NAME`
    // among equals (the book's own order).
    const tl::core::ItemStats* best = nullptr;
    for (const auto& [key, data] : items.all()) {
        if (data.is_gold() || data.spawn_rarity <= 0) continue;
        if (!unit_types.isa(data.unittype, tl::core::unit_type::kStaff) ||
            !unit_types.isa(data.unittype, tl::core::unit_type::kWeapon)) {
            continue;
        }
        if (data.level > player_character.level()) continue;
        if (best == nullptr || data.level > best->level) best = &data;
    }
    if (best == nullptr) {
        TL_LOG_WARN("debug: no staff at or under level %d", player_character.level());
        return;
    }
    tl::core::ItemStats staff = create_item(*best, false, /*random_enchant_allowed=*/false);
    // A unique's authored affixes can leave the copy unidentified, which no
    // character that answers to requirements may wear.
    staff.identified = true;
    const std::string name = tl::core::item_name(staff);

    // The requirements with no reductions and the stats with no effects: the
    // most the wear can ask for, whatever is worn or taken off on the way.
    const tl::core::RequirementReductions none{};
    const tl::core::StatType order[4] = {tl::core::StatType::Strength,
                                         tl::core::StatType::Dexterity, tl::core::StatType::Magic,
                                         tl::core::StatType::Defense};
    const int required[4] = {tl::core::item_strength_requirement(staff, &none, unit_types),
                             tl::core::item_dexterity_requirement(staff, &none, unit_types),
                             tl::core::item_magic_requirement(staff, &none, unit_types),
                             tl::core::item_defense_requirement(staff, &none, unit_types)};
    const int points = player_character.unspent_stat_points();
    for (int i = 0; i < 4; ++i) {
        const int short_by = required[i] - player_character.base_stat(order[i]);
        const int moved = std::min(std::max(0, short_by), player_character.unspent_stat_points());
        player_character.move_stat_point(order[i], moved);
        if (moved < short_by) {
            TL_LOG_WARN("debug: %d point(s) short of '%s''s requirement %d", short_by - moved,
                        name.c_str(), required[i]);
        }
    }
    const tl::core::StatType spread[4] = {tl::core::StatType::Magic, tl::core::StatType::Defense,
                                          tl::core::StatType::Strength,
                                          tl::core::StatType::Dexterity};
    for (int i = 0; player_character.unspent_stat_points() > 0; i = (i + 1) % 4) {
        player_character.move_stat_point(spread[i], 1);
    }
    TL_LOG_INFO("debug: %d stat point(s) spent: str %d dex %d mag %d def %d (requires %d/%d/%d/%d)",
                points, player_character.base_stat(order[0]), player_character.base_stat(order[1]),
                player_character.base_stat(order[2]), player_character.base_stat(order[3]),
                required[0], required[1], required[2], required[3]);

    const int slot = player_inventory.pickup(staff, 1, unit_types, /*equip_when_full=*/false);
    if (slot < 0) {
        TL_LOG_WARN("debug: no room in the bag for '%s'; it goes on the floor", name.c_str());
        drop_item_near_player(staff, 1);
    } else {
        TL_LOG_INFO("debug: '%s' (%s, item level %d) in the bag at slot %d", name.c_str(),
                    best->name.c_str(), staff.level, slot);
        wear_player_item(static_cast<size_t>(slot));
    }
    sync_equipped_weapon();
    refresh_skill_effects();
    update_menu();
    update_character_preview(0.0f);
}

void Renderer::Impl::debug_level_row(int level) {
    level_up_to(level);
    outfit_debug_staff();
}

void Renderer::Impl::refresh_debug_menu() {
    if (!modal_screen.ready()) return;
    const auto row = [this](int index, const std::string& text) {
        return std::string(debug_menu_row == index ? "> " : "   ") + text;
    };
    const bool gl3plus_wanted = RenderSystemChoice::gl3plus_wanted();
    const bool gl3plus_running =
        root->getRenderSystem()->getName().find("OpenGL 3+") != Ogre::String::npos;
    modal_screen.set_caption("Title", "Debug");
    modal_screen.set_caption(
        "Dialog",
        row(kDebugGodspeed, std::string("GODSPEED: ") + (god_mode ? "ON" : "OFF")) + "\n" +
            row(kDebugStats, std::string("FPS: ") + (stats_overlay_on ? "ON" : "OFF")) + "\n" +
            row(kDebugGpuSkinning,
                std::string("GPU SKINNING: ") + (!gpu_skinning.available() ? "N/A"
                                                 : GpuSkinning::enabled()  ? "ON"
                                                                           : "OFF")) +
            "\n" +
            // Asked as each GL context is made, so it
            // changes with the next level, not now.
            row(kDebugGlThread,
                std::string("GL THREAD: ") +
                    (!GlThread::driver_supports() ? "N/A"
                     : GlThread::wanted() == GlThread::started()
                         ? (GlThread::wanted() ? "ON" : "OFF")
                         : (GlThread::wanted() ? "ON NEXT LEVEL" : "OFF NEXT LEVEL"))) +
            "\n" +
            // Installed as each Root is made, so it too
            // changes with the next level.
            row(kDebugRenderSystem,
                std::string("RENDERER: ") +
                    (!RenderSystemChoice::selectable()
                         ? "N/A"
                         : std::string(gl3plus_wanted ? "GL3PLUS" : "GLES2") +
                               (gl3plus_wanted == gl3plus_running ? "" : " NEXT LEVEL"))) +
            "\n" +
            row(kDebugLevel, "LEVEL " + std::to_string(kDebugLevelTarget) + " (now " +
                                 std::to_string(player_character.level()) + ")") +
            "\n" + row(kDebugLeave, "Exit to main menu"));
    modal_screen.set_hidden("Accept", true);
    modal_screen.set_hidden("Decline", true);
    modal_screen.set_hidden("Ok", false);
    modal_screen.refresh();
}

// CStatsMenuXenon::vfunc_7
void Renderer::Impl::sync_stats_page() {
    const bool up = inventory_menu.visible() && inventory_menu.menu_mode() == MenuMode::Stats;
    if (up == stats_page.open) return;
    if (up) {
        tl::core::open_stats_page(stats_page, player_character.unspent_stat_points());
        return;
    }
    tl::core::close_stats_page(stats_page, player_character);
    refresh_game_hud();
}

void Renderer::Impl::close_master_menu() {
    // The open housing's `setOpen(false)`. A vendor's screen keeps the bag's
    // mode and closes with `InventoryClose` like the bag (`CMerchantMenuXenon`
    // `CStashMenuXenon`).
    const bool sound = inventory_menu.vendor() != VendorScreen::None ||
                       tl::core::housing_plays_close_sound(
                           static_cast<tl::core::HousingTab>(inventory_menu.menu_mode()));
    inventory_menu.hide();
    if (sound) play_ui(tl::core::InterfaceSound::MenuClose);
    sync_stats_page();
}

void Renderer::Impl::step_master_menu_tab(int direction) {
    // with false on the open screen,
    // then with true on the one the step names.
    if (tl::core::housing_plays_close_sound(
            static_cast<tl::core::HousingTab>(inventory_menu.menu_mode()))) {
        play_ui(tl::core::InterfaceSound::MenuClose);
    }
    if (direction > 0) inventory_menu.next_menu_tab();
    else inventory_menu.prev_menu_tab();
    play_ui(tl::core::InterfaceSound::MenuOpen);
    update_menu();
}

void Renderer::Impl::open_debug_menu() {
    if (inventory_menu.visible()) {
        inventory_menu.hide();
        sync_stats_page();
        if (automap_initialized && config.minimap) automap.show();
    }
    if (!modal_screen.ready()) {
        // No console interface data to draw it with: + is the switch itself.
        set_godspeed(!god_mode);
        return;
    }
    debug_menu_open = true;
    debug_menu_row = 0;
    debug_stick_held = true;
    // The level stands still under it, as under any modal screen
    // (`game_paused`).
    refresh_debug_menu();
    modal_screen.show();
    play_ui(tl::core::InterfaceSound::MenuOpen);
    TL_LOG_INFO("debug: menu open");
}

void Renderer::Impl::close_debug_menu() {
    if (!debug_menu_open) return;
    debug_menu_open = false;
    modal_screen.hide();
    play_ui(tl::core::InterfaceSound::MenuClose);
    TL_LOG_INFO("debug: menu closed");
}

void Renderer::Impl::drive_debug_menu(const tl::core::InputState& in) {
    if (std::abs(in.move_y) > 0.5f) {
        if (!debug_stick_held) {
            const int step = in.move_y > 0.0f ? -1 : 1;
            debug_menu_row = (debug_menu_row + step + kDebugRowCount) % kDebugRowCount;
            play_ui(tl::core::InterfaceSound::Navigate);
            refresh_debug_menu();
        }
        debug_stick_held = true;
    } else {
        debug_stick_held = false;
    }
    const bool confirm_edge = in.confirm && !confirm_held;
    const bool skip_edge = in.skip && !skip_held;
    if (confirm_edge) {
        if (debug_menu_row == kDebugGodspeed) {
            set_godspeed(!god_mode);
            refresh_debug_menu();
            refresh_game_hud();
        } else if (debug_menu_row == kDebugStats) {
            set_stats_overlay(!stats_overlay_on);
            refresh_debug_menu();
        } else if (debug_menu_row == kDebugGpuSkinning) {
            // Applied at the end of this frame (GpuSkinning::step), so the FPS
            // row beside it can show the difference straight away.
            if (gpu_skinning.available()) GpuSkinning::set_enabled(!GpuSkinning::enabled());
            refresh_debug_menu();
        } else if (debug_menu_row == kDebugGlThread) {
            if (GlThread::driver_supports()) GlThread::set_wanted(!GlThread::wanted());
            refresh_debug_menu();
        } else if (debug_menu_row == kDebugRenderSystem) {
            if (RenderSystemChoice::selectable()) {
                RenderSystemChoice::set_gl3plus_wanted(!RenderSystemChoice::gl3plus_wanted());
            }
            refresh_debug_menu();
        } else if (debug_menu_row == kDebugLevel) {
            debug_level_row(kDebugLevelTarget);
            refresh_debug_menu();
            refresh_game_hud();
        } else if (debug_menu_row == kDebugLeave) {
            close_debug_menu();
            quit_requested = true;
        }
    } else if (skip_edge) {
        close_debug_menu();
    }
}

void Renderer::Impl::refresh_fishing_screen() {
    // The console's `CGameUI` update: the ring is
    // up exactly while the player is in AI state 0x21.
    const bool wanted = fishing_state == tl::core::FishingState::Waiting;
    if (!fishing_screen.ready()) return;
    if (!wanted) {
        if (fishing_screen.visible()) fishing_screen.hide();
        return;
    }
    const float extent = tl::core::fishing_ring_extent(bobber);
    fishing_screen.set_extent("FishingRing", extent, extent);
    fishing_screen.set_tint("FishingRing", tl::core::fishing_ring_alert(bobber)
                                               ? Ogre::ColourValue(1.0f, 0.3f, 0.3f, 1.0f)
                                               : Ogre::ColourValue::White);
    fishing_screen.refresh();
    if (!fishing_screen.visible()) {
        // Every other menu closed first.
        if (inventory_menu.visible()) inventory_menu.hide();
        sync_stats_page();
        fishing_screen.show();
    }
}

namespace renderer_detail {
/// A mesh by full archive path, its materials read and qualified as a held
/// weapon's are.
Ogre::MeshPtr load_gear_mesh(const std::string& path, const std::string& fallback_dir) {
    const std::string dir = dir_of_asset(path);
    std::string stem = path;
    const size_t slash = stem.find_last_of('/');
    if (slash != std::string::npos) stem = stem.substr(slash + 1);
    const size_t dot = stem.find_last_of('.');
    if (dot != std::string::npos) stem.resize(dot);
    const std::string found = archive_path_in_dir(stem + ".MESH", dir);
    if (found.empty()) {
        TL_LOG_WARN("fishing: no '%s.MESH' under '%s'", stem.c_str(), dir.c_str());
        return Ogre::MeshPtr();
    }
    parse_sibling_material(found);
    Ogre::MeshPtr mesh;
    try {
        mesh = Ogre::MeshManager::getSingleton().load(found, kGameGroup);
    } catch (const Ogre::Exception& e) {
        TL_LOG_WARN("fishing: could not load '%s': %s", found.c_str(), e.getDescription().c_str());
        return Ogre::MeshPtr();
    }
    auto& materials = Ogre::MaterialManager::getSingleton();
    for (const Ogre::SubMesh* sub : mesh->getSubMeshes()) {
        qualify_texture_paths(materials.getByName(sub->getMaterialName(), kGameGroup), dir,
                              fallback_dir);
    }
    return mesh;
}

/// A point on the Catmull-Rom curve through `p`, `t` from 0 at the first point
/// to 1 at the last.
Ogre::Vector3 line_point(const Ogre::Vector3 (&p)[4], float t) {
    const float span = t * 3.0f;
    const int i = std::min(2, static_cast<int>(span));
    const float u = span - static_cast<float>(i);
    const Ogre::Vector3 p1 = p[i];
    const Ogre::Vector3 p2 = p[i + 1];
    const Ogre::Vector3 p0 = i > 0 ? p[i - 1] : p1 * 2.0f - p2;
    const Ogre::Vector3 p3 = i + 2 < 4 ? p[i + 2] : p2 * 2.0f - p1;
    const float u2 = u * u;
    const float u3 = u2 * u;
    return 0.5f * ((2.0f * p1) + (p2 - p0) * u + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * u2 +
                   (3.0f * p1 - p0 - 3.0f * p2 + p3) * u3);
}

}  // namespace renderer_detail
void Renderer::Impl::build_fishing_gear() {
    if (subject == nullptr || !subject->hasSkeleton()) return;
    // CPlayer::loadModel: the rod and the fish (IDLE),
    // hidden. CCharacter::getBones makes the rod node;
    // updateBonePositions keeps it on the right hand.
    if (fishing_rod == nullptr) {
        Ogre::SkeletonInstance* skeleton = subject->getSkeleton();
        const char* bone = skeleton->hasBone("tag_righthand")  ? "tag_righthand"
                           : skeleton->hasBone("Bip01 R Hand") ? "Bip01 R Hand"
                                                               : nullptr;
        Ogre::MeshPtr mesh =
            load_gear_mesh("media/models/fishingpole/fishingpole.mesh", subject_dir);
        if (bone != nullptr && mesh) {
            fishing_rod = scene->createEntity(mesh);
            rebind_materials(fishing_rod);
            subject->attachObjectToBone(bone, fishing_rod);
            fishing_rod->setVisible(false);
        }
    }
    if (hooked_fish == nullptr) {
        Ogre::MeshPtr mesh = load_gear_mesh("media/models/fish/fish.mesh", subject_dir);
        if (mesh) {
            hooked_fish = scene->createEntity(mesh);
            rebind_materials(hooked_fish);
            Ogre::SceneNode* node = scene->getRootSceneNode()->createChildSceneNode();
            node->attachObject(hooked_fish);
            hooked_fish->setVisible(false);
            const std::string skeleton =
                archive_path_in_dir("idle.SKELETON", dir_of_asset("media/models/fish/fish.mesh"));
            if (!skeleton.empty()) link_path_onto(hooked_fish, skeleton);
            const std::string idle = animation_named(hooked_fish, "IDLE");
            if (!idle.empty()) {
                hooked_fish_idle = hooked_fish->getAnimationState(idle);
                hooked_fish_idle->setEnabled(true);
                hooked_fish_idle->setLoop(true);
            }
        }
    }
    // CPlayer::CPlayer: a 16-point line list, hidden.
    // DIVERGENCE: its material is undefined (BaseWhite, ambient-lit); RTSS cannot light a
    // vertex without a normal, so it is drawn unlit in the ambient colour.
    if (fishing_line == nullptr) {
        auto& materials = Ogre::MaterialManager::getSingleton();
        if (!materials.resourceExists("FishingLine", kGameGroup)) {
            Ogre::MaterialPtr material = materials.create("FishingLine", kGameGroup);
            Ogre::Pass* pass = material->getTechnique(0)->getPass(0);
            pass->setLightingEnabled(false);
        }
        fishing_line = scene->createManualObject();
        fishing_line->setDynamic(true);
        fishing_line->estimateVertexCount(16);
        fishing_line->begin("FishingLine", Ogre::RenderOperation::OT_LINE_LIST, kGameGroup);
        for (int i = 0; i < 16; ++i) {
            fishing_line->position(Ogre::Vector3::ZERO);
            fishing_line->colour(Ogre::ColourValue::White);
        }
        fishing_line->end();
        fishing_line_node = scene->getRootSceneNode()->createChildSceneNode();
        fishing_line_node->attachObject(fishing_line);
        fishing_line->setVisible(false);
    }
}

// PORT: the fishing rod, the hooked fish and the line as OGRE objects, destroyed.
void Renderer::Impl::destroy_fishing_gear() {
    if (fishing_rod != nullptr) {
        if (subject != nullptr) subject->detachObjectFromBone(fishing_rod);
        scene->destroyEntity(fishing_rod);
        fishing_rod = nullptr;
    }
    if (hooked_fish != nullptr) {
        Ogre::SceneNode* node = hooked_fish->getParentSceneNode();
        if (node != nullptr) node->detachObject(hooked_fish);
        scene->destroyEntity(hooked_fish);
        if (node != nullptr) scene->destroySceneNode(node);
        hooked_fish = nullptr;
        hooked_fish_idle = nullptr;
    }
    if (fishing_line != nullptr) {
        if (fishing_line_node != nullptr) {
            fishing_line_node->detachObject(fishing_line);
            scene->destroySceneNode(fishing_line_node);
        }
        scene->destroyManualObject(fishing_line);
        fishing_line = nullptr;
        fishing_line_node = nullptr;
    }
}

void Renderer::Impl::update_fishing_gear() {
    const bool fishing = fishing_state != tl::core::FishingState::Off;
    // CPlayer::updateAnimation, CCharacter::updateAnimation:
    // in 0x20..0x22 the rod and line are shown. Weapons hide by the fishing
    // animations' HIDEWEAPONS keys.
    if (weapon_entity != nullptr) weapon_entity->setVisible(!fishing);
    for (Ogre::SceneNode* hand : hand_nodes) {
        if (hand != nullptr) hand->setVisible(!fishing);
    }
    if (fishing_rod != nullptr) fishing_rod->setVisible(fishing);
    if (!fishing) {
        if (hooked_fish != nullptr) hooked_fish->setVisible(false);
        if (fishing_line != nullptr) fishing_line->setVisible(false);
        return;
    }
    if (subject == nullptr || fishing_line == nullptr || !subject->hasSkeleton()) return;

    // `CPlayer::updateFishingLine`: the four
    // `tag_zfish*` bones of the character's own skeleton, in the world; the
    // lure never below the hole.
    Ogre::SkeletonInstance* skeleton = subject->getSkeleton();
    const char* const kBones[4] = {"tag_zfishpoleend", "tag_zfishcenter", "tag_zfishlure",
                                   "tag_zfishlureend"};
    Ogre::Vector3 points[4];
    const Ogre::Affine3& world = subject->getParentNode()->_getFullTransform();
    for (int i = 0; i < 4; ++i) {
        if (!skeleton->hasBone(kBones[i])) {
            fishing_line->setVisible(false);
            return;
        }
        points[i] = world * skeleton->getBone(kBones[i])->_getDerivedPosition();
    }
    if (points[2].y <= fishing_hole_at.y) points[2].y = fishing_hole_at.y;

    // Eight segments along the curve. INVENTED: CPath
    // stands as a Catmull-Rom cut
    // at eight equal steps.
    const Ogre::ColourValue colour = scene->getAmbientLight();
    fishing_line->beginUpdate(0);
    for (int s = 0; s < 8; ++s) {
        fishing_line->position(line_point(points, static_cast<float>(s) / 8.0f));
        fishing_line->colour(colour);
        fishing_line->position(line_point(points, static_cast<float>(s + 1) / 8.0f));
        fishing_line->colour(colour);
    }
    fishing_line->end();
    fishing_line->setVisible(true);

    // updateFishingLine: the fish at the line's end, up
    // back along the line, forward the player's - CPositionableObject::setOrientation.
    // INVENTED: "0.05 short of the end" on the stand-in spline.
    if (hooked_fish != nullptr) {
        hooked_fish->setVisible(bobber.hooked);
        if (bobber.hooked) {
            Ogre::SceneNode* node = hooked_fish->getParentSceneNode();
            node->setPosition(points[3]);
            Ogre::Vector3 up = line_point(points, 0.98f) - points[3];
            const Ogre::Vector3 player_forward =
                subject->getParentNode()->_getDerivedOrientation() * Ogre::Vector3::UNIT_Z;
            if (1e-08f < up.length()) {
                up.normalise();
                Ogre::Vector3 forward = up.crossProduct(player_forward.crossProduct(up));
                if (1e-08f < forward.length()) {
                    forward.normalise();
                    // `setOrientation` takes the up's part across the forward.
                    Ogre::Vector3 across = forward.crossProduct(up.crossProduct(forward));
                    across.normalise();
                    node->setOrientation(
                        Ogre::Quaternion(across.crossProduct(forward), across, forward));
                }
            }
        }
    }
}

void Renderer::Impl::apply_quest_item_removals() {
    const std::vector<tl::core::QuestItemRemoval> removals = quest_log.take_item_removals();
    if (removals.empty()) return;
    load_unit_types();
    bool changed = false;
    for (const tl::core::QuestItemRemoval& r : removals) {
        if (r.level_items) {
            // `CLevel::makeQuestUnitsUncool`: the level's
            // items of that quest that `ISA(QUESTITEM)` are marked, and the
            // level deletes a marked item on its next update - here at once.
            size_t gone = 0;
            for (size_t i = dropped_items.size(); i-- > 0;) {
                const DroppedWorldItem& dropped = dropped_items[i];
                if (dropped.is_gold || dropped.item.quest_guid != r.quest_guid) continue;
                if (!unit_types.isa(dropped.item.unittype, r.isa)) continue;
                if (static_cast<int>(i) == item_targeted) item_targeted = -1;
                else if (static_cast<int>(i) < item_targeted) --item_targeted;
                remove_ground_item(dropped_items[i]);
                dropped_items.erase(dropped_items.begin() + static_cast<long>(i));
                ++gone;
            }
            if (gone > 0) {
                TL_LOG_INFO("quest %lld takes %zu of its item(s) off the floor",
                            static_cast<long long>(r.quest_guid), gone);
            }
            continue;
        }
        // `removeQuestItemFromInventoryOrPetsInventoryByGuid`:
        // the player's inventory (`+0x490`),
        // then the first pet's (`+0x648`).
        const bool player = player_inventory.remove_quest_items(r.quest_guid, r.unit, r.whole_stack,
                                                                r.isa, unit_types);
        const bool animal =
            pet.alive() &&
            pet.bag().remove_quest_items(r.quest_guid, r.unit, r.whole_stack, r.isa, unit_types);
        if (player || animal) {
            TL_LOG_INFO("quest %lld takes back %s%s%s", static_cast<long long>(r.quest_guid),
                        r.unit.empty() ? "its quest items" : r.unit.c_str(),
                        player ? " from the player" : "", animal ? " from the pet" : "");
            changed = true;
        }
    }
    if (!changed) return;
    sync_equipped_weapon();
    update_menu();
    refresh_game_hud();
}

/// Finish a quest and hand over what it pays: `CQuestManager::completeQuest`,
/// whose payout `pay_quest_completions`
/// carries out.
bool Renderer::Impl::complete_quest(const std::string& name) {
    if (!quest_log.complete(name)) return false;
    quest_completed(name);
    return true;
}

// `CQuestDialogMenu::onClick` -> `CQuestManager::completeQuest`,
// with no test of the quest's state.
bool Renderer::Impl::complete_quest_by_dialog(const std::string& name) {
    load_quest_book();
    const tl::core::QuestDefinition* quest = quest_book.find(name);
    if (quest == nullptr || !quest_log.complete_by_dialog(*quest)) return false;
    quest_completed(name);
    return true;
}

void Renderer::Impl::quest_completed(const std::string& name) {
    pay_quest_completions();
    report_gates_waiting_on(name);
    // `CQuestManager::completeQuest`
    // refreshes the icons.
    update_automap_icons();
    update_npc_icons();
    update_menu();
    refresh_game_hud();
}

// CQuestManager::giveQuest, CQuestManager::removeQuest,
// CQuestManager::completeQuest:
// the quest controllers. DIVERGENCE: handed over once a frame after the change
// (the script's own Force Accept/Complete go at once, LevelScript::quest_notice).
void Renderer::Impl::deliver_quest_notices() {
    // The quests an accept or a completion has just made complete through
    // their lists of names: the banner `questEventUpdate` puts up for them.
    const std::vector<const tl::core::QuestDefinition*> finished = quest_log.take_finished();
    if (!finished.empty()) quest_event(finished);
    const std::vector<tl::core::QuestNotice> notices = quest_log.take_notices();
    for (const tl::core::QuestNotice& notice : notices) {
        for (const MachineView& view : machines()) {
            std::vector<tl::core::ScriptEvent> events;
            view.script->quest_notice(notice, quest_log.state(), events);
            if (!events.empty() && view.nodes != nullptr) apply_script_events(events, *view.nodes);
        }
    }
}

void Renderer::Impl::pay_quest_completions() {
    const std::vector<tl::core::QuestCompletion> done_list = quest_log.take_completions();
    for (const tl::core::QuestCompletion& done : done_list) {
        if (done.quest == nullptr) continue;
        // CQuest::giveRewardForQuest: incrementJournalStatistic(4, 1)
        count_journal(tl::core::JournalStatistic::QuestsCompleted);
        if (done.held && done.quest->reward_spec.any()) reward_player(done);
        TL_LOG_INFO("quest complete: %s%s", done.quest->name.c_str(),
                    done.held ? "" : " (not held - nothing to pay)");
        // No banner here: "Quest Complete" is questEventUpdate's; completeQuest's
        // setIsComplete sends event 4, which it skips for a complete quest.
    }
    // `CQuest::giveRewardForQuest` ends in `cleanUp(true)`, after the payout:
    // what the quests take back (`QuestLog` queued it with each completion),
    // and the pets their lines made.
    apply_quest_item_removals();
    for (const tl::core::QuestCompletion& done : done_list) {
        if (done.quest != nullptr) release_quest_pets(done);
    }
    if (!done_list.empty()) {
        update_menu();
        refresh_game_hud();
    }
}

/// CQuest::cleanUp -> CQuest::cleanUpDialog
/// -> CQuestDialog::cleanUp
/// per line: a shown line with REMOVE_AS_PET_ON_COMPLETE takes its UNITNAME pets
/// away; DESTROY_PET marks for deletion
/// and hides, else calculateNPCIcon.
/// `+0x76` is kept per quest (QuestEntry::dialog_interacted), as CQuest::isComplete reads.
void Renderer::Impl::release_quest_pets(const tl::core::QuestCompletion& done) {
    if (!done.dialog_interacted) return;
    const tl::core::QuestDefinition& quest = *done.quest;
    bool icons = false;
    for (const std::vector<tl::core::DialogLine>* lines :
         {&quest.intro, &quest.ret, &quest.complete, &quest.passive}) {
        for (const tl::core::DialogLine& line : *lines) {
            if (!line.remove_as_pet_on_complete || line.unit.empty()) continue;
            const auto release = [&](tl::core::LevelScript& machine) {
                for (const int64_t id : machine.release_player_pets(line.unit)) {
                    TL_LOG_INFO("pet: %s leaves the player as '%s' is cleaned up%s",
                                line.unit.c_str(), quest.name.c_str(),
                                line.destroy_pet ? ", and is removed" : "");
                    if (!line.destroy_pet) {
                        icons = true;
                        continue;
                    }
                    const auto range = scene_units.equal_range(machine.unit_of(id));
                    for (auto it = range.first; it != range.second; ++it) {
                        if (it->second.node != id || it->second.entity == nullptr) continue;
                        if (Ogre::SceneNode* node = it->second.entity->getParentSceneNode()) {
                            node->setVisible(false);
                        }
                    }
                    machine.remove_spawned_unit(id);
                }
            };
            release(script);
            for (NestedScript& nested : nested_scripts) release(nested.script);
        }
    }
    if (icons) update_npc_icons();
}

void Renderer::Impl::reward_player(const tl::core::QuestCompletion& done) {
    const tl::core::QuestReward& paid = done.reward;
    // `CQuestRewards::rewardPlayer`, in its
    // order. Slot 23 - `GoldBuy` - on the game UI's bank when there is gold,
    // then `giveGold(ceil(gold))`.
    if (paid.gold > 0) play_ui(tl::core::InterfaceSound::GoldBuy);
    player_inventory.give_gold(paid.gold);
    // CCharacter::giveGold: incrementJournalStatistic(1, amount)
    if (paid.gold > 0) count_journal(tl::core::JournalStatistic::GoldGathered, paid.gold);
    // Slot 36 - `Reveal` - only when there is experience and **no** gold
    // (`xp > 0 && gold == 0`).
    if (paid.xp > 0 && paid.gold == 0) play_ui(tl::core::InterfaceSound::Reveal);
    if (paid.xp > 0 && award_player_experience(paid.xp, player_health.dead)) {
        player_levelled_up();
    }
    if (paid.fame > 0 && award_player_fame(paid.fame)) {
        // `CCharacter::fameUp`.
        play_voice(tl::core::SoundSlot::VoiceFameUp);
    }
    TL_LOG_INFO("quest reward: %s - %d gold, %d xp, %d fame", done.quest->name.c_str(), paid.gold,
                paid.xp, paid.fame);

    // The items (`+0x60`), each into the bag if `canPickup` lets it, else put
    // down at the player's feet (`CLevel::addUnit` at `getPosition`) - and the
    // list emptied, so a second payout of the same rewards has none.
    if (!done.reward_items) {
        sync_equipped_weapon();
        return;
    }
    ensure_item_index();
    load_unit_types();
    // The units calculateRewards made at the offer, or
    // now (as CQuest::getQuestRewardGold and kin do on an empty list).
    std::vector<tl::core::QuestRewardUnit> units = done.reward_units;
    if (units.empty()) {
        tl::core::QuestLog::Offered rolled;
        rolled.level = done.level;
        rolled.seed = done.seed;
        quest_log.calculate_rewards(*done.quest, rolled);
        units = std::move(rolled.reward_units);
    }
    for (const tl::core::QuestRewardUnit& unit : units) {
        if (!unit.is_item) {
            // INVENTED: a `CMonster` reward becomes a pet (`addPet`);
            // this engine has one pet and no second, so it is not handed over.
            TL_LOG_WARN("quest reward: '%s' is not an item this port can hand over",
                        unit.unit.c_str());
            continue;
        }
        tl::core::ItemStats copy = unit.item;
        if (player_inventory.can_pickup(copy, unit_types, false)) {
            player_inventory.pickup(copy, 1, unit_types);
            TL_LOG_INFO("quest reward: %s into the bag", copy.display_name.c_str());
        } else if (subject_node != nullptr) {
            place_ground_item(copy, 1, subject_node->getPosition(), Ogre::Quaternion::IDENTITY, {});
            TL_LOG_INFO("quest reward: %s at the player's feet, the bag being full",
                        copy.display_name.c_str());
        }
    }
    sync_equipped_weapon();
}

/// The banner and the narrator, when a quest is taken or finished: CQuestManager::giveQuest
/// and questEventUpdate,
/// slot 31 QUESTRECEIVED_SOUND or 32 QUESTCOMPLETED_SOUND on the player's bank,
/// then the big text and the quest's name for 2 s.
void Renderer::Impl::announce_quest(const tl::core::QuestDefinition& quest, bool complete) {
    // `NO_NOTIFICATION` is the quest saying to keep quiet, and
    // `SHOWQUESTCOMPLETE` gates the completion half specifically.
    if (quest.no_notification) return;
    if (complete && !quest.show_quest_complete) return;

    set_big_text(complete ? "Quest Complete" : "New Quest", kQuestBannerSeconds);
    set_small_text(
        tl::core::strip_markup(quest.display_name.empty() ? quest.name : quest.display_name),
        kQuestBannerSeconds);

    // Two sounds from two banks: the narrator, and the quest
    // manager's own chime.
    play_ui(complete ? tl::core::InterfaceSound::QuestCompleted
                     : tl::core::InterfaceSound::QuestReceived);

    const tl::core::SoundSlot slot =
        complete ? tl::core::SoundSlot::QuestCompleted : tl::core::SoundSlot::QuestReceived;
    // Worth a line: a banner that does not come up and a narrator who does not
    // speak look identical from the sofa, and the two have different causes.
    TL_LOG_INFO("quest banner: '%s' / '%s' for %.1fs, %s -> '%s'", big_banner.text.c_str(),
                small_banner.text.c_str(), static_cast<double>(kQuestBannerSeconds),
                tl::core::kSoundSlotKeys[static_cast<size_t>(slot)],
                player_stats.sounds[slot].c_str());
    // Volume 1 and a second's delay: `giveQuest`,
    // `questEventUpdate` (both 1.0f).
    play_voice(slot, 1.0f, 1.0f);
    refresh_game_hud();
}

/// Report that a group in this level was gated on `quest`. Gates are evaluated only at
/// load, as in the original: CRandomGroup::getGroupCanBeCreated()
/// is called
/// only by CRandomGroupDescriptor::descriptorSceneLoaded and rollRandomGroups.
void Renderer::Impl::report_gates_waiting_on(const std::string& quest) {
    if (quest.empty() || script_layout.nodes.empty()) return;

    size_t groups = 0;
    for (const tl::formats::CmpNode& node : script_layout.nodes) {
        if (node.type != tl::formats::cmp_type::kGroup) continue;
        if (script_gated.count(node.id) == 0) continue;
        // This group specifically, not any group that happens to be open now:
        // completing the intro opens several at once, and reporting the whole
        // set against each quest in turn says nothing.
        if (!tl::core::group_gates_on(node, quest)) continue;
        if (!tl::core::group_is_open(node, quest_log.state())) continue;
        ++groups;
    }
    if (groups == 0) return;
    if (!reported_gates.insert(quest).second) return;

    TL_LOG_INFO(
        "quest '%s': %zu group(s) this level held back would now be open - "
        "they stay closed until the level is loaded again, which is what the "
        "original does too (see report_gates_waiting_on)",
        quest.c_str(), groups);
}

/// The unit a quest's `TREASURECLASS` rolls to, or empty. CQuestRewards::calculateRewards:
/// CSpawnClassParser::getSpawnClass, CSpawnClass::rollSpawnClass,
/// CResourceManager::createUnit. A class picking by UNITTYPE gives empty. The quest's own
/// seed, so offer and payout agree.
std::vector<std::string> Renderer::Impl::quest_reward_items(const tl::core::QuestDefinition& quest,
                                                            int level, int32_t seed) {
    std::vector<std::string> handed;
    // The units the quest's offer already made (`CQuestRewards +0x44`), when
    // an NPC has offered it.
    const tl::core::QuestEntry* held = quest_log.find(quest.name);
    const tl::core::QuestLog::Offered* rolled = quest_log.offered(quest);
    const std::vector<tl::core::QuestRewardUnit>* made =
        held != nullptr && !held->reward_units.empty()       ? &held->reward_units
        : rolled != nullptr && !rolled->reward_units.empty() ? &rolled->reward_units
                                                             : nullptr;
    if (made != nullptr) {
        for (const tl::core::QuestRewardUnit& unit : *made) handed.push_back(unit.unit);
        return handed;
    }
    // +0x30, the [UNITREWARD] units, from the groups CQuestRewards::parseRewardTag
    // names. DIVERGENCE: a fifth group reads past the
    // four names on the console; here nothing. INVENTED: IDENTIFY not applied.
    for (const tl::core::QuestRewardSpec::UnitReward& reward : quest.reward_spec.unit_rewards) {
        if (reward.unit.empty()) continue;
        bool found = false;
        if (reward.resource_group == 0) {
            ensure_item_index();
            found = items.find(reward.unit) != nullptr;
        } else if (reward.resource_group < 4) {
            index_unit(reward.unit);
            found = unit_book.find(reward.unit) != nullptr;
        }
        if (found) handed.push_back(reward.unit);
    }
    // `+0x28`, `TREASURECLASS`: rolled for the player (`getPlayer` twice, as
    // character and finder) at the quest's level plus -1..2, with the quest's
    // seed plus 0x272b, no flags.
    if (!quest.reward_spec.treasure_class.empty()) {
        SpawnTables tables;
        build_spawn_context(tables, /*items_by_type=*/true);
        std::optional<tl::core::SpawnClass> table =
            load_spawn_class(quest.reward_spec.treasure_class, tables.context);
        if (table) {
            tl::core::RunicRandom rng(0u);
            const tl::core::TreasureRoll roll = tl::core::quest_treasure_roll(level, seed, rng);
            tl::core::SpawnCharacterContext player;
            player.level = player_character.level();
            // INVENTED: magic find (`getEffectValue(0x1b, 7)` on the player
            // and the pets) is not summed here; 0 stands in.
            std::vector<std::string> rolled;
            std::vector<bool> enchant;
            table->roll_spawn_class(rolled, &enchant, &player, &player, roll.level, roll.seed,
                                    /*level_bonus=*/0, /*rarity_override=*/-1, /*magic_find=*/0,
                                    /*flags=*/0, rng, tables.context);
            handed.insert(handed.end(), rolled.begin(), rolled.end());
        }
    }
    return handed;
}

// INVENTED: one item, the first `CQuestRewards::calculateRewards` hands over
// (`quest_reward_items`): the port's offer window and log keep a single reward
// tile.
std::string Renderer::Impl::quest_reward_item(const tl::core::QuestDefinition& quest, int level,
                                              int32_t seed) {
    const std::vector<std::string> handed = quest_reward_items(quest, level, seed);
    return handed.empty() ? std::string() : handed.front();
}

// What `CQuestRewards::calculateRewards`
// reaches outside the quest for: `createUnit`, the `TREASURECLASS` roll on
// `g_Rand` for the player as character and finder, `enchant(true)`, the
// difficulty.
tl::core::QuestRewardWorld Renderer::Impl::quest_reward_world() {
    tl::core::QuestRewardWorld world;
    // A level and a player: the port's quests are offered and paid in play.
    world.ready = [] { return true; };
    load_stat_graphs();
    world.graphs = &stat_graphs;
    world.difficulty = [this] { return config.difficulty; };
    world.create_unit = [this](const std::string& unit, uint32_t resource_group,
                               tl::core::QuestRewardUnit& out) {
        out.unit = unit;
        if (resource_group != 0) {
            // INVENTED: `createUnit` of a monster, player or prop (its own
            // `unitInit` draws) is not made; the port hands over items only.
            index_unit(unit);
            return unit_book.find(unit) != nullptr;
        }
        ensure_item_index();
        const tl::core::ItemStats* item = items.find(unit);
        if (item == nullptr) return false;
        // `createUnit(rm, data, 0, false)` -> `createItem(guid, false, true,
        // false)`: `+0x20a` set, no forced enchant.
        out.is_item = true;
        out.item = create_item(*item, false, true, false);
        return true;
    };
    world.roll_spawn_class = [this](const std::string& spawn_class, int32_t level, uint32_t seed,
                                    std::vector<std::string>& units, std::vector<bool>& enchant) {
        SpawnTables tables;
        build_spawn_context(tables, /*items_by_type=*/true);
        std::optional<tl::core::SpawnClass> table = load_spawn_class(spawn_class, tables.context);
        if (!table) return;
        tl::core::SpawnCharacterContext player;
        player.level = player_character.level();
        // INVENTED: magic find (`getEffectValue(0x1b, 7)` on the player and
        // the pets) is not summed here; 0 stands in.
        table->roll_spawn_class(units, &enchant, &player, &player, level, seed,
                                /*level_bonus=*/0, /*rarity_override=*/-1, /*magic_find=*/0,
                                /*flags=*/0, tl::core::game_random(), tables.context);
    };
    world.enchant = [this](tl::core::ItemStats& item) {
        tl::core::enchant_item(item, true, item_roll_context());
    };
    return world;
}

// PORT: the reward item's icon looked up for the port's journal and offer window.
void Renderer::Impl::resolve_reward_item(tl::core::QuestEntry& entry) {
    if (entry.definition == nullptr) return;
    entry.reward_item = quest_reward_item(*entry.definition, entry.level, entry.seed);
    if (entry.reward_item.empty()) return;
    ensure_item_index();
    if (const tl::core::ItemStats* item = items.find(entry.reward_item)) {
        entry.reward_icon = item->icon;
    }
}

void Renderer::Impl::close_quest_offer() {
    talk_unit.clear();
    // Whatever closes the window lets its unit go (`setEnabled(+0x1e9)`).
    if (tl::core::LevelScript* owner = machine_script(talk_menu_machine)) {
        owner->menu_released(talk_menu_node);
    }
    talk_menu_machine = kNoMachine;
    talk_menu_node = 0;
    offered_section = tl::core::QuestSection::Intro;
    offered_line = nullptr;
    // `CQuestDialogMenu::setOpen(false)`
    // stops the dialog's sound.
    if (quest_dialog.visible()) stop_dialog_sound();
    quest_dialog.hide();
    set_quest_dialog_interactive(false);
    offered_quest = nullptr;
    offered_by.clear();
    offered_by_node = 0;
    offered_makes_pet = false;
}

// `CQuestDialogMenu::onClick` on the unit of
// the player's talk.
void Renderer::Impl::talk_menu_closed(int machine, int64_t node, int32_t menu_event,
                                      bool complete_line, const std::string& quest) {
    tl::core::LevelScript* owner = node != 0 ? machine_script(machine) : nullptr;
    if (owner == nullptr) {
        // A unit no machine runs has nothing to hear; the quest still completes.
        if (complete_line) complete_quest_by_dialog(quest);
        return;
    }
    std::vector<tl::core::ScriptEvent> events;
    owner->menu_closed_on(node, menu_event, complete_line, quest, quest_log.state(), events);
    apply_machine_events(machine, events);
}

void Renderer::Impl::finish_talk_window(int32_t menu_event) {
    // A [COMPLETE] line closing finishes the quest: CQuestDialogMenu::onClick
    // calls CQuestManager::completeQuest on that branch.
    const bool complete_line =
        menu_event == 2 && offered_section == tl::core::QuestSection::Complete;
    const std::string quest = offered_quest != nullptr ? offered_quest->name : std::string();
    const tl::core::QuestDefinition* definition = offered_quest;
    const tl::core::DialogLine* line = offered_line;
    const int machine = talk_menu_machine;
    const int64_t node = talk_menu_node;
    close_quest_offer();
    talk_menu_closed(machine, node, menu_event, complete_line, quest);
    // `onClick`'s `OK` (10): `completeQuest` for a `[COMPLETE]` line, then
    // `giveOrRemoveItems`. `Accept` hands them over in
    // accept_offered_quest(); `Decline` (8) never does.
    if (menu_event == 2 && definition != nullptr && line != nullptr) {
        hand_dialog_items(*definition, *line);
    }
}

namespace {

/// One of the unit `name` out of `bag`, the first slot holding it:
/// with a count of -1, as calls it for one bag.
/// INVENTED: the slot order walks was not read; slot order stands in.
bool remove_one_named(tl::core::Inventory& bag, const std::string& name) {
    const std::vector<tl::core::InventorySlot>& slots = bag.slots();
    for (size_t i = 0; i < slots.size(); ++i) {
        if (slots[i].count > 0 && tl::core::upper_ascii(slots[i].item.name) == name) {
            return bag.remove_item(i, 1);
        }
    }
    return false;
}

}  // namespace

bool Renderer::Impl::hand_dialog_items(const tl::core::QuestDefinition& quest,
                                       const tl::core::DialogLine& line) {
    if (quest_log.items_handed(quest, line)) return false;
    quest_log.set_items_handed(quest, line);
    if (line.take_items.empty() && line.give_items.empty()) return true;
    ensure_item_index();
    load_unit_types();
    // The `[TAKEITEM]`s:, the console's own half of
    // `CQuestDialogItem::dialogComplete` for
    // a group parsed with `false`.
    for (const tl::core::DialogLine::Item& item : line.take_items) {
        if (item.gold) {
            player_inventory.give_gold(-item.count);
            continue;
        }
        // COUNT times, the UNIT_GUID out of the bag, the first pet's, and X360 +0x9f4's.
        // INVENTED: +0x9f4's bag, and the pets dismissed.
        for (int i = 0; i < item.count; ++i) {
            if (!remove_one_named(player_inventory, item.name) && pet.alive()) {
                remove_one_named(pet.bag(), item.name);
            }
        }
        TL_LOG_INFO("quest %s takes %d x %s", quest.name.c_str(), item.count, item.name.c_str());
    }
    // [GIVEITEM]: CQuestDialogItem::dialogComplete: giveGold(COUNT) or one
    // unit; an item carries the quest's GUID and goes to the bag or the floor.
    for (const tl::core::DialogLine::Item& item : line.give_items) {
        if (item.gold) {
            player_inventory.give_gold(item.count);
            continue;
        }
        if (const tl::core::ItemStats* stats = items.find(item.name)) {
            tl::core::ItemStats made = *stats;
            made.quest_guid = quest.guid;
            if (player_inventory.pickup(made, 1, unit_types) < 0) drop_item_near_player(made, 1);
            TL_LOG_INFO("quest %s gives %s", quest.name.c_str(), item.name.c_str());
        } else {
            // INVENTED: a character the console makes the player's pet
            // (`CCharacter::addPet`) is not made here; nothing else is kept.
            TL_LOG_WARN("quest %s: '%s' is no item; not given", quest.name.c_str(),
                        item.name.c_str());
        }
    }
    sync_equipped_weapon();
    update_menu();
    refresh_game_hud();
    return true;
}

/// Take the quest the window offered (`CQuestDialogMenu::acceptQuest`).
///
void Renderer::Impl::tell_player_motion(tl::core::LevelScript& machine) {
    // What a pet's `idleAI` reads off its master: `+0xb4` (sin, 0, cos) of the
    // facing, `+0x264` moving, `+0x321` a stick pushed past the walking speed,
    // `walkingSpeed()`/`runningSpeed()` (the speeds `step_player` last set),
    // and `+0x340`, the player's target.
    machine.set_player_motion(
        tl::core::Vec3{std::sin(player.facing()), 0.0f, std::cos(player.facing())}, player.moving(),
        player.moving() && player.speed() > player.stats().walking_speed);
    machine.set_player_speeds(player.stats().walking_speed, player.stats().running_speed);
    // `CGameClient +0x38d0 == 1`, a game being played (a player under control,
    // as `kill_player` reads it), and the interface's half of
    // `getUIIsInCinematic`.
    machine.set_game_playing(player_control);
    machine.set_ui_cinematic(ui_in_cinematic());
    // `player_target_node` is dropped once that unit dies or leaves.
    tl::core::LevelScript* owner =
        player_target_node != 0 ? script_owning(player_target_node) : nullptr;
    if (owner == nullptr || owner->unit_is_dead(player_target_node)) {
        player_target_node = 0;
        owner = nullptr;
    }
    machine.set_player_attack_target(owner, owner != nullptr ? player_target_node : 0);
}

// PORT: which scene unit of this name is nearest the player, to say which node
// offered a quest; the console's offer carries the unit itself
// (`CQuestDialogMenu::setNPC`), the port's scene keys units by name.
int64_t Renderer::Impl::nearest_unit_node(const std::string& unit) const {
    if (subject_node == nullptr) return 0;
    const Ogre::Vector3 me = subject_node->getPosition();
    const std::string key = lowered(unit);
    int64_t best = 0;
    float best_d = 0.0f;
    auto consider = [&](const std::string& name, int64_t node, const Ogre::Entity* entity) {
        if (node == 0 || entity == nullptr || lowered(name) != key) return;
        const Ogre::SceneNode* sn = entity->getParentSceneNode();
        if (sn == nullptr) return;
        const float d = (sn->_getDerivedPosition() - me).squaredLength();
        if (best == 0 || d < best_d) {
            best = node;
            best_d = d;
        }
    };
    for (const auto& entry : scene_units)
        consider(entry.first, entry.second.node, entry.second.entity);
    for (const auto& owner : spawned) {
        for (const SpawnedUnit& made : owner.second) consider(made.unit, made.id, made.entity);
    }
    return best;
}

bool Renderer::Impl::accept_offered_quest(const tl::core::QuestDefinition& quest) {
    const uint32_t seed = static_cast<uint32_t>(offer_seed(quest));
    if (take_quest(quest, quest_level_of(quest), seed)) {
        if (tl::core::QuestEntry* held = quest_log.find(quest.name)) {
            held->dialog_interacted = true;
            resolve_reward_item(*held);
        }
        TL_LOG_INFO("quest accepted: %s", quest.name.c_str());
        announce_quest(quest, false);
        // `CQuestDialog::giveOrRemoveItems`,
        // which the Accept runs, once a line: the items, then with
        // `MAKE_PET_ON_ACCEPT` (+0x78) the speaker becomes the player's pet
        // (`CCharacter::addPet`) and its NPC icon is worked out again.
        const bool first_time = offered_line == nullptr || hand_dialog_items(quest, *offered_line);
        if (offered_makes_pet && first_time) {
            tl::core::LevelScript* owner =
                offered_by_node != 0 ? script_owning(offered_by_node) : nullptr;
            std::vector<tl::core::ScriptEvent> events;
            if (owner != nullptr &&
                owner->make_player_pet(offered_by_node, quest_log.state(), events)) {
                TL_LOG_INFO("pet: %s follows the player", offered_by.c_str());
                if (!events.empty()) apply_script_events(events, scripted_nodes);
            } else {
                TL_LOG_WARN("pet: %s (node %lld) is no character a machine runs; it stays",
                            offered_by.c_str(), static_cast<long long>(offered_by_node));
            }
        }
        // `CQuestManager::giveQuest` ends in
        // `updateAutomapIcons` and `CLevel::updateNPCIcons`.
        update_automap_icons();
        update_npc_icons();
    } else {
        // `CQuestDialogMenu::onClick` opens the "Your quest log is
        // full!" modal here; this port has no modal, so the refusal
        // is the `Error` sound and a log line.
        play_ui(tl::core::InterfaceSound::Error);
        TL_LOG_WARN("your quest log is full: '%s' not taken", quest.name.c_str());
        update_menu();
        return false;
    }
    update_menu();
    return true;
}

/// A boxed line a Quest Controller opened, in the quest window
/// (CGameUI::forceInteractiveDialog -> setNPC).
bool Renderer::Impl::open_script_dialog(const tl::core::ScriptEvent& event,
                                        const std::string& text) {
    load_quest_book();
    load_stat_graphs();
    close_quest_offer();
    if (!quest_dialog.ready()) return false;
    const tl::core::QuestDefinition* quest = quest_book.find(event.quest);
    QuestOfferState state;
    state.portrait = portrait_of(event.unit);
    state.text = text;
    state.offer = event.offer && quest != nullptr;
    state.force_accept = state.offer && quest->force_accept;
    if (quest != nullptr) {
        const uint32_t seed = static_cast<uint32_t>(offer_seed(*quest));
        if (const tl::core::QuestEntry* held = quest_log.find(quest->name)) {
            state.reward = held->reward;
        } else if (state.offer) {
            state.reward = tl::core::calculate_quest_reward(
                quest->reward_spec, quest_level_of(*quest), seed, stat_graphs);
        }
        state.reward_item = quest_reward_item(*quest, quest_level_of(*quest), offer_seed(*quest));
        if (!state.reward_item.empty()) {
            ensure_item_index();
            if (const tl::core::ItemStats* item = items.find(state.reward_item)) {
                state.reward_icon = item->icon;
            }
        }
    }
    quest_dialog.set_state(state);
    quest_dialog.show();
    set_quest_dialog_interactive(true);
    offered_quest = quest;
    offered_by = event.unit;
    script_dialog_window = true;
    script_talk_node = event.speaker_node;
    script_talk_camera_mod =
        Ogre::Vector3(event.camera_mod.x, event.camera_mod.y, event.camera_mod.z);
    return true;
}

// PORT: the renderer's interface to `main.cpp` (headless runs).
bool Renderer::quest_offer_visible() const { return impl_->quest_dialog.visible(); }

// PORT: the renderer's interface to `main.cpp` (headless runs).
bool Renderer::death_screen_open() const { return impl_->death_screen_up; }

// PORT: a headless verification switch (`--die`): the resurrection choice pressed.
bool Renderer::choose_resurrection(int choice) {
    Impl& m = *impl_;
    if (!m.death_screen_up || choice < 0 || choice > 2) return false;
    m.death_selected = choice;
    m.refresh_death_screen();
    m.choose_resurrection(static_cast<tl::core::ResurrectChoice>(choice));
    return true;
}

// PORT: a headless verification switch (`--die`): the death screen drawn to a PNG.
bool Renderer::dump_death_screen(const std::string& png_path) {
    if (!impl_->death_screen_up) {
        TL_LOG_ERROR("--die: the death screen is not up");
        return false;
    }
    return dump_frame(png_path);
}

// PORT: a headless verification switch (`--die`): the player hurt by their whole health.
void Renderer::kill_player(const std::string& from) {
    Impl& m = *impl_;
    m.hurt_player(std::max(1.0f, m.player_health.current), from);
}

// PORT: a headless verification switch (`--quest-offer`).
bool Renderer::open_quest_offer(const std::string& unit) { return impl_->open_quest_offer(unit); }

bool Renderer::answer_quest_offer(bool accept) {
    Impl& m = *impl_;
    if (m.offered_quest == nullptr) return false;

    const tl::core::QuestDefinition* quest = m.offered_quest;
    const bool was_offer = m.quest_dialog.state().offer;
    if (!accept || !was_offer) {
        // `CQuestDialogMenu::broadcastAcceptedOrDeclined(false, npc)`: declining
        // costs nothing and the quest may be offered again.
        m.finish_talk_window(!was_offer ? 2 : 4);
        return true;
    }
    const uint32_t seed = static_cast<uint32_t>(m.offer_seed(*quest));
    if (!m.take_quest(*quest, m.quest_level_of(*quest), seed)) {
        // The one refusal there is. "Your quest log is full!" is a modal in the
        // original; here it is a log line, because this port has no modal.
        // `onClick` then goes on as a decline (event 4).
        TL_LOG_WARN("your quest log is full: '%s' not taken", quest->name.c_str());
        m.finish_talk_window(4);
        return false;
    }
    m.finish_talk_window(3);
    if (tl::core::QuestEntry* entry = m.quest_log.find(quest->name)) {
        // `CQuest::setQuestAcceptDialogInteracted(true)` - the offer has been
        // answered, which is what completes a quest that asks for nothing else.
        entry->dialog_interacted = true;
    }
    const tl::core::QuestEntry* entry = m.quest_log.find(quest->name);
    TL_LOG_INFO("quest accepted: %s - %d gold, %d xp, %d fame", quest->name.c_str(),
                entry != nullptr ? entry->reward.gold : 0, entry != nullptr ? entry->reward.xp : 0,
                entry != nullptr ? entry->reward.fame : 0);
    m.announce_quest(*quest, false);
    return true;
}

// PORT: the offer window's reward page turned (the renderer's interface).
void Renderer::toggle_quest_rewards() { impl_->quest_dialog.toggle_rewards(); }

// PORT: a headless verification switch (`--complete-quest`).
bool Renderer::complete_quest(const std::string& name) { return impl_->complete_quest(name); }

// PORT: a headless verification switch (`--award-xp`).
void Renderer::award_experience(uint64_t amount) {
    Impl& m = *impl_;
    const bool levelled = m.award_player_experience(static_cast<int>(amount), m.player_health.dead);
    TL_LOG_INFO("--award-xp: %llu experience, level %d%s", static_cast<unsigned long long>(amount),
                m.player_character.level(), levelled ? " (levelled up)" : "");
    if (levelled) m.player_levelled_up();
}

bool Renderer::dump_quest_offer(const std::string& unit, const std::string& png_path) {
    Impl& m = *impl_;
    if (!m.quest_dialog.ready()) {
        TL_LOG_WARN("no quest offer window to dump; see the lines above for why");
        return false;
    }
    if (!m.open_quest_offer(unit)) {
        TL_LOG_WARN("nobody called '%s' has a quest to offer", unit.c_str());
        return false;
    }
    // CGameUI::forceInteractiveDialog stops the player and reframes the camera; letterboxed.
    m.hud.set_letterboxed(true);
    // The bars slide in over 0.2 s (`CInteractiveMenu::update`); the picture is
    // of them come to rest.
    m.hud.update_bars(0.2f);
    const bool ok = dump_frame(png_path);
    m.hud.set_letterboxed(false);
    return ok;
}

// PORT: a headless verification switch (`--accept-quest`): the player pressing Accept.
bool Renderer::accept_quest(const std::string& name, int level) {
    Impl& m = *impl_;
    m.load_quest_book();
    m.load_stat_graphs();

    const tl::core::QuestDefinition* quest = m.quest_book.find(name);
    if (quest == nullptr) {
        TL_LOG_WARN("no quest called '%s' under media/Quests/", name.c_str());
        return false;
    }

    // DIVERGENCE: the seed (CQuest +0x1c) is randomIntegerBetweenVolatile(0, INT_MAX) in
    // the original; here derived from the quest's GUID, so a verification run repeats.
    const uint32_t seed = static_cast<uint32_t>(quest->guid) ^ 0x9e3779b9u;

    if (!m.take_quest(*quest, level, seed)) {
        TL_LOG_WARN("quest log is full: '%s' not taken", quest->name.c_str());
        return false;
    }
    if (tl::core::QuestEntry* held = m.quest_log.find(quest->name)) {
        // `--accept-quest` is the player pressing Accept, so the offer counts
        // as answered - which is what completes a quest that asks for nothing.
        held->dialog_interacted = true;
        m.resolve_reward_item(*held);
    }
    const tl::core::QuestEntry* entry = m.quest_log.find(quest->name);
    TL_LOG_INFO("quest accepted: %s - %d gold, %d xp, %d fame", quest->name.c_str(),
                entry != nullptr ? entry->reward.gold : 0, entry != nullptr ? entry->reward.xp : 0,
                entry != nullptr ? entry->reward.fame : 0);
    m.announce_quest(*quest, false);
    return true;
}

// PORT: a headless verification switch (`--quest-slot`).
void Renderer::set_quest_tab_selection(int index) {
    impl_->inventory_menu.set_selected_quest(index);
}

// PORT: a headless verification switch (`--stats-dump`): the stats page drawn to a PNG.
bool Renderer::dump_stats(const std::string& png_path) {
    Impl& m = *impl_;
    if (!m.inventory_menu.ready()) {
        TL_LOG_WARN("no stats menu to dump; see the lines above for why");
        return false;
    }
    m.inventory_menu.show();
    m.inventory_menu.set_menu_mode(MenuMode::Stats);
    m.update_menu();
    m.update_character_preview(0.0f);
    m.refresh_game_hud();
    m.game_hud.show();
    keep_inventory = true;
    keep_game_hud = true;
    const bool ok = dump_frame(png_path);
    keep_inventory = false;
    keep_game_hud = false;
    return ok;
}

// PORT: a headless verification switch (`--skills-dump`): the skills page drawn to a PNG.
bool Renderer::dump_skills(const std::string& png_path) {
    Impl& m = *impl_;
    if (!m.inventory_menu.ready()) {
        TL_LOG_WARN("no skills menu to dump; see the lines above for why");
        return false;
    }
    // A text sidecar of the description column: the frame is not reproducible, the text is.
    m.write_skill_text_sidecar(png_path);
    m.inventory_menu.show();
    m.inventory_menu.set_menu_mode(MenuMode::Skills);
    m.update_menu();
    m.update_character_preview(0.0f);
    m.refresh_game_hud();
    m.game_hud.show();
    keep_inventory = true;
    keep_game_hud = true;
    const bool ok = dump_frame(png_path);
    keep_inventory = false;
    keep_game_hud = false;
    return ok;
}

// PORT: a headless verification switch (`--skill-select`): the skills page's cursor put on a skill by name.
bool Renderer::select_skill(const std::string& name) {
    Impl& m = *impl_;
    const std::string want = lowered(name);
    if (want.empty()) return false;
    for (const tl::core::KnownSkill& skill : m.player_skills.skills()) {
        const std::string display =
            skill.def != nullptr ? lowered(skill.def->display_name) : std::string();
        if (lowered(skill.entry.name).find(want) == std::string::npos &&
            display.find(want) == std::string::npos) {
            continue;
        }
        tl::core::SkillCursor cursor;
        cursor.pane = skill.entry.pane;
        cursor.row = skill.entry.row;
        cursor.column = skill.entry.column;
        m.inventory_menu.set_skill_cursor(cursor);
        TL_LOG_INFO("skills: cursor on '%s' (pane %d, row %d, column %d)", skill.entry.name.c_str(),
                    cursor.pane, cursor.row, cursor.column);
        return true;
    }
    TL_LOG_WARN("skills: no skill of this class matches '%s'", name.c_str());
    return false;
}

// PORT: a headless verification switch (`--skill-map`).
void Renderer::set_skill_map_open(bool open) { impl_->inventory_menu.set_skill_map_open(open); }

// PORT: the `--skills-dump` sidecar: the description column as text, so a run can be compared.
void Renderer::Impl::write_skill_text_sidecar(const std::string& png_path) {
    const tl::core::KnownSkill* skill = inventory_menu.selected_skill(player_skills);
    if (skill == nullptr || skill->def == nullptr) return;
    refresh_skill_effects();
    std::string text = skill->def->name + "\n";
    text += tl::core::skill_description_text(*skill, -1, unit_types, skill_effects);
    const bool has_next = skill->effective != 0 && skill->effective < skill->max_rank();
    if (has_next) {
        text += "--- next level ---\n";
        text += tl::core::skill_description_text(*skill, static_cast<int>(skill->effective) + 1,
                                                 unit_types, skill_effects);
    }
    const std::string path = png_path + ".txt";
    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) {
        TL_LOG_WARN("skills: could not write '%s'", path.c_str());
        return;
    }
    std::fwrite(text.data(), 1, text.size(), file);
    std::fclose(file);
    TL_LOG_INFO("skills: wrote %s (%zu bytes)", path.c_str(), text.size());
}

// PORT: a headless verification switch (`--learn-skill`): a point spent through
// `SkillBook::learn`, found by name.
bool Renderer::learn_skill(const std::string& name) {
    Impl& m = *impl_;
    const std::string want = lowered(name);
    if (want.empty()) return false;
    for (tl::core::KnownSkill& skill : m.player_skills.skills()) {
        const std::string display =
            skill.def != nullptr ? lowered(skill.def->display_name) : std::string();
        if (lowered(skill.entry.name).find(want) == std::string::npos &&
            display.find(want) == std::string::npos) {
            continue;
        }
        const uint32_t level = static_cast<uint32_t>(std::max(1, m.player_character.level()));
        if (!m.player_skills.learn(skill, level, m.player_character.unspent_skill_points_ref())) {
            TL_LOG_WARN("skills: '%s' cannot take a point now (level %u, %d point(s) left)",
                        skill.entry.name.c_str(), level, m.player_character.unspent_skill_points());
            return false;
        }
        TL_LOG_INFO("skills: learned '%s' to level %u, %d point(s) left", skill.entry.name.c_str(),
                    skill.invested, m.player_character.unspent_skill_points());
        m.start_player_passives();
        return true;
    }
    TL_LOG_WARN("skills: no skill of this class matches '%s'", name.c_str());
    return false;
}

bool Renderer::cast_skill(const std::string& name) {
    Impl& m = *impl_;
    const std::string want = lowered(name);
    if (want.empty()) return false;
    for (const tl::core::KnownSkill& skill : m.player_skills.skills()) {
        const std::string display =
            skill.def != nullptr ? lowered(skill.def->display_name) : std::string();
        if (lowered(skill.entry.name).find(want) == std::string::npos &&
            display.find(want) == std::string::npos) {
            continue;
        }
        // The press's aim, as the pad leaves it: the given point,
        // or with nothing pushed one unit ahead.
        m.set_player_target_direction(m.pad_skill_target.has_value()
                                          ? *m.pad_skill_target
                                          : m.pad_skill_point(skill, 0.0f, 0.0f, 0.0f, 0.0f));
        return m.cast_player_skill(skill, true);
    }
    TL_LOG_WARN("skills: no skill of this class matches '%s'", name.c_str());
    return false;
}

// PORT: a headless verification switch (`--cast-at-point`): a skill cast at a point.
bool Renderer::cast_skill_at(const std::string& name, float x, float z) {
    Impl& m = *impl_;
    m.pad_skill_target = tl::core::Vec3{x, 0.0f, z};
    const bool cast = cast_skill(name);
    m.pad_skill_target.reset();
    return cast;
}

bool Renderer::face_nearest_foe() {
    Impl& m = *impl_;
    if (m.subject_node == nullptr) return false;
    const int64_t foe = m.hostile_in_reach(40.0f);
    Ogre::Vector3 at = Ogre::Vector3::ZERO;
    bool found = false;
    for (tl::core::LevelScript* machine : m.level_machines) {
        tl::core::Vec3 p;
        if (foe != 0 && machine->unit_position(foe, p)) {
            at = Ogre::Vector3(p.x, p.y, p.z);
            found = true;
            break;
        }
    }
    if (!found) {
        TL_LOG_WARN("--cast-at-foe: no enemy within 40 units");
        return false;
    }
    m.set_player_target(foe);
    const Ogre::Vector3 me = m.subject_node->getPosition();
    const float yaw = std::atan2(at.x - me.x, at.z - me.z);
    m.player.place(m.player.position(), yaw);
    m.subject_node->setOrientation(Ogre::Quaternion(Ogre::Radian(yaw), Ogre::Vector3::UNIT_Y));
    TL_LOG_INFO("--cast-at-foe: facing unit %lld at (%.2f, %.2f, %.2f), %.2f away",
                static_cast<long long>(foe), static_cast<double>(at.x), static_cast<double>(at.y),
                static_cast<double>(at.z), static_cast<double>(me.distance(at)));
    return true;
}

// PORT: a headless verification switch (`--map-skill`): a skill put on a button
// through `SkillBook::map`, found by name.
bool Renderer::map_skill(const std::string& name, const std::string& button) {
    Impl& m = *impl_;
    const std::string which = lowered(button);
    std::optional<tl::core::SkillButton> mapped;
    if (which == "y") mapped = tl::core::SkillButton::Y;
    else if (which == "b") mapped = tl::core::SkillButton::B;
    else if (which == "lt") mapped = tl::core::SkillButton::LT;
    else if (which == "rt") mapped = tl::core::SkillButton::RT;
    if (!mapped.has_value()) {
        TL_LOG_WARN("skills: '%s' is not a button a skill goes on (y, b, lt, rt)", button.c_str());
        return false;
    }
    const std::string want = lowered(name);
    if (want.empty()) return false;
    for (tl::core::KnownSkill& skill : m.player_skills.skills()) {
        const std::string display =
            skill.def != nullptr ? lowered(skill.def->display_name) : std::string();
        if (lowered(skill.entry.name).find(want) == std::string::npos &&
            display.find(want) == std::string::npos) {
            continue;
        }
        const int set = m.player_skills.skill_set();
        if (!m.player_skills.map(skill, *mapped, set)) {
            TL_LOG_WARN("skills: '%s' cannot go on %s", skill.entry.name.c_str(), button.c_str());
            return false;
        }
        TL_LOG_INFO("skills: '%s' mapped to function slot %d", skill.entry.name.c_str(),
                    tl::core::skill_button_slot(*mapped, set));
        m.refresh_game_hud();
        return true;
    }
    TL_LOG_WARN("skills: no skill of this class matches '%s'", name.c_str());
    return false;
}

namespace {

/// A hotbar button by its pad name, as `--hotbar` and `--hold-hotbar` take it.
tl::core::HotbarButton hotbar_button_named(const std::string& which) {
    using tl::core::HotbarButton;
    if (which == "lb") return HotbarButton::LB;
    if (which == "rb") return HotbarButton::RB;
    if (which == "y") return HotbarButton::Y;
    if (which == "b") return HotbarButton::B;
    if (which == "lt") return HotbarButton::LT;
    if (which == "rt") return HotbarButton::RT;
    return HotbarButton::None;
}

}  // namespace

bool Renderer::press_hotbar(const std::string& button) {
    Impl& m = *impl_;
    using tl::core::HotbarButton;
    const std::string which = lowered(button);
    const HotbarButton pressed_button = hotbar_button_named(which);
    if (pressed_button == HotbarButton::None) {
        TL_LOG_WARN("hotbar: '%s' is not a hotbar button (lb, rb, y, b, lt, rt)", button.c_str());
        return false;
    }
    std::array<bool, 7> pressed{};
    pressed[static_cast<size_t>(pressed_button)] = true;
    TL_LOG_INFO("hotbar: %s pressed, %.0f mana", which.c_str(),
                static_cast<double>(m.player_character.mp()));
    m.step_hotbar(pressed, std::array<bool, 7>{});
    m.refresh_game_hud();
    return true;
}

bool Renderer::hold_hotbar(const std::string& button, float seconds) {
    Impl& m = *impl_;
    using tl::core::HotbarButton;
    const std::string which = lowered(button);
    const HotbarButton held_button = hotbar_button_named(which);
    if (held_button == HotbarButton::None || !m.player_control) {
        TL_LOG_WARN("hotbar: cannot hold '%s' (lb, rb, y, b, lt, rt, with player control)",
                    button.c_str());
        return false;
    }
    face_nearest_foe();
    const float mana_before = m.player_character.mp();
    // The same fixed step as `advance_scene`, rendered so the frame events run.
    constexpr float kStep = 1.0f / 60.0f;
    const int steps = static_cast<int>(seconds / kStep);
    for (int i = 0; i < steps; ++i) {
        std::array<bool, 7> pressed{};
        std::array<bool, 7> held{};
        (i == 0 ? pressed : held)[static_cast<size_t>(held_button)] = true;
        m.release_held_skill(pressed, held, false);
        m.step_hotbar(pressed, held);
        m.advance(kStep);
        if (!m.root->renderOneFrame(kStep)) break;
    }
    TL_LOG_INFO("hotbar: %s held %.2fs, '%s' active, mana %.1f to %.1f", which.c_str(),
                static_cast<double>(seconds), m.player_active_skill.skill.c_str(),
                static_cast<double>(mana_before), static_cast<double>(m.player_character.mp()));
    m.refresh_game_hud();
    return true;
}

bool Renderer::dump_effects(const std::string& txt_path) {
    Impl& m = *impl_;
    std::string text;
    char line[512];
    for (const tl::core::LiveAffix& affix : m.player_effects.affixes()) {
        std::snprintf(line, sizeof(line), "affix %s duration %.3f from %s\n", affix.name.c_str(),
                      static_cast<double>(affix.duration),
                      affix.skill.empty() ? "-" : affix.skill.c_str());
        text += line;
    }
    for (const tl::core::LiveEffect* live : m.player_effects.all_effects()) {
        const tl::core::EffectTypeInfo* info = m.effect_types.at(live->effect.type);
        std::snprintf(
            line, sizeof(line),
            "effect %s type %d (%s) activation %d value %.3f duration %.3f "
            "elapsed %.3f affix %s skill %s\n",
            live->effect.name.c_str(), live->effect.type,
            info != nullptr ? info->name.c_str() : "?", static_cast<int>(live->effect.activation),
            static_cast<double>(live->value), static_cast<double>(live->effect.duration),
            static_cast<double>(live->elapsed), live->affix.empty() ? "-" : live->affix.c_str(),
            live->skill.empty() ? "-" : live->skill.c_str());
        text += line;
    }
    std::FILE* file = std::fopen(txt_path.c_str(), "wb");
    if (file == nullptr) {
        TL_LOG_WARN("effects: could not write '%s'", txt_path.c_str());
        return false;
    }
    std::fwrite(text.data(), 1, text.size(), file);
    std::fclose(file);
    TL_LOG_INFO("effects: wrote %s (%zu affix(es), %zu effect(s))", txt_path.c_str(),
                m.player_effects.affixes().size(), m.player_effects.all_effects().size());
    return true;
}

// PORT: the menu shown or hidden (the renderer's interface).
void Renderer::toggle_inventory() {
    Impl& m = *impl_;
    if (m.inventory_menu.ready()) {
        m.inventory_menu.toggle();
        if (m.inventory_menu.visible()) {
            m.update_menu();
            m.update_character_preview(0.0f);
            if (m.automap_initialized) m.automap.hide();
        } else {
            if (m.automap_initialized && m.config.minimap) m.automap.show();
        }
    }
}

// PORT: the renderer's interface to `main.cpp` (headless runs).
void Renderer::open_inventory() {
    Impl& m = *impl_;
    if (m.inventory_menu.ready()) {
        m.update_menu();
        m.inventory_menu.show();
        m.update_character_preview(0.0f);
        if (m.automap_initialized) m.automap.hide();
    }
}

// PORT: the renderer's interface to `main.cpp` (headless runs).
void Renderer::close_inventory() {
    Impl& m = *impl_;
    m.inventory_menu.hide();
    m.sync_stats_page();
    if (m.automap_initialized && m.config.minimap) m.automap.show();
}

// PORT: the renderer's interface to `main.cpp` (headless runs).
bool Renderer::inventory_visible() const { return impl_->inventory_menu.visible(); }

void Renderer::Impl::init_character_preview() {
    if (preview_texture || scene == nullptr) return;

    preview_texture = Ogre::TextureManager::getSingleton().createManual(
        "tl/inv/char_preview", Ogre::RGN_DEFAULT, Ogre::TEX_TYPE_2D, 512, 512, 0,
        Ogre::PF_BYTE_RGBA, Ogre::TU_RENDERTARGET);

    auto& mats = Ogre::MaterialManager::getSingleton();
    Ogre::MaterialPtr mat = mats.getByName("tl/inv/preview_material", Ogre::RGN_INTERNAL);
    if (!mat) mat = mats.getByName("tl/inv/preview_material", Ogre::RGN_DEFAULT);
    if (!mat) {
        mat = mats.create("tl/inv/preview_material", Ogre::RGN_INTERNAL);
        Ogre::Pass* pass = mat->getTechnique(0)->getPass(0);
        pass->setLightingEnabled(false);
        pass->setDepthCheckEnabled(false);
        pass->setDepthWriteEnabled(false);
        pass->setCullingMode(Ogre::CULL_NONE);
        Ogre::TextureUnitState* unit = pass->createTextureUnitState();
        unit->setTexture(preview_texture);
        unit->setColourOperationEx(Ogre::LBX_SOURCE1, Ogre::LBS_TEXTURE, Ogre::LBS_CURRENT);
        unit->setAlphaOperation(Ogre::LBX_SOURCE1, Ogre::LBS_TEXTURE, Ogre::LBS_CURRENT);
        unit->setTextureAddressingMode(Ogre::TextureUnitState::TAM_CLAMP);
    } else {
        auto* pass = mat->getTechnique(0)->getPass(0);
        if (pass->getNumTextureUnitStates() > 0) {
            pass->getTextureUnitState(0)->setTexture(preview_texture);
        }
    }

    preview_node = scene->getRootSceneNode()->createChildSceneNode(
        "tl/inv/preview_node", Ogre::Vector3(0.0f, 10000.0f, 0.0f));
    preview_node->setOrientation(Ogre::Quaternion(Ogre::Degree(15.0f), Ogre::Vector3::UNIT_Y));

    preview_light = scene->createLight("tl/inv/preview_light");
    preview_light->setType(Ogre::Light::LT_POINT);
    preview_light->setDiffuseColour(Ogre::ColourValue(1.15f, 1.10f, 1.05f));
    preview_light->setSpecularColour(Ogre::ColourValue(0.5f, 0.5f, 0.5f));
    preview_light->setAttenuation(50.0f, 1.0f, 0.04f, 0.001f);

    preview_light_node =
        scene->getRootSceneNode()->createChildSceneNode("tl/inv/preview_light_node");
    preview_light_node->attachObject(preview_light);
    preview_light_node->setPosition(0.8f, 10002.5f, 2.5f);

    preview_camera = scene->createCamera("tl/inv/preview_camera");
    preview_camera->setNearClipDistance(0.1f);
    preview_camera->setFarClipDistance(50.0f);
    preview_camera->setAspectRatio(266.0f / 412.0f);

    preview_camera_node =
        scene->getRootSceneNode()->createChildSceneNode("tl/inv/preview_camera_node");
    preview_camera_node->attachObject(preview_camera);

    Ogre::RenderTexture* rt = preview_texture->getBuffer()->getRenderTarget();
    Ogre::Viewport* vp = rt->addViewport(preview_camera);
    apply_scheme(vp);
    vp->setClearEveryFrame(true);
    vp->setBackgroundColour(Ogre::ColourValue(0.05f, 0.05f, 0.05f, 1.0f));
    vp->setOverlaysEnabled(false);
    // Without it the viewport draws Automap::mRingQuad (an uncullable Rectangle2D meant
    // for the automap's viewport only) full-screen.
    vp->setVisibilityMask(kVisibilityWorld);
    rt->setAutoUpdated(false);
}

void Renderer::Impl::update_character_preview(float delta) {
    if (!preview_texture) init_character_preview();
    // `init_character_preview` makes the node with the texture; checked here
    // once, so nothing below has to.
    if (!preview_texture || !subject || !scene || preview_node == nullptr) return;

    // The pet's page (`petmenu_xenon.layout`) has the same `CharacterViewport`
    // as the character's, and `CCharacterMenuXenon` renders whichever owns the
    // panel (hands the viewport to the panel being shown): the pet
    // while its tray has the focus. The pet wears and holds nothing here.
    const bool pet_shown = inventory_menu.pet_tray_focused() && pet_entity != nullptr;
    Ogre::Entity* const source = pet_shown ? pet_entity : subject;
    static const std::vector<Ogre::Entity*> kNothingWorn;
    const std::vector<Ogre::Entity*>& worn = pet_shown ? kNothingWorn : equipment;
    Ogre::Entity* const held = pet_shown ? nullptr : weapon_entity;

    const bool mesh_differs =
        (preview_subject == nullptr) || (preview_subject->getMesh() != source->getMesh());
    if (mesh_differs) {
        if (preview_weapon != nullptr) {
            if (preview_subject != nullptr) preview_subject->detachObjectFromBone(preview_weapon);
            scene->destroyEntity(preview_weapon);
            preview_weapon = nullptr;
        }
        for (Ogre::Entity* eq : preview_equipment) {
            if (eq != nullptr) {
                preview_node->detachObject(eq);
                scene->destroyEntity(eq);
            }
        }
        preview_equipment.clear();
        if (preview_subject != nullptr) {
            preview_node->detachObject(preview_subject);
            scene->destroyEntity(preview_subject);
            preview_subject = nullptr;
        }

        try {
            preview_subject = scene->createEntity(source->getMesh());
            rebind_materials(preview_subject);
            preview_node->attachObject(preview_subject);
        } catch (const Ogre::Exception& e) {
            TL_LOG_WARN("failed to create preview subject: %s", e.getDescription().c_str());
            return;
        }
    }

    if (preview_subject != nullptr) {
        const size_t sub_count =
            std::min(preview_subject->getNumSubEntities(), source->getNumSubEntities());
        for (size_t i = 0; i < sub_count; ++i) {
            preview_subject->getSubEntity(i)->setMaterial(source->getSubEntity(i)->getMaterial());
            preview_subject->getSubEntity(i)->setVisible(source->getSubEntity(i)->isVisible());
        }
    }

    bool pieces_differ = worn.size() != preview_equipment.size();
    for (size_t i = 0; !pieces_differ && i < worn.size(); ++i) {
        pieces_differ = worn[i] != nullptr && preview_equipment[i] != nullptr &&
                        worn[i]->getMesh() != preview_equipment[i]->getMesh();
    }
    if (pieces_differ) {
        for (Ogre::Entity* eq : preview_equipment) {
            if (eq != nullptr) {
                preview_node->detachObject(eq);
                scene->destroyEntity(eq);
            }
        }
        preview_equipment.clear();
        for (Ogre::Entity* eq : worn) {
            if (eq != nullptr) {
                try {
                    Ogre::Entity* peq = scene->createEntity(eq->getMesh());
                    rebind_materials(peq);
                    preview_node->attachObject(peq);
                    preview_equipment.push_back(peq);
                } catch (const Ogre::Exception& e) {
                    TL_LOG_WARN("failed to create preview equipment: %s",
                                e.getDescription().c_str());
                } catch (...) {
                    TL_LOG_WARN("failed to create preview equipment");
                }
            }
        }
    }
    for (size_t i = 0; i < std::min(worn.size(), preview_equipment.size()); ++i) {
        Ogre::Entity* eq = worn[i];
        Ogre::Entity* peq = preview_equipment[i];
        if (eq && peq) {
            const size_t subs = std::min(peq->getNumSubEntities(), eq->getNumSubEntities());
            for (size_t j = 0; j < subs; ++j) {
                peq->getSubEntity(j)->setMaterial(eq->getSubEntity(j)->getMaterial());
                peq->getSubEntity(j)->setVisible(eq->getSubEntity(j)->isVisible());
            }
        }
    }

    if (held != nullptr && preview_subject != nullptr) {
        if (preview_weapon == nullptr || preview_weapon->getMesh() != held->getMesh()) {
            if (preview_weapon != nullptr) {
                preview_subject->detachObjectFromBone(preview_weapon);
                scene->destroyEntity(preview_weapon);
                preview_weapon = nullptr;
            }
            try {
                preview_weapon = scene->createEntity(held->getMesh());
                rebind_materials(preview_weapon);
                const char* bone = (preview_subject->hasSkeleton() &&
                                    preview_subject->getSkeleton()->hasBone("tag_righthand"))
                                       ? "tag_righthand"
                                   : (preview_subject->hasSkeleton() &&
                                      preview_subject->getSkeleton()->hasBone("tag_lefthand"))
                                       ? "tag_lefthand"
                                       : nullptr;
                if (bone != nullptr) {
                    Ogre::TagPoint* tag = preview_subject->attachObjectToBone(bone, preview_weapon);
                    const float scale =
                        player_stats.weapon_scale > 0.0f ? player_stats.weapon_scale : 1.0f;
                    tag->setScale(scale, scale, scale);
                }
            } catch (const Ogre::Exception& e) {
                TL_LOG_WARN("failed to equip preview weapon: %s", e.getDescription().c_str());
            }
        }
        if (preview_weapon != nullptr) {
            const size_t subs =
                std::min(preview_weapon->getNumSubEntities(), held->getNumSubEntities());
            for (size_t j = 0; j < subs; ++j) {
                preview_weapon->getSubEntity(j)->setMaterial(held->getSubEntity(j)->getMaterial());
            }
        }
    } else if (held == nullptr && preview_weapon != nullptr) {
        preview_subject->detachObjectFromBone(preview_weapon);
        scene->destroyEntity(preview_weapon);
        preview_weapon = nullptr;
    }

    if (preview_subject != nullptr && preview_camera_node != nullptr) {
        const Ogre::AxisAlignedBox bounds = preview_subject->getBoundingBox();
        const Ogre::Vector3 centre = bounds.getCenter();
        const float radius = bounds.getHalfSize().length();
        const float cam_dist = std::max(2.5f, radius * 2.2f);
        preview_camera_node->setPosition(0.0f, 10000.0f + centre.y + 0.05f, cam_dist);
        preview_camera_node->lookAt(Ogre::Vector3(0.0f, 10000.0f + centre.y - 0.05f, 0.0f),
                                    Ogre::Node::TS_WORLD);
    }

    preview_idle_time += delta;
    if (preview_subject != nullptr) {
        const std::string anim = animation_named(preview_subject, "Idle");
        if (!anim.empty()) {
            Ogre::AnimationState* state = preview_subject->getAnimationState(anim);
            state->setEnabled(true);
            state->setLoop(true);
            if (delta > 0.0f) state->addTime(delta);
            else state->setTimePosition(kMenuIdleTime);
        }
    }
    for (Ogre::Entity* peq : preview_equipment) {
        if (peq != nullptr) {
            const std::string anim = animation_named(peq, "Idle");
            if (!anim.empty()) {
                Ogre::AnimationState* state = peq->getAnimationState(anim);
                state->setEnabled(true);
                state->setLoop(true);
                if (delta > 0.0f) state->addTime(delta);
                else state->setTimePosition(kMenuIdleTime);
            }
        }
    }

    Ogre::RenderTexture* rt = preview_texture->getBuffer()->getRenderTarget();
    if (rt != nullptr) {
        rt->update();
    }
}

void Renderer::Impl::shutdown_character_preview() {
    if (scene != nullptr) {
        if (preview_weapon != nullptr) {
            if (preview_subject != nullptr) preview_subject->detachObjectFromBone(preview_weapon);
            scene->destroyEntity(preview_weapon);
            preview_weapon = nullptr;
        }
        for (Ogre::Entity* eq : preview_equipment) {
            if (eq != nullptr) {
                if (preview_node != nullptr) preview_node->detachObject(eq);
                scene->destroyEntity(eq);
            }
        }
        preview_equipment.clear();
        if (preview_subject != nullptr) {
            if (preview_node != nullptr) preview_node->detachObject(preview_subject);
            scene->destroyEntity(preview_subject);
            preview_subject = nullptr;
        }
        if (preview_light != nullptr) {
            if (preview_light_node != nullptr) preview_light_node->detachObject(preview_light);
            scene->destroyLight(preview_light);
            preview_light = nullptr;
        }
        if (preview_light_node != nullptr) {
            scene->destroySceneNode(preview_light_node);
            preview_light_node = nullptr;
        }
        if (preview_camera != nullptr) {
            if (preview_camera_node != nullptr) preview_camera_node->detachObject(preview_camera);
            scene->destroyCamera(preview_camera);
            preview_camera = nullptr;
        }
        if (preview_camera_node != nullptr) {
            scene->destroySceneNode(preview_camera_node);
            preview_camera_node = nullptr;
        }
        if (preview_node != nullptr) {
            scene->destroySceneNode(preview_node);
            preview_node = nullptr;
        }
    }
    if (preview_texture) {
        Ogre::TextureManager::getSingleton().remove(preview_texture);
        preview_texture.reset();
    }
}

}  // namespace tl::render
