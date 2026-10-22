// Context tips: the window, the queue's turn each frame and the pad. What is queued when is
// core/context_tips (CGameUI::queueTip, CTipMenu); the window is tipmenu_xenon.layout.
#include "render/renderer_impl.h"

#include "core/context_tips.h"
#include "core/settings_menu.h"
#include "core/text.h"

#include <fstream>

namespace tl::render {

void Renderer::Impl::create_tip_screen() {
    // Over the modal message and under the death screen.
    tip_screen.create(kGameGroup, config.width, config.height, "media/UI/tipmenu_xenon.layout",
                      "tl/tip", 596);
    tip_queue.clear();
    tip_open = false;
}

// CGameUI::modalDialogOpenPartial; its list is filled in
// CGameUI::create.
bool Renderer::Impl::modal_dialog_open() const {
    // The debug menu is the port's own, drawn on the modal message's window.
    return start_menu_up() || death_screen_up || waypoint_screen.visible() ||
           script_dialog_open() || quest_dialog.visible() || cinematic_open ||
           modal_screen.visible() || tip_open;
}

// PORT: a diagnostic, the name of the modal screen that is up.
const char* Renderer::Impl::modal_dialog_name() const {
    if (tip_open) return "a context tip";
    if (debug_menu_open) return "the debug menu";
    if (start_menu_up()) return "the Pause Menu or Options";
    if (death_screen_up) return "the death screen";
    if (waypoint_screen.visible()) return "the waypoint menu";
    if (cinematic_open) return "a cinematic";
    if (modal_screen.visible()) return "the modal message";
    if (quest_dialog.visible()) return "a quest window";
    if (script_dialog_open()) return "a dialog";
    if (full_screen_menu_up()) return "the master menu";
    return "nothing";
}

// CGameUI::queueTip
void Renderer::Impl::queue_tip(tl::core::ContextTip tip) {
    if (!config.context_tips) return;
    // DIVERGENCE (data): with no tip window to show it in, nothing is queued, so the tip is
    // left unseen rather than spent unshown.
    if (!tip_screen.ready()) return;
    const bool has_player = player_control && subject_node != nullptr;
    const size_t before = tip_queue.size();
    tip_queue.queue(tip, has_player, player_character.tips_seen(), game_settings().show_tips);
    if (tip_queue.size() != before && before == 0) {
        TL_LOG_INFO("tips: %s queued", tl::core::context_tip_name(static_cast<int>(tip)));
    }
}

// TIP_INVENTORY from the setOpen of CInventoryMenuXenon, CMerchantMenuXenon,
// CStashMenuXenon (not the PC's
// CMerchantMenu::setOpen and kin). TIP_STATS while stat points are unspent,
// TIP_SKILLS likewise.
void Renderer::Impl::queue_menu_tips() {
    const bool up = inventory_menu.visible();
    const MenuMode mode = inventory_menu.menu_mode();
    const VendorScreen vendor = inventory_menu.vendor();
    const bool bag_open = up && mode == MenuMode::Inventory &&
                          (vendor == VendorScreen::None || vendor == VendorScreen::Merchant ||
                           vendor == VendorScreen::Stash || vendor == VendorScreen::SharedStash);
    if (bag_open && !tip_bag_was_open) queue_tip(tl::core::ContextTip::Inventory);
    tip_bag_was_open = bag_open;
    if (up && mode == MenuMode::Stats && player_character.unspent_stat_points() > 0) {
        queue_tip(tl::core::ContextTip::Stats);
    }
    if (up && mode == MenuMode::Skills && player_character.unspent_skill_points() > 0) {
        queue_tip(tl::core::ContextTip::Skills);
    }
}

// CGameUI::updateIngameUI, the tip queue at
//
void Renderer::Impl::step_tips() {
    if (tip_queue.size() == 0) return;
    load_context_tips();
    const bool has_player = player_control && subject_node != nullptr;
    const tl::core::TipQueue::Shown shown =
        tip_queue.update(modal_dialog_open(), has_player, player_character.tips_seen(), tip_texts);
    if (!shown.open) return;
    const char* name = tl::core::context_tip_name(static_cast<int>(shown.tip));
    // `CTipMenu::setContents` and `setOpen(true)`.
    tip_screen.set_caption("Dialog", shown.text);
    tip_screen.refresh();
    tip_screen.show();
    tip_open = true;
    TL_LOG_INFO("tips: %s shown: %s", name, shown.text.c_str());
}

// The console tip window's pad handler
void Renderer::Impl::press_tip_menu(int button) {
    if (!tip_open) return;
    tl::core::GameSettings& settings = game_settings();
    const tl::core::TipPress press = tl::core::tip_menu_press(button, settings.show_tips);
    if (press.disable_tips) {
        // the key set to 0 and the settings saved.
        if (!config.settings_path.empty()) {
            std::ofstream file(config.settings_path, std::ios::binary | std::ios::trunc);
            file << tl::core::write_game_settings(settings);
            if (!file) TL_LOG_WARN("tips: could not write '%s'", config.settings_path.c_str());
        }
        TL_LOG_INFO("tips: disabled from the tip window");
    }
    if (press.closed) {
        tip_screen.hide();
        tip_open = false;
        TL_LOG_INFO("tips: closed");
    }
}

bool Renderer::press_tip(const std::string& button) {
    Impl& m = *impl_;
    const int code = button == "b" || button == "B"   ? tl::core::kMenuPadB
                     : button == "y" || button == "Y" ? tl::core::kMenuPadY
                                                      : 0;
    if (code == 0 || !m.tip_open) {
        TL_LOG_WARN("tips: cannot press '%s' (b or y, on an open tip)", button.c_str());
        return false;
    }
    m.press_tip_menu(code);
    return true;
}

}  // namespace tl::render
