// The pad script that stands in for a pad on a headless run.
#include "core/input.h"
#include "test_support.h"

#include <string>

/// Each press is one clean edge: held for a few frames, then released.
TL_TEST(pad_script_presses_one_edge_each) {
    tl::core::set_pad_script("down,a,text=Lara");
    TL_CHECK(tl::core::pad_script_running());
    int downs = 0, confirms = 0;
    bool was_down = false, was_confirm = false;
    for (int frame = 0; frame < 100 && tl::core::pad_script_running(); ++frame) {
        const tl::core::InputState in = tl::core::poll_input();
        const bool is_down = in.move_y < -0.5f;
        if (is_down && !was_down) ++downs;
        if (in.confirm && !was_confirm) ++confirms;
        was_down = is_down;
        was_confirm = in.confirm;
    }
    TL_CHECK(!tl::core::pad_script_running());
    TL_CHECK_EQ(downs, 1);
    TL_CHECK_EQ(confirms, 1);
    std::string text;
    TL_CHECK(tl::core::take_scripted_text(text));
    TL_CHECK(text == "Lara");
    TL_CHECK(!tl::core::take_scripted_text(text));
}
