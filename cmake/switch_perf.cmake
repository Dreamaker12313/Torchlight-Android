# Link-time and profile-guided optimisation for the Switch build.
#
# Off by default: the console build is what it was unless one of these is asked
# for, and each variant lives in a build directory of its own.
#
#   -DTL_LTO=ON          link-time optimisation across the engine,
#                        ParticleUniverse and OGRE
#   -DTL_PGO=generate    instrumented build: the console writes a profile to
#                        sdmc:/switch/torchlight/pgo while the game is played
#                        (the port's PGO glue)
#   -DTL_PGO=use         optimise with that profile, fetched from the console's
#                        card into pgo/
#
# One file for two builds. OGRE is configured separately (scripts/build_ogre.py)
# and the frame's time is mostly spent inside it, so the same flags have to
# reach its objects: the script hands this file to OGRE's build as
# CMAKE_PROJECT_INCLUDE, and the engine includes it from cmake/switch.cmake.
#
# None of this changes arithmetic. Neither LTO nor a profile licenses the
# compiler to reorder or fuse floating-point operations (no -ffast-math
# anywhere), and tl_set_warnings()'s -ffp-contract=off is recorded per function
# and survives the link-time pass.
include_guard(GLOBAL)

option(TL_LTO "Switch: link-time optimisation across the engine, ParticleUniverse and OGRE" OFF)
set(TL_PGO "" CACHE STRING "Switch: profile-guided optimisation - empty, generate or use")
set(TL_PGO_DIR "${CMAKE_CURRENT_LIST_DIR}/../pgo"
    CACHE PATH "Where the profile fetched from the console is kept (gitignored)")

if(TL_LTO)
    message(STATUS "Torchlight: link-time optimisation on")
    # Slim objects: the archives are made by gcc-ar (devkitPro's toolchain file
    # already sets it), which reads the symbols out of the intermediate code.
    add_compile_options(-flto -fno-fat-lto-objects)
    add_link_options(-flto=8 -flto-partition=balanced)
endif()

if(TL_PGO)
    if(NOT TL_PGO STREQUAL "generate" AND NOT TL_PGO STREQUAL "use")
        message(FATAL_ERROR "TL_PGO must be empty, generate or use (not '${TL_PGO}')")
    endif()

    # The compiler is a native Windows program and names a profile file after
    # the object's path as Windows spells it: <profile dir>\<object path, with
    # every / turned into #>. -fprofile-prefix-path takes the build directory
    # off the front, so the instrumented build and the one that uses the profile
    # agree on the name although they are different directories - but it is
    # compared as text, and only the backslash spelling matches (measured:
    # with C:/... the compiler warns "does not start with profile prefix" and
    # keeps the whole path). CMake here is an MSYS2 program and knows the
    # directory as /c/..., hence cygpath.
    execute_process(COMMAND cygpath -w "${CMAKE_BINARY_DIR}"
        OUTPUT_VARIABLE TL_PGO_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE TL_PGO_CYGPATH)
    execute_process(COMMAND cygpath -m "${TL_PGO_DIR}"
        OUTPUT_VARIABLE TL_PGO_DIR_NATIVE OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT TL_PGO_CYGPATH EQUAL 0 OR NOT TL_PGO_PREFIX OR NOT TL_PGO_DIR_NATIVE)
        message(FATAL_ERROR "TL_PGO needs cygpath (devkitPro's MSYS2 shell) to spell the build directory")
    endif()
    message(STATUS "Torchlight: profile-guided optimisation: ${TL_PGO}, profile in ${TL_PGO_DIR_NATIVE}")

    # -fno-profile-values: the indirect-call profiler reads tpidr_el0, which is
    # always 0 on Horizon under -mtp=soft, so an instrumented build dies at
    # start-up without it. It has to be the same on both sides, or the two
    # disagree on what a function's profile holds.
    if(TL_PGO STREQUAL "generate")
        add_compile_options(
            "-fprofile-generate=${TL_PGO_DIR_NATIVE}"
            "-fprofile-prefix-path=${TL_PGO_PREFIX}"
            -fprofile-update=single -fno-profile-values)
        add_link_options(-fprofile-generate)
    else()
        # -fprofile-correction: the counters are plain increments shared by the
        # game's threads, so a few come out inconsistent, and without this the
        # compiler rejects the file. -fprofile-partial-training: a play session
        # never reaches all of the game, and what it missed is to be compiled
        # as usual, not as cold code to be made small. A source file edited
        # since the session no longer matches its profile and is simply
        # compiled without one - a warning, never an error.
        add_compile_options(
            "-fprofile-use=${TL_PGO_DIR_NATIVE}"
            "-fprofile-prefix-path=${TL_PGO_PREFIX}"
            -fprofile-partial-training -fprofile-correction -fno-profile-values
            -Wno-missing-profile -Wno-error=coverage-mismatch)
        add_link_options(-fprofile-use -fprofile-partial-training -fprofile-correction)
    endif()
endif()

# The engine's own part of the instrumented build: libgcov opens each profile
# file by the path the compiler gave it, a directory of this PC. --wrap sends
# every fopen through platform_pgo.cpp, which turns those into files on the
# card; --undefined pulls that object out of tl_core's archive whether or not
# anything ahead of it on the link line happens to call fopen.
function(tl_perf_configure_executable target)
    if(TL_PGO STREQUAL "generate")
        target_compile_definitions(tl_core PRIVATE TL_PGO_GENERATE=1)
        target_link_options(${target} PRIVATE -Wl,--wrap=fopen -Wl,--undefined=__wrap_fopen)
    endif()
endfunction()
