# Source paths in the binaries (__FILE__ in asserts and messages, debug info)
# are written relative to the checkout, so a build names no folder of the
# machine it was made on. Included by the engine's CMakeLists.txt, and by
# OGRE's build through CMAKE_PROJECT_INCLUDE_BEFORE (scripts/build_ogre.py).
get_filename_component(_tl_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_tl_roots "${_tl_root}")
# devkitPro's MSYS2 CMake spells C:/x as /c/x; its compiler is given C:/x.
if(_tl_root MATCHES "^/([a-zA-Z])/(.*)$")
    string(TOUPPER "${CMAKE_MATCH_1}" _tl_drive)
    list(APPEND _tl_roots "${_tl_drive}:/${CMAKE_MATCH_2}")
endif()
foreach(_tl_r IN LISTS _tl_roots)
    add_compile_options("-ffile-prefix-map=${_tl_r}/=")
endforeach()
