// Options and the Pause Menu: CSettingsMenuXenon,
//
// COptionsMenu::onClick, CSoundManager::updateAudioLevels.
//
#include "core/settings_menu.h"
#include "core/display_gamma.h"
#include "core/run_config.h"
#include "test_support.h"

#include <cmath>
#include <vector>

namespace {

using tl::core::AudioLevels;
using tl::core::GameSettings;
using tl::core::SettingsControl;
using tl::core::SettingsMenu;

bool near(float a, float b) { return std::fabs(a - b) < 1.0e-4f; }

/// A screen that records every `updateAudioLevels` call.
struct Recorded {
    SettingsMenu menu;
    std::vector<AudioLevels> calls;
    Recorded() {
        menu.update_audio_levels = [this](const AudioLevels& levels) { calls.push_back(levels); };
    }
    void focus(SettingsControl control) { menu.focus = static_cast<int>(control) - 1; }
};

}  // namespace

/// Sound volume on the master sound and channel groups; music volume on the music group
/// under that channel group, whose mute is the sound's.
TL_TEST(audio_levels_become_gains) {
    tl::core::AudioGains gains = tl::core::audio_gains(AudioLevels{0.8f, 0.5f, false, false});
    TL_CHECK(near(gains.effects, 0.64f));
    TL_CHECK(near(gains.music, 0.4f));
    gains = tl::core::audio_gains(AudioLevels{0.8f, 0.5f, false, true});
    TL_CHECK(near(gains.effects, 0.64f));
    TL_CHECK(near(gains.music, 0.0f));
    gains = tl::core::audio_gains(AudioLevels{0.8f, 0.5f, true, false});
    TL_CHECK(near(gains.effects, 0.0f));
    TL_CHECK(near(gains.music, 0.0f));
}

/// Up, the stick up and the stick left are "previous"; down, the stick down
/// and right "next". The D-pad's left and right walk nothing on this screen.
TL_TEST(settings_navigation_codes) {
    TL_CHECK_EQ(tl::core::settings_navigation(tl::core::kMenuPadUp), 2);
    TL_CHECK_EQ(tl::core::settings_navigation(tl::core::kMenuPadStickUp), 2);
    TL_CHECK_EQ(tl::core::settings_navigation(tl::core::kMenuPadStickLeft), 2);
    TL_CHECK_EQ(tl::core::settings_navigation(tl::core::kMenuPadDown), 1);
    TL_CHECK_EQ(tl::core::settings_navigation(tl::core::kMenuPadStickDown), 1);
    TL_CHECK_EQ(tl::core::settings_navigation(tl::core::kMenuPadStickRight), 1);
    TL_CHECK_EQ(tl::core::settings_navigation(tl::core::kMenuPadLeft), 0);
    TL_CHECK_EQ(tl::core::settings_navigation(tl::core::kMenuPadRight), 0);
    TL_CHECK_EQ(tl::core::settings_navigation(tl::core::kMenuPadA), 0);

    SettingsMenu menu;
    TL_CHECK(menu.focused() == SettingsControl::MusicVolume);
    TL_CHECK(menu.navigate(tl::core::kMenuPadUp));
    TL_CHECK(menu.focused() == SettingsControl::Defaults);
    TL_CHECK(menu.navigate(tl::core::kMenuPadStickRight));
    TL_CHECK(menu.focused() == SettingsControl::MusicVolume);
    TL_CHECK(!menu.navigate(tl::core::kMenuPadLeft));

    // A port without a storage device hides the button; the walk passes it.
    menu.storage_hidden = true;
    menu.focus = static_cast<int>(SettingsControl::Contrast) - 1;
    menu.navigate(tl::core::kMenuPadDown);
    TL_CHECK(menu.focused() == SettingsControl::Defaults);
    menu.navigate(tl::core::kMenuPadUp);
    TL_CHECK(menu.focused() == SettingsControl::Contrast);
}

