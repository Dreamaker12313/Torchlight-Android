# ParticleUniverse - the particle engine Torchlight's effects run on.
#
# The OGRECave library
# (https://github.com/OGRECave/particleuniverse, MIT) is the first source for
# the particle runtime, checked class by class against Torchlight's own older
# copy in the binary. It is a pinned submodule in external/particleuniverse and,
# like external/ogre, is never edited in place: every change is a file in
# patches/particleuniverse/, applied by scripts/apply_patches.py.
#
# Built here from its sources as one static library rather than through its own
# CMakeLists, which wants find_package(OGRE CONFIG) and a shared build. There is
# no plugin to install: this version's entry point is the
# ParticleUniverseRoot singleton (initialise/shutdown), which registers its
# MovableObject factories with Ogre::Root directly - nothing is dlopen'd, so the
# Switch's missing loader is not in the way.
#
# Its own library, like tl_stb, so tl_set_warnings()'s -Werror never reaches
# third-party code. Optional the same way OGRE is: without OGRE, or without the
# submodule checked out, the engine builds and draws its particles as before.

set(TL_PU_DIR "${CMAKE_SOURCE_DIR}/external/particleuniverse/Plugins/ParticleUniverse")

# Off only to tell apart what the library changes from what it does not: a
# build without it is the engine exactly as it was before it came in.
option(TL_WITH_PARTICLEUNIVERSE "link ParticleUniverse when it is checked out" ON)

set(TL_HAVE_PARTICLEUNIVERSE FALSE)
if(NOT TL_WITH_PARTICLEUNIVERSE)
    message(STATUS "ParticleUniverse: left out (TL_WITH_PARTICLEUNIVERSE=OFF)")
elseif(TL_HAVE_OGRE AND EXISTS "${TL_PU_DIR}/src/ParticleUniverseRoot.cpp")
    set(TL_HAVE_PARTICLEUNIVERSE TRUE)
elseif(TL_HAVE_OGRE)
    message(STATUS
        "No ParticleUniverse at ${TL_PU_DIR} - building without it. "
        "Run: git submodule update --init external/particleuniverse && "
        "python scripts/apply_patches.py")
endif()

if(TL_HAVE_PARTICLEUNIVERSE)
    # The same two globs as the library's own CMakeLists. The PhysX externs are
    # in the list but compile to nothing: they sit behind PU_PHYSICS_PHYSX,
    # which ParticleUniversePrerequisites.h leaves undefined.
    file(GLOB TL_PU_SOURCES CONFIGURE_DEPENDS
        "${TL_PU_DIR}/src/*.cpp"
        "${TL_PU_DIR}/src/*/*.cpp")

    add_library(tl_particleuniverse STATIC ${TL_PU_SOURCES})

    # The library targets C++11; C++17 removed things code of its age still
    # uses. OGRE 14's headers need 14, so that is the floor and the ceiling.
    set_target_properties(tl_particleuniverse PROPERTIES
        CXX_STANDARD 14
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)

    # SYSTEM, like OGRE's: the engine's own sources include these headers under
    # -Werror, and the library's headers do not survive -Wreorder and friends.
    target_include_directories(tl_particleuniverse PRIVATE "${TL_PU_DIR}/src")
    target_include_directories(tl_particleuniverse SYSTEM PUBLIC
        "${TL_PU_DIR}/include"
        "${CMAKE_SOURCE_DIR}/external/ogre/OgreMain/include"
        "${TL_OGRE_BUILD_DIR}/include")

    # OGRE_STATIC_LIB also turns ParticleUniverse's own export macro into
    # nothing (ParticleUniversePrerequisites.h), which is what a static archive
    # linked into one executable wants on both targets.
    target_compile_definitions(tl_particleuniverse PUBLIC OGRE_STATIC_LIB)

    if(NINTENDO_SWITCH)
        # The same additions the engine gets in cmake/switch.cmake: portlibs is
        # not on devkitPro's include path, newlib hides locale_t without
        # _GNU_SOURCE, and OGRE's headers need exceptions and RTTI.
        target_include_directories(tl_particleuniverse SYSTEM PRIVATE "${DKP_PORTLIBS}/include")
        target_compile_definitions(tl_particleuniverse PRIVATE _GNU_SOURCE)
        target_compile_options(tl_particleuniverse PRIVATE -fexceptions -frtti)
    endif()

    # Third-party code: quiet, and never -Werror.
    if(NOT MSVC)
        target_compile_options(tl_particleuniverse PRIVATE -w)
    endif()
endif()
