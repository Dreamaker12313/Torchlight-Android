// The Android build's entry point.
//
// A NativeActivity loads libtorchlight.so and the NDK's glue calls
// android_main() on a thread of its own. This waits for the activity's first
// window, reads the run's arguments from `args.txt` in the data root (an
// `adb push` sets a run's switches, where the desktop has a command line), and
// hands over to the engine's main(), which on this target is tl_main().
//
// When the engine returns, the activity is finished and the process ends with
// it: OGRE's singletons and the engine's statics are made for one run per
// process, and Android would otherwise start the next launch in the same one.
#include "platform/android/android_glue.h"

#include "core/platform.h"
#include "core/touch_pad.h"

#include <android/log.h>
#include <android_native_app_glue.h>
#include <unistd.h>

#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

// main.cpp's main() on this target.
int tl_main(int argc, char** argv);

namespace tl::core::android {

namespace {

android_app* g_app = nullptr;
bool g_started = false;
bool g_window_lost = false;

void on_command(android_app* glue, int32_t command) {
    switch (command) {
        case APP_CMD_INIT_WINDOW:
            __android_log_print(ANDROID_LOG_INFO, "torchlight", "window up: %dx%d",
                                ANativeWindow_getWidth(glue->window),
                                ANativeWindow_getHeight(glue->window));
            break;
        case APP_CMD_TERM_WINDOW:
            if (g_started) {
                g_window_lost = true;
                __android_log_print(ANDROID_LOG_INFO, "torchlight",
                                    "window taken away: ending the run");
            }
            break;
        case APP_CMD_LOST_FOCUS:
            // A button released while another window had the events would
            // otherwise stay held.
            release_pad();
            break;
        default: break;
    }
}

// The pad's events are the game's (platform_input.cpp); everything else -
// the phone's own Back gesture first - stays the system's, and Back finishes
// the activity, which ends the run.
int32_t on_input(android_app* glue, AInputEvent* event) {
    (void)glue;
    return handle_input_event(event) ? 1 : 0;
}

void wait_for(android_app* glue, bool (*done)(android_app*)) {
    while (!done(glue)) {
        android_poll_source* source = nullptr;
        int events = 0;
        if (ALooper_pollOnce(-1, nullptr, &events, reinterpret_cast<void**>(&source)) >= 0 &&
            source != nullptr) {
            source->process(glue, source);
        }
    }
}

// What the platform knows first: the window's own size (the interface and the
// camera's aspect are laid out for the size the engine is told, and the
// phone's is not the desktop's 1280x720) and where OGRE's shader library was
// pushed: `RTShaderLib/` at the data root (by the Android deploy step; the console names
// its directory at build time).
// Then `args.txt`: the arguments of the run, separated by white space - double
// quotes keep one together, as a shell's do (a `--pad-script` has `wait x30`)
// - which may override them. Absent is the ordinary case, and the defaults
// (main.cpp's device build) are the game.
// The first line of a small text file, or empty.
std::string first_line(const std::string& path) {
    std::FILE* in = std::fopen(path.c_str(), "rb");
    if (in == nullptr) return {};
    char buffer[1024] = {};
    const bool read = std::fgets(buffer, sizeof buffer, in) != nullptr;
    std::fclose(in);
    std::string line = read ? buffer : "";
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
    return line;
}

// Where the game's files are: the folder the launcher activity had the player
// choose (android/java, `game_data.txt` in the app's directory), else the
// app's directory itself, where a deploy over adb pushes them.
std::string game_data_folder() {
    std::string folder = first_line(platform_data_root() + "game_data.txt");
    if (folder.empty()) return platform_data_root();
    if (folder.back() != '/') folder += '/';
    return folder;
}

// Where OGRE's shader library is: unpacked by the launcher from the APK into
// the app's internal files (`RTShaderLib/`), else beside the game data, where a
// deploy over adb pushes it.
std::string shader_library_root() {
    const char* internal = app()->activity->internalDataPath;
    if (internal != nullptr) {
        const std::string root = std::string(internal) + "/";
        if (std::FILE* probe =
                std::fopen((root + "RTShaderLib/RTSLib_Lighting.glsl").c_str(), "rb")) {
            std::fclose(probe);
            return root;
        }
    }
    return platform_data_root();
}

std::vector<std::string> read_arguments(ANativeWindow* window) {
    const std::string data = game_data_folder();
    std::vector<std::string> args{"torchlight",
                                  "--width",
                                  std::to_string(ANativeWindow_getWidth(window)),
                                  "--height",
                                  std::to_string(ANativeWindow_getHeight(window)),
                                  "--ogre-media",
                                  shader_library_root(),
                                  "--game-data",
                                  data,
                                  "--console-ui",
                                  data + "console_ui"};
    const std::string path = platform_data_root() + "args.txt";
    std::FILE* in = std::fopen(path.c_str(), "rb");
    if (in == nullptr) return args;
    std::string word;
    bool quoted = false;
    bool any = false;  // "" is an argument, an empty one
    for (int c = std::fgetc(in); c != EOF; c = std::fgetc(in)) {
        if (c == '"') {
            quoted = !quoted;
            any = true;
        } else if (std::isspace(c) != 0 && !quoted) {
            if (any) args.push_back(word);
            word.clear();
            any = false;
        } else {
            word.push_back(static_cast<char>(c));
            any = true;
        }
    }
    if (any) args.push_back(word);
    std::fclose(in);
    return args;
}

}  // namespace

android_app* app() { return g_app; }

bool window_lost() { return g_window_lost; }

}  // namespace tl::core::android

void android_main(android_app* glue) {
    using namespace tl::core::android;
    g_app = glue;
    glue->onAppCmd = on_command;
    glue->onInputEvent = on_input;

    wait_for(glue, [](android_app* g) { return g->window != nullptr || g->destroyRequested != 0; });
    if (glue->destroyRequested != 0) return;

    std::vector<std::string> args = read_arguments(glue->window);
    std::vector<char*> argv;
    for (std::string& arg : args) argv.push_back(arg.data());
    argv.push_back(nullptr);
    std::string line;
    for (const std::string& arg : args) line += " " + arg;
    __android_log_print(ANDROID_LOG_INFO, "torchlight", "starting:%s", line.c_str());

    // The on-screen pad (core/touch_pad.h) starts off the screen: a tap puts
    // it there, and a controller's first press takes it off again.
    tl::core::touch_pad().set_screen(static_cast<float>(ANativeWindow_getWidth(glue->window)),
                                     static_cast<float>(ANativeWindow_getHeight(glue->window)));

    g_started = true;
    const int result = tl_main(static_cast<int>(args.size()), argv.data());
    __android_log_print(ANDROID_LOG_INFO, "torchlight", "the engine returned %d", result);

    ANativeActivity_finish(glue->activity);
    wait_for(glue, [](android_app* g) { return g->destroyRequested != 0; });
    _exit(result);
}
