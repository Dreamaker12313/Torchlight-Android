// The render window's surface on Android, which the app going to the background takes
// away and gives back, and the camera's aspect to the window. Split from
// renderer_world.cpp.
#include "render/renderer_impl.h"

#if defined(TL_PLATFORM_ANDROID)
#include "platform/android/android_glue.h"

#include <OgreAndroidEGLWindow.h>
#endif

#include <utility>

namespace tl::render {

void Renderer::Impl::watch_window_surface() {
#if defined(TL_PLATFORM_ANDROID)
    tl::core::android::SurfaceHooks hooks;
    hooks.lost = [this] {
        if (window != nullptr)
            static_cast<Ogre::AndroidEGLWindow*>(window)->_notifySurfaceDestroyed();
    };
    hooks.restored = [this](ANativeWindow* native, AConfiguration* configuration) {
        if (window == nullptr) return;
        static_cast<Ogre::AndroidEGLWindow*>(window)->_notifySurfaceCreated(native, configuration);
        // Another screen of a foldable phone: the viewports and the camera to its size. The
        // interface stays laid out for the size the run started at.
        for (unsigned short i = 0; i < window->getNumViewports(); ++i) {
            window->getViewport(i)->_updateDimensions();
        }
        fit_camera_to_window();
        TL_LOG_INFO("window: surface made again, %ux%u", window->getWidth(), window->getHeight());
    };
    tl::core::android::set_surface_hooks(std::move(hooks));
#endif
}

void Renderer::Impl::fit_camera_to_window() const {
    if (camera == nullptr || window == nullptr || window->getNumViewports() == 0) return;
    const Ogre::Viewport* viewport = window->getViewport(0);
    camera->setAspectRatio(static_cast<Ogre::Real>(viewport->getActualWidth()) /
                           static_cast<Ogre::Real>(viewport->getActualHeight()));
}

}  // namespace tl::render