/// Opening reads the settings into the windows; the two volume sliders each
/// apply the levels as they are set.
TL_TEST(settings_open_fills_the_windows) {
    Recorded r;
    GameSettings s;
    s.sound_volume = 0.6f;
    s.music_volume = 0.25f;
    s.music_mute = true;
    s.show_blood = false;
    s.brightness = 350.0f;
    s.contrast = 250.0f;
    r.menu.set_open(true, s);
    TL_CHECK(r.menu.open);
    TL_CHECK(!r.menu.finished);
    TL_CHECK(near(r.menu.sound_tab, 0.6f));
    TL_CHECK(near(r.menu.music_tab, 0.25f));
    TL_CHECK(near(r.menu.brightness_tab, 0.5f));
    TL_CHECK(near(r.menu.contrast_tab, 0.5f));
    TL_CHECK(r.menu.music_mute);
    TL_CHECK(!r.menu.sound_mute);
    TL_CHECK(!r.menu.show_blood);
    TL_CHECK(r.menu.show_tips);
    TL_CHECK_EQ(r.calls.size(), static_cast<size_t>(2));
    TL_CHECK(near(r.calls[1].sound, 0.6f));
    TL_CHECK(near(r.calls[1].music, 0.25f));
    TL_CHECK(r.calls[1].music_mute);
}

/// A step is 0.05 of a volume, 10 of brightness (100..600) and 5 of contrast
/// (100..400); each clamps at its ends.
TL_TEST(settings_sliders_step_and_clamp) {
    Recorded r;
    GameSettings s;
    // The console's pair, which the steps below are counted from.
    s.brightness = tl::core::kConsoleBrightness;
    s.contrast = tl::core::kConsoleContrast;
    r.menu.set_open(true, s);

    r.focus(SettingsControl::MusicVolume);
    r.menu.press(tl::core::kMenuPadRight, s);
    TL_CHECK(near(s.music_volume, 0.55f));
    TL_CHECK(near(r.menu.music_tab, 0.55f));
    for (int i = 0; i < 20; ++i) r.menu.press(tl::core::kMenuPadRight, s);
    TL_CHECK(near(s.music_volume, 1.0f));

    r.focus(SettingsControl::SoundVolume);
    for (int i = 0; i < 20; ++i) r.menu.press(tl::core::kMenuPadLeft, s);
    TL_CHECK(near(s.sound_volume, 0.0f));
    TL_CHECK(near(r.calls.back().sound, 0.0f));
    TL_CHECK(near(r.calls.back().music, 1.0f));

    r.focus(SettingsControl::Brightness);
    r.menu.press(tl::core::kMenuPadRight, s);
    TL_CHECK(near(s.brightness, 300.0f));
    for (int i = 0; i < 40; ++i) r.menu.press(tl::core::kMenuPadRight, s);
    TL_CHECK(near(s.brightness, 600.0f));
    TL_CHECK(near(r.menu.brightness_tab, 1.0f));

    r.focus(SettingsControl::Contrast);
    r.menu.press(tl::core::kMenuPadLeft, s);
    TL_CHECK(near(s.contrast, 205.0f));
    for (int i = 0; i < 40; ++i) r.menu.press(tl::core::kMenuPadLeft, s);
    TL_CHECK(near(s.contrast, 100.0f));
    TL_CHECK(near(r.menu.contrast_tab, 0.0f));

    // The stick does not change a value, and A does nothing on a slider.
    const float before = s.contrast;
    r.menu.press(tl::core::kMenuPadStickRight, s);
    r.menu.press(tl::core::kMenuPadA, s);
    TL_CHECK(near(s.contrast, before));
}

/// A flips a checkbox; the settings take the three display boxes only when
/// the screen is closed through Done.
TL_TEST(settings_done_keeps_the_changes) {
    Recorded r;
    GameSettings s;
    r.menu.set_open(true, s);
    r.focus(SettingsControl::ShowBlood);
    TL_CHECK(r.menu.press(tl::core::kMenuPadA, s).changed);
    TL_CHECK(!r.menu.show_blood);
    TL_CHECK(s.show_blood);
    r.focus(SettingsControl::MusicMute);
    r.menu.press(tl::core::kMenuPadA, s);
    TL_CHECK(r.menu.music_mute);

    // The mute is heard on the next update, before anything is closed.
    r.calls.clear();
    r.menu.update(s);
    TL_CHECK_EQ(r.calls.size(), static_cast<size_t>(1));
    TL_CHECK(r.calls[0].music_mute);

    const tl::core::SettingsPress done = r.menu.press(tl::core::kMenuPadB, s);
    TL_CHECK(done.closed);
    TL_CHECK(!done.help);
    TL_CHECK(!r.menu.open);
    TL_CHECK(!r.menu.finished);
    const tl::core::SettingsUpdate update = r.menu.update(s);
    TL_CHECK(update.finished);
    TL_CHECK(update.save);
    TL_CHECK(r.menu.finished);
    TL_CHECK(!s.show_blood);
    TL_CHECK(s.music_mute);
    // Nothing more once it is done.
    r.calls.clear();
    TL_CHECK(!r.menu.update(s).finished);
    TL_CHECK(r.calls.empty());
}

