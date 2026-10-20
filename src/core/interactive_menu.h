// The cinematic bars: the console's `CInteractiveMenu`, two strips of
// `media/ui/interactivemenu.layout` that slide in from the top and the bottom of the
// screen while a scene has the interface. Pure: the HUD draws them where this puts them.
#pragma once

namespace tl::core {

/// `CInteractiveMenu`.
struct InteractiveBars {
    /// The layout's bar height (`TopBar` `{0,0},{0,0},{1,0},{0.1,0}`). `setVisible`'s
    /// closing timer is the top bar's height times two.
    static constexpr float kBarHeight = 0.1f;

    /// `+0x1c`: asked to be shown. The constructor sets it, so `createMenus`'
    /// `setVisible(false)` closes a menu that never opened.
    bool open = true;
    /// `+0x20`: what is left of the slide, in seconds; below 0 once it has run
    /// out, and then only an open menu is updated.
    float timer = 0.0f;
    /// The root window's `setVisible` (`+0xc`), which `createMenus` clears.
    bool window_visible = false;
    /// The two bars' `UnifiedYPosition` scales (`+0x10`, `+0x14`), as the
    /// layout places them.
    float top_y = 0.0f;
    float bottom_y = 0.9f;
    /// The top bar's area bottom (`+0x110`, `URect` max y scale): each
    /// `setYPosition` keeps the size `max - min` and puts `max` at `y + size`,
    /// so the height the closing timer reads is the float that chain leaves.
    float top_bottom = kBarHeight;

    /// The constructor and its `createMenus`: hidden, then `setVisible(false)`.
    InteractiveBars();

    /// `CInteractiveMenu::setVisible`.
    /// Nothing when already in that state. Shown: the window up, the bars put
    /// off the screen (the top one at -0.1, the bottom at 1.1) and 0.2 s to
    /// slide in. Hidden: with the editor running (on the console, its flag
    /// bit 1 clear) the window goes at once; in the game the slide
    /// out takes the top bar's height times two (0.2 s) - from wherever the bars
    /// are, so a close during the slide in starts the slide out over.
    void set_visible(bool on, bool editor_running = false);

    /// `CInteractiveMenu::update`: each frame,
    /// `t = max(5 * timer, 0)` after the timer is run down by `delta`. Opening,
    /// the top bar at `-0.1 t` and the bottom at `1 - 0.1 (1 - t)`; closing, at
    /// `-0.1 (1 - t)` and `0.9 + 0.1 (1 - t)`, the window hidden once `t` is 0.
    /// Nothing while closed and run out.
    void update(float delta);

    /// The top edge of the bottom bar while it is drawn, else the screen's
    /// bottom: where the port's dialogue box may go down to.
    // PORT: presentation, where the port's dialogue box may go down to.
    float bottom_edge() const { return window_visible ? bottom_y : 1.0f; }
};

}  // namespace tl::core
