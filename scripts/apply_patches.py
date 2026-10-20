#!/usr/bin/env python3
"""Apply patches/<tree>/*.patch to the pinned submodules under external/.

external/ogre and external/particleuniverse are pinned submodules and are NEVER
edited in place. Every change is a patch in patches/ogre/ or
patches/particleuniverse/:

- OGRE's are numbered, each guarded by __SWITCH__, each saying what it works
  around.
- ParticleUniverse's `0xxx-ogre14-*` port the library from OGRE 1.13 to the OGRE 14
  both targets use, guarded by OGRE_VERSION.
- ParticleUniverse's `tl-*` are **Torchlight's fork of ParticleUniverse**: Torchlight
  linked a modified, older copy, and each hunk transcribes one of its methods from
  the binary, citing `ParticleUniverse::Class::method (0xADDRESS)` like any other
  transcription. One patch per library file.

The way to change the library: edit external/particleuniverse in place as a
working copy, then `--save particleuniverse`, which writes every changed or new
file back into the patch that covers it (a new `tl-<file>.patch` otherwise). The
patches are the source; the working copy is regenerated from them.

Usage:
    python scripts/apply_patches.py            # apply, skipping any already applied
    python scripts/apply_patches.py --check    # report status, change nothing
    python scripts/apply_patches.py --reverse  # undo
    python scripts/apply_patches.py --save particleuniverse   # working copy -> patches
"""
import argparse
import collections
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

# (external/<name> and patches/<name>, required, what a failure most likely
# means). A tree that is not checked out is reported and skipped, except OGRE,
# without which nothing builds.
TREES = [
    ("ogre", True, "the pinned OGRE version may have changed - check external/ogre is at v14.5.2"),
    ("particleuniverse", False, "the pinned ParticleUniverse may have changed - check external/particleuniverse is at fa74677"),
]


def git_apply(tree: pathlib.Path, patch: pathlib.Path, *flags: str) -> bool:
    result = subprocess.run(
        ["git", "apply", *flags, str(patch)],
        cwd=tree, capture_output=True, text=True)
    return result.returncode == 0


def git(tree: pathlib.Path, *args: str) -> bytes:
    return subprocess.run(["git", *args], cwd=tree, capture_output=True, check=True).stdout


def save(name: str) -> int:
    """Write the working copy's changes back into patches/<name>/.

    Each changed or new file goes into the existing patch that already covers it,
    so a patch keeps its name and its neighbours; a file no patch covers gets a
    patch of its own, `tl-<file stem>.patch`. Output is `git diff` byte for byte -
    ParticleUniverse's sources are CRLF and its patches must stay so
    (.gitattributes marks them -text).
    """
    tree = ROOT / "external" / name
    patch_dir = ROOT / "patches" / name
    patch_dir.mkdir(parents=True, exist_ok=True)

    # A new file only shows in `git diff` once git knows of it.
    untracked = git(tree, "ls-files", "--others", "--exclude-standard").decode().split()
    if untracked:
        git(tree, "add", "--intent-to-add", *untracked)
    changed = git(tree, "diff", "--name-only").decode().split()

    owner = {}
    for patch in sorted(patch_dir.glob("*.patch")):
        for m in re.finditer(rb"^diff --git a/(\S+) b/", patch.read_bytes(), re.M):
            owner[m.group(1).decode()] = patch

    groups = collections.defaultdict(list)
    for path in changed:
        patch = owner.get(path) or patch_dir / ("tl-%s.patch" % pathlib.PurePosixPath(path).stem)
        groups[patch].append(path)
    # A patch whose files are all unchanged now would be written empty; keep it
    # and say so, since that means the working copy lost its change.
    for patch in sorted(set(owner.values()) - set(groups)):
        print("  unchanged in the working copy, left as it is: %s" % patch.name)

    for patch, paths in sorted(groups.items()):
        # Every file the patch already covers, not only the changed ones, so
        # saving never drops a file from a patch.
        covered = sorted(set(paths) | {p for p, owning in owner.items() if owning == patch})
        diff = git(tree, "diff", "--", *covered)
        old = patch.read_bytes() if patch.exists() else None
        if diff != old:
            patch.write_bytes(diff)
            print("  %s %s (%s)" % ("wrote" if old is not None else "new", patch.name, ", ".join(covered)))
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--reverse", action="store_true")
    ap.add_argument("--save", metavar="TREE", help="write external/TREE's working copy back into patches/TREE")
    args = ap.parse_args()

    if args.save:
        return save(args.save)

    failures = 0
    for name, required, hint in TREES:
        tree = ROOT / "external" / name
        if not (tree.is_dir() and any(tree.iterdir())):
            print(f"external/{name} is empty - run:")
            print(f"    git submodule update --init external/{name}")
            if required:
                return 1
            continue

        patches = sorted((ROOT / "patches" / name).glob("*.patch"))
        print(f"external/{name}:")
        tree_failures = 0
        for patch in patches:
            label = patch.name
            applied = git_apply(tree, patch, "--reverse", "--check")

            if args.check:
                print(f"  {'APPLIED ' if applied else 'pending '} {label}")
                continue

            if args.reverse:
                if not applied:
                    print(f"  skip (not applied) {label}")
                elif git_apply(tree, patch, "--reverse"):
                    print(f"  reversed {label}")
                else:
                    print(f"  FAILED to reverse {label}")
                    tree_failures += 1
                continue

            if applied:
                print(f"  already applied {label}")
            elif git_apply(tree, patch):
                print(f"  applied {label}")
            else:
                print(f"  FAILED {label}")
                tree_failures += 1

        if tree_failures:
            print(f"  {tree_failures} patch(es) failed: {hint}.")
        failures += tree_failures

    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
