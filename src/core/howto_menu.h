// Help: the console's controls screen, `CHowToMenu` (console only). Its builder
// (X360 `vfunc_13`) loads `media/ui/howto_xenon.uilayout`. `CGameUI`
// keeps it at `+0x18b8` with the full-screen menus (`+0x1798`), not the modal
// ones (`+0x17a8`).
//
// Three ways in, each `CGameUI`'s Help toggle: the main menu's
// `Help & Controls` (`GUIHELPANDOPTIONSMENU`, function 0x62, `CMainMenu::onClick`),
// the Pause Menu's Help (`GUIHELP`, 0x64) and Y on Options. The
// drawing is `render/renderer_options.cpp`'s.
#pragma once

namespace tl::core {

/// What one button did on Help.
struct HowToPress {
    bool handled = false;  ///< taken (the handler sets for every code)
    bool toggle = false;   ///< `CGameUI`'s Help toggle: here, the close
};

struct HowToMenu {
    bool open = false;  ///< `+0xcc`, which `vfunc_3` answers
    /// `+0xd8`: the stick's up and down push it 2 at a time, to 10 either way,
    /// and `update` lets it fall away. Nothing reads it, so the screen never
    /// scrolls; kept because the original keeps it.
    float scroll_speed = 0.0f;

    /// `setOpen` (X360 `vfunc_7`): on opening, the speed back to 0.
    /// The close's own consequence (the Pause Menu back in a game) is the
    /// caller's.
    void set_open(bool now_open);

    /// The pad handler, dropped while closed. B (6), Start
    /// (0xd) and Back (0xe) toggle Help - that is, close it; the left stick's
    /// up (0x11) and down (0x12) push the speed.
    HowToPress press(int code);

    /// `vfunc_9`: while open and the left stick is off
    /// centre vertically, the speed is
    /// multiplied by `1 - dt`.
    void update(float dt, float stick_y);
};

}  // namespace tl::core
