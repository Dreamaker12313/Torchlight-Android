// Help, pinned against `CHowToMenu` (X360 `setOpen`, the pad
// handler, `vfunc_9`).
#include "core/howto_menu.h"
#include "core/settings_menu.h"
#include "test_support.h"

#include <cmath>

namespace {

bool near(float a, float b) { return std::fabs(a - b) < 1.0e-5f; }

}  // namespace

/// Closed, the handler takes nothing.
TL_TEST(howto_closed_takes_nothing) {
    tl::core::HowToMenu menu;
    const tl::core::HowToPress press = menu.press(tl::core::kMenuPadB);
    TL_CHECK(!press.handled);
    TL_CHECK(!press.toggle);
}

/// B, Start and Back close it; every other code is taken and does nothing.
TL_TEST(howto_b_start_and_back_close) {
    tl::core::HowToMenu menu;
    menu.set_open(true);
    for (int code : {tl::core::kMenuPadB, tl::core::kMenuPadStart, tl::core::kMenuPadBack}) {
        const tl::core::HowToPress press = menu.press(code);
        TL_CHECK(press.handled);
        TL_CHECK(press.toggle);
    }
    for (int code : {tl::core::kMenuPadA, tl::core::kMenuPadX, tl::core::kMenuPadY,
                     tl::core::kMenuPadUp, tl::core::kMenuPadDown, tl::core::kMenuPadStickLeft}) {
        const tl::core::HowToPress press = menu.press(code);
        TL_CHECK(press.handled);
        TL_CHECK(!press.toggle);
    }
    TL_CHECK(near(menu.scroll_speed, 0.0f));
}

/// The stick builds the speed 2 at a time to 10; turning back starts from 0.
TL_TEST(howto_stick_pushes_the_speed) {
    tl::core::HowToMenu menu;
    menu.set_open(true);
    for (int i = 0; i < 4; ++i) menu.press(tl::core::kMenuPadStickUp);
    TL_CHECK(near(menu.scroll_speed, 8.0f));
    menu.press(tl::core::kMenuPadStickUp);
    menu.press(tl::core::kMenuPadStickUp);
    TL_CHECK(near(menu.scroll_speed, 10.0f));
    menu.press(tl::core::kMenuPadStickDown);
    TL_CHECK(near(menu.scroll_speed, -2.0f));
    for (int i = 0; i < 9; ++i) menu.press(tl::core::kMenuPadStickDown);
    TL_CHECK(near(menu.scroll_speed, -10.0f));
}

/// The speed falls away only while the stick is off centre, and an opening
/// puts it back to 0.
TL_TEST(howto_speed_decays_and_resets) {
    tl::core::HowToMenu menu;
    menu.set_open(true);
    menu.press(tl::core::kMenuPadStickUp);
    menu.press(tl::core::kMenuPadStickUp);
    menu.update(0.25f, 0.0f);
    TL_CHECK(near(menu.scroll_speed, 4.0f));
    menu.update(0.25f, 0.5f);
    TL_CHECK(near(menu.scroll_speed, 3.0f));

    // An open that changes nothing leaves the speed alone.
    menu.set_open(true);
    TL_CHECK(near(menu.scroll_speed, 3.0f));
    menu.set_open(false);
    TL_CHECK(!menu.open);
    menu.update(0.25f, 0.5f);
    TL_CHECK(near(menu.scroll_speed, 3.0f));
    menu.set_open(true);
    TL_CHECK(near(menu.scroll_speed, 0.0f));
}
