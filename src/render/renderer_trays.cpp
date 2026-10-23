// The bumpers on the inventory's and the merchant's trays. Split from renderer_player.cpp.
#include "render/renderer_impl.h"

namespace tl::render {

void Renderer::Impl::tray_bumpers(bool vendor, bool rb, bool lb) {
#if defined(TL_PLATFORM_IOS)
    // DIVERGENCE (platform): the touch pad's RB steps the merchant's stock tab (the bag's), its
    // LB moves the focus over the trays (swaps the bags).
    if (rb) {
        if (vendor) {
            inventory_menu.cycle_vendor_tab(1);
        } else {
            inventory_menu.step_bag_tab(1);
        }
        play_ui(tl::core::InterfaceSound::Navigate);
        update_menu();
    }
    if (lb) {
        if (vendor) {
            inventory_menu.focus_next_tray(1);
        } else {
            inventory_menu.swap_bag_focus(0);
        }
        play_ui(tl::core::InterfaceSound::Navigate);
        update_menu();
    }
#else
    if (!rb && !lb) return;
    if (vendor) {
        // CMerchantMenuXenon's handler: RB/LB move the focus
        // between trays; stick left/right step the tray's tab,
        // past either end the focus.
        inventory_menu.focus_next_tray(rb ? 1 : -1);
        update_menu();
    } else {
        // CInventoryMenuXenon's handler: RB/LB swap bag and pet
        // tray; stick left/right step the tab, past either end the swap.
        inventory_menu.swap_bag_focus(0);
        play_ui(tl::core::InterfaceSound::Navigate);
        update_menu();
    }
#endif
}

}  // namespace tl::render