/// Y is Help: it closes the screen as Done does, then asks for Help.
TL_TEST(settings_y_accepts_and_asks_for_help) {
    Recorded r;
    GameSettings s;
    r.menu.set_open(true, s);
    const tl::core::SettingsPress help = r.menu.press(tl::core::kMenuPadY, s);
    TL_CHECK(help.closed);
    TL_CHECK(help.help);
    TL_CHECK(r.menu.update(s).save);
}

/// A close that is not accepted puts brightness and contrast back and leaves
/// the display boxes alone - and keeps the volumes, which the sliders wrote.
TL_TEST(settings_cancel_restores_brightness_and_contrast_only) {
    Recorded r;
    GameSettings s;
    // The console's pair, which the steps below are counted from.
    s.brightness = tl::core::kConsoleBrightness;
    s.contrast = tl::core::kConsoleContrast;
    r.menu.set_open(true, s);
    r.focus(SettingsControl::Brightness);
    r.menu.press(tl::core::kMenuPadRight, s);
    r.focus(SettingsControl::Contrast);
    r.menu.press(tl::core::kMenuPadRight, s);
    r.focus(SettingsControl::SoundVolume);
    r.menu.press(tl::core::kMenuPadLeft, s);
    r.focus(SettingsControl::ShowTips);
    r.menu.press(tl::core::kMenuPadA, s);
    TL_CHECK(near(s.brightness, 300.0f));

    r.menu.on_click(tl::core::kLayoutFunctionCancel, s);
    TL_CHECK(!r.menu.open);
    const tl::core::SettingsUpdate update = r.menu.update(s);
    TL_CHECK(update.finished);
    TL_CHECK(!update.save);
    TL_CHECK(near(s.brightness, 290.0f));
    TL_CHECK(near(s.contrast, 210.0f));
    TL_CHECK(near(s.sound_volume, 0.75f));
    TL_CHECK(s.show_tips);
}

/// Reset Defaults: the identity pair (DIVERGENCE; the console's 290, 210), 0.8,
/// 0.5, both mutes off, the three boxes on.
TL_TEST(settings_reset_defaults) {
    Recorded r;
    GameSettings s;
    s.sound_volume = 0.1f;
    s.music_volume = 0.9f;
    s.sound_mute = true;
    s.brightness = 500.0f;
    s.contrast = 120.0f;
    s.show_tips = false;
    r.menu.set_open(true, s);
    r.focus(SettingsControl::Defaults);
    // Only A presses it.
    r.menu.press(tl::core::kMenuPadRight, s);
    TL_CHECK(near(s.brightness, 500.0f));
    TL_CHECK(r.menu.press(tl::core::kMenuPadA, s).changed);
    TL_CHECK(near(s.brightness, 257.0f));
    TL_CHECK(near(s.contrast, 256.0f));
    // Through the sliders' arithmetic the ints are still the identity's, so
    // the frame is left alone.
    TL_CHECK(tl::core::gamma_pair(s).brightness == 257);
    TL_CHECK(tl::core::gamma_pair(s).contrast == 256);
    TL_CHECK(tl::core::gamma_blend_steps(tl::core::gamma_line(257, 256)).empty());
    TL_CHECK(near(s.sound_volume, 0.8f));
    TL_CHECK(near(s.music_volume, 0.5f));
    TL_CHECK(!r.menu.sound_mute);
    TL_CHECK(!r.menu.music_mute);
    TL_CHECK(r.menu.show_tips);
    TL_CHECK(near(r.calls.back().sound, 0.8f));
    TL_CHECK(near(r.calls.back().music, 0.5f));
    TL_CHECK(!r.calls.back().sound_mute);

    r.focus(SettingsControl::Storage);
    TL_CHECK(r.menu.press(tl::core::kMenuPadA, s).storage);
}

