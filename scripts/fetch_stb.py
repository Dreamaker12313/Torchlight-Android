#!/usr/bin/env python3
"""Fetch the vendored third-party sources this engine compiles in.

At the moment that is one file: `stb_vorbis.c`, the public-domain Ogg Vorbis
decoder. Torchlight's music and its ambient loops are Vorbis, and there is no
library on either toolchain that can be used on both -

  * this machine's `C:\\msys64\\mingw64\\lib` has no libogg, libvorbis, OpenAL
    or SDL2 at all;
  * devkitPro's portlibs have libavcodec, libopenal and libSDL2, but there is
    nothing on the desktop to pair them with, so the two targets would decode
    through different code and the desktop would stop being predictive of the
    console - which is the thing this port trades everything else for.

One public-domain C file compiles identically on both, links statically (the
Switch has no dynamic loader - constraint 3), and needs no build system. It is
**not committed**: `.gitignore` excludes `external/stb/`, the same way
`external/ogre` is a submodule rather than a copy.

    python scripts/fetch_stb.py
"""

import argparse
import hashlib
import pathlib
import sys
import urllib.request

# Pinned to a tag rather than to `master`, so a rebuild a year from now gets the
# file this engine was written against. stb has no releases; the repository's
# own tags are the only version handle it offers.
SOURCES = {
    "stb_vorbis.c": "https://raw.githubusercontent.com/nothings/stb/f58f558c120e9b32c217290b80bad1a0729fbb2c/stb_vorbis.c",
}

DEST = pathlib.Path(__file__).resolve().parent.parent / "external" / "stb"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--force", action="store_true",
                        help="re-download even if the file is already there")
    args = parser.parse_args()

    DEST.mkdir(parents=True, exist_ok=True)
    for name, url in SOURCES.items():
        target = DEST / name
        if target.exists() and not args.force:
            print(f"{target} already present ({target.stat().st_size} bytes)")
            continue
        print(f"fetching {url}")
        try:
            with urllib.request.urlopen(url, timeout=60) as response:
                data = response.read()
        except Exception as error:  # noqa: BLE001 - report and fail, whatever it was
            print(f"could not fetch {name}: {error}", file=sys.stderr)
            return 1
        target.write_bytes(data)
        digest = hashlib.sha256(data).hexdigest()[:16]
        print(f"wrote {target} ({len(data)} bytes, sha256 {digest}...)")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
