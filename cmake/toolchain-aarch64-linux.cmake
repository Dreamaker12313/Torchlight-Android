# AArch64 Linux, run under qemu-user: the Switch's instruction set without the
# Switch. Not a console build - no devkitPro, no OGRE - but the unit tests run
# on the same FPU the console has, which is where a transcription that only
# holds on x86 SSE would show (see -ffp-contract=off in CMakeLists.txt).
#
#   apt install g++-aarch64-linux-gnu qemu-user
#   cmake -S . -B build-aarch64 -G Ninja \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-aarch64-linux.cmake
#   ninja -C build-aarch64 tl_tests && ctest --test-dir build-aarch64
#
# ctest runs each test through CMAKE_CROSSCOMPILING_EMULATOR; by hand it is
# `qemu-aarch64 -L /usr/aarch64-linux-gnu build-aarch64/tests/tl_tests`.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

set(TL_AARCH64_SYSROOT /usr/aarch64-linux-gnu CACHE PATH "the cross toolchain's target libraries")
set(CMAKE_FIND_ROOT_PATH ${TL_AARCH64_SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(CMAKE_CROSSCOMPILING_EMULATOR qemu-aarch64 -L ${TL_AARCH64_SYSROOT})