/// The settings file round-trips, and a file missing a key leaves it alone.
TL_TEST(settings_file_round_trip) {
    GameSettings s;
    s.sound_volume = 0.35f;
    s.music_mute = true;
    s.show_blood = false;
    s.brightness = 410.0f;
    GameSettings back;
    tl::core::read_game_settings(tl::core::write_game_settings(s), back);
    TL_CHECK(near(back.sound_volume, 0.35f));
    TL_CHECK(back.music_mute);
    TL_CHECK(!back.show_blood);
    TL_CHECK(near(back.brightness, 410.0f));
    TL_CHECK(near(back.contrast, tl::core::kDefaultContrast));

    GameSettings partial;
    tl::core::read_game_settings("MUSICVOLUME:0.2\r\nnonsense\nUNKNOWN:4\n", partial);
    TL_CHECK(near(partial.music_volume, 0.2f));
    TL_CHECK(near(partial.sound_volume, 0.8f));
}

/// The Pause Menu walks the rows this port offers, and a click on each does
/// what `COptionsMenu::onClick` does.
TL_TEST(pause_menu_rows_and_clicks) {
    TL_CHECK_EQ(tl::core::options_row(0).function, tl::core::kLayoutFunctionSettingsMenu);
    TL_CHECK_EQ(tl::core::options_row(4).function, tl::core::kLayoutFunctionExitGame);
    for (int i = 0; i < tl::core::kOptionsRowCount; ++i) {
        TL_CHECK_EQ(tl::core::options_row(i).tab_order, i + 1);
    }

    // The debug tools on (config.txt's `debug=1`): the Achievements slot is offered.
    (void)tl::core::load_run_config("", tl::core::RunConfig{true, true, false}, false);
    tl::core::OptionsMenu menu;
    // Closed, a click is nothing.
    TL_CHECK(!menu.on_click(tl::core::kLayoutFunctionExitGame).handled);

    menu.open = true;
    TL_CHECK_EQ(menu.focus, 0);
    TL_CHECK(menu.navigate(true));
    TL_CHECK_EQ(menu.focus, 1);  // Help
    TL_CHECK(menu.navigate(true));
    TL_CHECK_EQ(menu.focus, 2);  // the port's debug menu, on the Achievements slot
    TL_CHECK(menu.navigate(true));
    TL_CHECK_EQ(menu.focus, 4);
    TL_CHECK(menu.navigate(true));
    TL_CHECK_EQ(menu.focus, 0);
    TL_CHECK(menu.navigate(false));
    TL_CHECK_EQ(menu.focus, 4);

    tl::core::OptionsClick click = menu.on_click(tl::core::kLayoutFunctionSettingsMenu);
    TL_CHECK(click.handled && click.closed && click.open_settings);
    TL_CHECK(!menu.open);

    menu.open = true;
    click = menu.on_click(tl::core::kLayoutFunctionExitGame);
    TL_CHECK(click.exit_to_title && click.closed);

    menu.open = true;
    click = menu.on_click(tl::core::kLayoutFunctionClose);
    TL_CHECK(click.closed && !click.exit_to_title && !click.open_settings);

    menu.open = true;
    click = menu.on_click(tl::core::kPortFunctionDebugMenu);
    TL_CHECK(click.open_debug && click.closed);

    // `GUIHELP` (the Pause Menu's row) and `GUIHELPANDOPTIONSMENU` (the main
    // menu's) both reach the Help toggle.
    TL_CHECK_EQ(tl::core::options_row(1).function, tl::core::kLayoutFunctionHelp);
    TL_CHECK_EQ(tl::core::kLayoutFunctionHelp, 0x64);
    TL_CHECK_EQ(tl::core::kLayoutFunctionHelpAndOptions, 0x62);
    for (int function : {tl::core::kLayoutFunctionHelp, tl::core::kLayoutFunctionHelpAndOptions}) {
        menu.open = true;
        click = menu.on_click(function);
        TL_CHECK(click.open_help && click.closed && !menu.open);
    }

    menu.open = true;
    click = menu.on_click(0x30);
    TL_CHECK(!click.handled);
    TL_CHECK(menu.open);
    (void)tl::core::load_run_config("", tl::core::RunConfig{}, false);
}

/// With the debug tools off (config.txt's `debug=0`) the debug menu's row is
/// neither drawn nor reached: Help goes straight to Exit to Title.
TL_TEST(pause_menu_skips_the_debug_row_without_the_tools) {
    (void)tl::core::load_run_config("", tl::core::RunConfig{}, false);
    tl::core::OptionsMenu menu;
    menu.open = true;
    TL_CHECK(!tl::core::options_row_offered(2));
    menu.focus = 1;
    TL_CHECK(menu.navigate(true));
    TL_CHECK_EQ(menu.focus, 4);
    TL_CHECK(menu.navigate(false));
    TL_CHECK_EQ(menu.focus, 1);
}
