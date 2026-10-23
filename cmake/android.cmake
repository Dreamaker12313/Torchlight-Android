# Android target configuration: the phone test build.
#
# Configured with the NDK's android.toolchain.cmake, which sets ANDROID;
# `python scripts/build_apk.py game` does it and packages the result. The engine
# is a shared library here, libtorchlight.so, because that is what a
# NativeActivity loads; inside it everything is static, as on the other targets
# (OGRE, its plugins by installPlugin()). Only the system's own libraries are
# dynamic, which no Android app avoids.

set(TL_OGRE_BUILD_DIR "${CMAKE_SOURCE_DIR}/build-ogre-android"
    CACHE PATH "Directory containing the Android build of OGRE")

if(EXISTS "${TL_OGRE_BUILD_DIR}/lib")
    set(TL_HAVE_OGRE TRUE)
else()
    set(TL_HAVE_OGRE FALSE)
    message(STATUS
        "No Android OGRE at ${TL_OGRE_BUILD_DIR} - building without rendering. "
        "Run: python scripts/build_ogre.py --platform android")
endif()

# The NDK's glue: the activity's callbacks, turned into a looper that
# android_main() (platform/android/android_main.cpp) polls on its own thread.
# Its own library, compiled without the port's -Werror, and SYSTEM so its
# header does not trip the warnings of the files that include it.
set(TL_APP_GLUE "${ANDROID_NDK}/sources/android/native_app_glue")
add_library(tl_android_glue STATIC "${TL_APP_GLUE}/android_native_app_glue.c")
target_include_directories(tl_android_glue SYSTEM PUBLIC "${TL_APP_GLUE}")
# nativewindow: `ANativeWindow_setFrameRate` (API 30), which libandroid does not carry.
target_link_libraries(tl_android_glue PUBLIC android nativewindow log)

# The NDK's CPU probe, which OgreMain calls on Android (android_getCpuFamily,
# for its SIMD choice); shipped as source, like the glue.
set(TL_CPU_FEATURES "${ANDROID_NDK}/sources/android/cpufeatures")
add_library(tl_android_cpufeatures STATIC "${TL_CPU_FEATURES}/cpu-features.c")
target_include_directories(tl_android_cpufeatures SYSTEM PUBLIC "${TL_CPU_FEATURES}")
target_link_libraries(tl_android_cpufeatures PUBLIC dl)

# Only what the renderer installs on this target: GLES2, STBI, RTSS, Overlay.
# OgreMainStatic first, for the reason cmake/switch.cmake gives.
set(TL_OGRE_LIBS
    OgreMainStatic
    RenderSystem_GLES2Static
    OgreGLSupportStatic
    Codec_STBIStatic
    OgreRTShaderSystemStatic
    OgreOverlayStatic
)

function(tl_configure_platform target)
    target_compile_definitions(${target} PRIVATE TL_PLATFORM_ANDROID=1)

    # NativeActivity finds the glue's entry point by name, and the glue finds
    # android_main() in tl_core; nothing else refers to either.
    target_link_options(${target} PRIVATE "-Wl,-u,ANativeActivity_onCreate" "-Wl,-u,android_main")
    target_link_libraries(${target} PRIVATE tl_android_glue)

    if(NOT TL_HAVE_OGRE)
        return()
    endif()
    target_compile_definitions(${target} PRIVATE TL_HAVE_OGRE=1 OGRE_STATIC_LIB)

    # OGRE's shader library is pushed beside the game data and found at run
    # time under the data root (main.cpp sets --ogre-media's default), so the
    # build-time directory the other targets name is empty here.
    target_compile_definitions(${target} PRIVATE TL_OGRE_MEDIA_DIR="")

    target_include_directories(${target} SYSTEM PRIVATE
        "${CMAKE_SOURCE_DIR}/external/ogre/Components/Overlay/include"
        "${CMAKE_SOURCE_DIR}/external/ogre/OgreMain/include"
        "${CMAKE_SOURCE_DIR}/external/ogre/RenderSystems/GLES2/include"
        "${CMAKE_SOURCE_DIR}/external/ogre/RenderSystems/GLSupport/include"
        # The Android EGL window, which the renderer releases before a restart.
        "${CMAKE_SOURCE_DIR}/external/ogre/RenderSystems/GLSupport/include/EGL"
        "${CMAKE_SOURCE_DIR}/external/ogre/RenderSystems/GLSupport/include/EGL/Android"
        "${CMAKE_SOURCE_DIR}/external/ogre/Components/RTShaderSystem/include"
        "${CMAKE_SOURCE_DIR}/external/ogre/PlugIns/STBICodec/include"
        "${TL_OGRE_BUILD_DIR}/include"
    )

    # OGRE built FreeType itself (OGRE_BUILD_DEPENDENCIES, scripts/build_ogre.py)
    # into its Dependencies directory; zlib is the NDK's.
    target_link_directories(${target} PRIVATE
        "${TL_OGRE_BUILD_DIR}/lib" "${TL_OGRE_BUILD_DIR}/Dependencies/lib")
    target_link_libraries(${target} PRIVATE
        -Wl,--start-group ${TL_OGRE_LIBS} -Wl,--end-group
        tl_android_cpufeatures freetype EGL GLESv3 GLESv2 z)
endfunction()
