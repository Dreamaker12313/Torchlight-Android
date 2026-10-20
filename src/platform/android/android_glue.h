// What the Android platform files share: the NativeActivity's glue, which
// android_main() (android_main.cpp) is handed and keeps for the rest of the run.
#pragma once

struct android_app;
struct AInputEvent;

namespace tl::core::android {

/// Keep the game controller's state from one of the activity's input events
/// (platform_input.cpp). True when the event was the pad's, and so the game's.
bool handle_input_event(const AInputEvent* event);

/// Let go of every button and stick, when the activity loses the events.
void release_pad();

/// The activity's glue, set before the engine starts.
android_app* app();

/// True once the window the engine draws into has been taken away (the app
/// sent to the background, a foldable phone switching screens). The run then ends.
bool window_lost();

}  // namespace tl::core::android
