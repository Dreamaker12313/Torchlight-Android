// The inventory menu's trays: which one has the focus, and their tabs. Split from
// inventory_menu.cpp.
#include "render/inventory_menu.h"

#include "core/inventory_screen.h"

namespace tl::render {

void InventoryMenu::focus_next_tray(int direction) {
    if (vendor_ == VendorScreen::None || direction == 0) return;
#if defined(TL_PLATFORM_IOS)
    // DIVERGENCE (platform): the touch pad's LB rotates over the vendor's tray, the
    // bag and the pet's, and on the enchanter swaps the bag and the pet's.
    if (enchanter_screen()) {
        if (!pet_tray_available_) return;
        focus_ = focus_ == Tray::Pet ? Tray::Bag : Tray::Pet;
        return;
    }
    if (direction < 0) {
        if (focus_ == Tray::Vendor) focus_ = Tray::Bag;
        else if (focus_ == Tray::Bag && pet_tray_available_) focus_ = Tray::Pet;
        else focus_ = Tray::Vendor;
    } else {
        if (focus_ == Tray::Vendor && pet_tray_available_) focus_ = Tray::Pet;
        else if (focus_ == Tray::Pet || focus_ == Tray::Vendor) focus_ = Tray::Bag;
        else focus_ = Tray::Vendor;
    }
#else
    // `CEnchantMenuXenon`'s RB/LB (codes 10 and 12) swap the
    // bag and the pet's tray; the pet's takes no focus here (below), so the
    // bag keeps it.
    if (enchanter_screen()) return;
    // codes 10/12 -> (direction, false): the focus
    // rotates over the three trays and wraps. INVENTED: the pet's tray takes
    // no focus here yet (it has no selection of its own), so the rotation is
    // over the vendor's and the bag.
    focus_ = focus_ == Tray::Vendor ? Tray::Bag : Tray::Vendor;
#endif
}

void InventoryMenu::swap_bag_focus(int landing) {
    if (vendor_ != VendorScreen::None || menu_mode_ != MenuMode::Inventory) return;
    if (!pet_tray_available_) return;
    // the other tray takes the focus (on both
    // trays, on both panels); with a direction, the tray it lands on
    // gets its tab first - 5 for -1, 0 for +1 - and with none keeps its own.
    focus_ = focus_ == Tray::Pet ? Tray::Bag : Tray::Pet;
    if (landing != 0) {
        int& tab = focus_ == Tray::Pet ? pet_tab_ : current_tab_;
        tab = landing < 0 ? tl::core::kInventoryFilterCount - 1 : 0;
    }
}

void InventoryMenu::step_bag_tab(int direction) {
    if (vendor_ != VendorScreen::None || menu_mode_ != MenuMode::Inventory || direction == 0) {
        return;
    }
    // Handler: code 0x13 is the tab - 1 and below 0 the swap with
    // -1; code 0x14 (tray, 1) and a step off the end the swap with
    // +1. Each step refreshes the tray, which starts it at its
    // first row.
    const bool on_pet = focus_ == Tray::Pet;
    int& tab = on_pet ? pet_tab_ : current_tab_;
    const int leave = tl::core::step_tray_tab(tab, direction);
    if (on_pet) {
        pet_selected_ = 0;
    } else {
        selected_index_ = 0;
    }
    if (leave == 0) return;
    swap_bag_focus(leave);
    if (focus_ == Tray::Pet) {
        pet_selected_ = 0;
    } else {
        selected_index_ = 0;
    }
}

// PORT: the pet tray's slot under the cursor.
int InventoryMenu::selected_pet_slot() const {
    if (pet_rows_.empty() || pet_selected_ < 0 ||
        pet_selected_ >= static_cast<int>(pet_rows_.size())) {
        return -1;
    }
    return static_cast<int>(pet_rows_[static_cast<size_t>(pet_selected_)]);
}

void InventoryMenu::step_tray_tab(int direction) {
    if (vendor_ == VendorScreen::None || direction == 0) return;
    const bool on_vendor = focus_ == Tray::Vendor;
    int& tab = on_vendor ? vendor_tab_ : current_tab_;
    const int leave =
        tl::core::step_tray_tab(tab, direction, on_vendor && vendor_ == VendorScreen::Combine);
    if (on_vendor) {
        vendor_selected_ = 0;
    } else {
        selected_index_ = 0;
    }
    if (leave == 0) return;
    // (leave, true)`: the tray it lands on starts at the edge it
    // came in from - tab 0 moving right, tab 5 moving left.
    focus_next_tray(leave);
    int& landed = focus_ == Tray::Vendor ? vendor_tab_ : current_tab_;
    landed = leave > 0 ? 0 : tl::core::kInventoryFilterCount - 1;
    if (focus_ == Tray::Vendor) {
        vendor_selected_ = 0;
    } else {
        selected_index_ = 0;
    }
}

void InventoryMenu::cycle_vendor_tab(int direction) {
    if (vendor_ == VendorScreen::None || direction == 0) return;
    vendor_tab_ = (vendor_tab_ + (direction > 0 ? 1 : -1) + tl::core::kInventoryFilterCount) %
                  tl::core::kInventoryFilterCount;
    vendor_selected_ = 0;
}

// PORT: the vendor tray's slot under the cursor.
int InventoryMenu::selected_vendor_slot() const {
    if (vendor_rows_.empty() || vendor_selected_ < 0 ||
        vendor_selected_ >= static_cast<int>(vendor_rows_.size())) {
        return -1;
    }
    return static_cast<int>(vendor_rows_[static_cast<size_t>(vendor_selected_)]);
}

}  // namespace tl::render
