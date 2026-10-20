# Third-party components

| component | where | licence |
| --- | --- | --- |
| OGRE 14 | `external/ogre` (submodule), changes in `patches/ogre/` | MIT |
| ParticleUniverse (OGRECave) | `external/particleuniverse` (submodule), changes in `patches/particleuniverse/` | MIT |
| stb_vorbis | fetched by `scripts/fetch_stb.py`, not committed | public domain / MIT |
| Mesa (switch-mesa) | Switch build, built by `scripts/build_mesa.py`, changes in `patches/mesa/` | MIT |
| libnx, devkitA64 runtime | Switch build | ISC; newlib's BSD-style licences; GCC runtime exception |
| FreeType, HarfBuzz, libpng, bzip2, zlib | Switch build: devkitPro's portlibs, linked statically. Android build: FreeType built with OGRE and linked statically; zlib is the system's | FTL, Old MIT, libpng, bzip2, zlib |

## The Windows build's DLLs

The Windows player folder (`scripts/package_windows.py`) ships these beside
`torchlight.exe` as DLLs. Each is MSYS2's own build of the library, unmodified:

| MSYS2 package | DLLs | licence |
| --- | --- | --- |
| gcc-libs | `libstdc++-6.dll`, `libgcc_s_seh-1.dll` | GPL-3.0+ with the GCC Runtime Library Exception |
| libwinpthread | `libwinpthread-1.dll` | MIT and BSD-3-Clause-Clear |
| freetype | `libfreetype-6.dll` | FTL (or GPL-2.0+) |
| harfbuzz | `libharfbuzz-0.dll` | Old MIT |
| graphite2 | `libgraphite2.dll` | LGPL-2.1+ |
| glib2 | `libglib-2.0-0.dll` | LGPL-2.1+ |
| gettext-runtime | `libintl-8.dll` | LGPL-2.1+ |
| libiconv | `libiconv-2.dll` | LGPL-2.1+ |
| pcre2 | `libpcre2-8-0.dll` | BSD-3-Clause |
| libpng | `libpng16-16.dll` | libpng |
| bzip2 | `libbz2-1.dll` | bzip2 |
| brotli | `libbrotlicommon.dll`, `libbrotlidec.dll` | MIT |
| zlib | `zlib1.dll` | zlib |

The packaging script works this list out from the program's imports and does
not use this table, so the folder always matches the build. It writes every
licence text into the folder's `licenses/` directory, and it writes a
`README.txt` that names the exact MSYS2 source archive for each package. Any
of these DLLs can be replaced with another build of the same library.

## The Linux build

The Linux player folder (`scripts/package_linux.py`) ships no libraries. X11,
OpenGL/EGL, ALSA, FreeType and zlib are the system's own. The folder carries
the licences of what is built into the program: OGRE, ParticleUniverse and
stb_vorbis.

Torchlight is a trademark of its owners. This project ships no part of the
game's data.
