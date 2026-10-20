#!/usr/bin/env python3
"""Pull the console HUD's interface data out of the Xbox 360 release.

**Why this exists.** The in-game HUD this engine draws is the Xbox 360 one -
`media/UI/hud_xenon.layout` and `media/UI/pethud_xenon.layout`, over the four
imagesets in `media/UI/icons2/`. Those files ship in that release's `pak.zip`
and in no other: neither the Steam `Pak.zip` nor the native Linux one has them,
because both carry the PC interface instead - a mouse-driven tray along the
bottom of the screen with ten numbered hotkeys. So a run against a PC install
has a game and no HUD unless it is told where a copy of the console data is.

This copies out the ten files that matter - 4 MB of a 300 MB archive - into a
directory the engine can mount beside the install:

    python scripts/extract_console_ui.py <xbox-data>/pak.zip console_ui
    ./build-desktop/torchlight.exe --game-data torchlight-steam \\
        --extra-data console_ui --layout "MEDIA/LAYOUTS/TOWN/1X1SINGLE_ROOM/TOWN.LAYOUT" \\
        --play --headless --hud-dump hud.png

and the same directory is small enough to put on the console's SD card, which
mounting the whole archive twice would not be.

**These are game assets and are never committed.** The output directory is the
user's own copy of their own purchase, exactly like the PC release's install.
`.gitignore` should keep it out, and this script
refuses to write inside the repository's tracked tree.
"""

import argparse
import os
import sys
import zipfile

# Everything the HUD reads, by archive path, lower-cased.
#
# The two layouts, and the four imagesets with their four sheets. A `.imageset`
# names its own texture, so the pairing is not guessed: `widgets1.imageset` says
# `Imagefile="media/ui/icons2/widgets1.dds"`.
WANTED = [
    "media/ui/hud_xenon.layout",
    "media/ui/pethud_xenon.layout",
    "media/ui/inventorymenu_xenon.layout",
    "media/ui/mastermenu_xenon.layout",
    "media/ui/charctermenu_xenon.layout",
    "media/ui/statsmenu_xenon.layout",
    "media/ui/journalmenu_xenon.layout",
    "media/ui/questmenu_xenon.layout",
    "media/ui/questdialogmenu_xenon.layout",
    "media/ui/petmenu_xenon.layout",
    "media/ui/resurrect_xenon.layout",
    # The enchanter, gem and retirement panes the vendor screens put in the left
    # column (`render/inventory_menu.cpp`, VendorScreen).
    "media/ui/enchantermenu_xenon.layout",
    # The item-select dialog A on a gem opens (`CMessageItemSelectXenon`, a
    # `CMessageHousingXenon`): the backdrop and the close button.
    "media/ui/messagehousing_xenon.layout",
    # The pet commands dialog Start opens on the inventory tab
    # (`CMessagePetCommandsXenon`, also a `CMessageHousingXenon`): the stance
    # tray and `Y Send Pet To Town`.
    "media/ui/petcommands_xenon.layout",
    # The replace-spell dialog A on a spell scroll opens when the owner's
    # slots are full (`CMessageSpellRemoveXenon`, also a housing message): four
    # spell boxes, `A Replace Spell`, `B Cancel`.
    "media/ui/spellremove_xenon.layout",
    # Fishing (`CFishingMenu`): the ring over the bobber, `A Catch`, `B Cancel`.
    "media/ui/fishing_xenon.layout",
    # The modal message (`CModalMenu`, `CGameUI::openModalDialog`): a title, a
    # line and `Ok` - what a catch reports.
    "media/ui/modalmenu_xenon.layout",
    # The skill screen (`CSkillMenuXenon`): the grid and description pane, and
    # the map-to-button sheet it lays over them.
    "media/ui/skillsmenu_xenon.layout",
    "media/ui/skillmap_xenon.layout",
    # The waypoint menu (`CWaypointMenu`) A on the town's waypoint portal
    # opens: ten `Button`/`Choice` rows, a title and B to close.
    "media/ui/waypointmenu_xenon.layout",
    # The Pause Menu (`COptionsMenu`, Start in the game) and the Options
    # screen it opens (`CSettingsMenuXenon`). The console loads their CEGUI
    # exports (`optionsmenu.uilayout`, `settingsmenu_xenon.uilayout`); these
    # are the `[UI]` sources of the same two screens.
    "media/ui/options_xenon.layout",
    "media/ui/settingsmenu_xenon.layout",
    # The context tip window (`CTipMenu`): the text, `Y Disable Tips` and
    # `B Close`. The console loads its CEGUI export (`tipmenu.uilayout`); this
    # is the `[UI]` source of the same window.
    "media/ui/tipmenu_xenon.layout",
    # Help (`CHowToMenu`): the pad's picture with each button's use and
    # `B Close`. The console loads its CEGUI export (`howto_xenon.uilayout`);
    # this is the `[UI]` source of the same screen.
    "media/ui/howto_xenon.layout",
    # `NAVIGATE`, which every console housing screen binds to its slot 0x43
    # (`CMasterHousingXenon`) and which no PC release carries.
    "media/sounds/ui/menunavigate.wav",

    # The front end. `mainmenu.uilayout` is the console's own main menu and
    # ships **only** in the Xbox archive and **only** as the CEGUI export -
    # unlike the HUD, there is no `[UI]` source beside it.
    # `charactercreate_xenon.layout` is the console's character creation, which
    # is a different screen from the PC's `charactercreate.layout`.
    "media/ui/mainmenu.uilayout",
    # The difficulty screen New Game opens after the character is made
    # (`CDifficultyMenu`; the console's builder names this file).
    "media/ui/difficulty.uilayout",
    # Load Character (`CContinueGameMenu`; its builder names this file).
    # The `characterload.layout` beside it is the PC's screen.
    "media/ui/characterload.uilayout",
    # The loading screen (`CGameUI::setLoadingVisible`): black, the artwork, the
    # hourglass and the tip strip. Its art is the `Loading` imageset, which the
    # PC archive carries in the same geometry, and the `widgets*` sets above.
    "media/ui/loading.uilayout",
    "media/ui/charactercreate.uilayout",
    "media/ui/charactercreate_xenon.layout",
    "media/ui/icons2/widgets.imageset",
    "media/ui/icons2/widgets.dds",
    "media/ui/icons2/widgets1.imageset",
    "media/ui/icons2/widgets1.dds",
    "media/ui/icons2/widgets2.imageset",
    "media/ui/icons2/widgets2.dds",
    "media/ui/icons2/ratings.imageset",
    "media/ui/icons2/ratings.dds",

    # The console's **third pet**, which the PC releases have not got.
    #
    # `charactercreate.uilayout` offers Dog, Cat and `Chakawary`, and the last
    # of those ships only in the Xbox archive - model, skeletons, texture, its
    # unit and the fish that turns a pet back into one. Without these the third
    # button on the character screen would be a picture of an animal that
    # cannot be spawned.
    "media/units/monsters/chakawary/chakawary.dat.adm",
    "media/units/items/fish/fish_chakawary_return.dat.adm",

    # Not interface at all, and the only non-interface file here.
    #
    # `FISH_TROLLFISH_PERM.DAT.adm` is the Xbox release's `TrollFish_Giant` -
    # the `Improbably Large Fish` that turns a pet into a troll permanently -
    # and it ships in *neither* PC release: their `Pak.zip`s carry
    # `FISH_TROLLFISH`, which is the same fish for thirty seconds, and no
    # permanent twin. It is 400 bytes of authored record naming a mesh, an icon
    # and a unit that the PC archive already has, so extracting it costs
    # nothing and means the engine reads the real thing instead of the
    # transcription in `tl::core::xbox_starting_fish()`.
    "media/units/items/fish/fish_trollfish_perm.dat.adm",
]

# **The fonts are deliberately not copied.** The console layouts name `Serif`,
# `Serif14`, `SerifBig` and `SerifHuge`, all of which are `BRLNSDB.TTF` at
# different point sizes - and every PC release ships that file already, so the
# engine finds it in the main archive, which is always mounted. The Xbox copy is
# byte-identical to the Steam one, checked.
#
# Copying it anyway would put a second `BRLNSDB.TTF` in the index under a
# basename that is already taken. OGRE indexes archive entries by basename with
# `OGRE_RESOURCEMANAGER_STRICT=0` and a duplicate basename is the failure this
# project keeps rediscovering (see ADR 0002). A self-contained directory would
# be worth that risk if the engine could run from it alone; it cannot, so it is not.
OPTIONAL = []

# **The console's own copies of files the PC install also has**, written under
# `xbox/` so they do not shadow the install's (an extra mount never wins over
# the install, `Renderer`'s resource setup) and do not collide with its
# basenames in a way a full path cannot resolve.
#
# The skill screen needs them because the Xbox release re-laid the classes'
# skills for a pad: its class files place each skill on a two-column grid
# (`PANE`/`ROW`/`COLUMN`) where the PC ones draw a three-column tree, and a
# few requirements moved (the Alchemist's Adventurer from level 10 to 15). The
# skill and affix files differ in their numbers here and there too (effect
# `DURATION`s, `RANGE`), which is what the screen's descriptions print. And
# `UI.DAT` is where `Navigate` is defined.
XBOX_TREES = [
    ("media/units/players/", ".adm"),
    ("media/skills/", ".adm"),
    ("media/affixes/", ".adm"),
]
XBOX_FILES = [
    "media/sounds/ui.dat.adm",
    # `[CONTEXTTIPS]`: the console words its tips for a pad ("press LB to drink
    # a health potion") and has one the PC lacks, `TIP_SKILLS`.
    "media/globals.dat.adm",
]

# **The console's exclusive gear**: three six-piece armour sets, one per class,
# that ship only in the Xbox archive - `Thunderforge` (Destroyer), `Empyrean`
# (Vanquisher) and `Chronogear` (Alchemist), level-20 uniques (`RARITY:250`)
# that drop from the ordinary loot rolls. Every file is new to a PC install, so
# each goes at its own path like the Chakawary's: the 18 item records and the
# 3 set records (all named `360_*`), and the worn looks the items' `[WARDROBE]`
# blocks name - `media/wardrobe/<class>/360_*` meshes and materials and the
# `<class>_360_*` textures.
EXCLUSIVE_PREFIXES = ("media/units/items/", "media/sets/")
EXCLUSIVE_WARDROBE = "media/wardrobe/"

# The pieces' icons (`360_destroyer_body` and the other 14) live in the Xbox's
# `itemicons13` sheet, which has the same path as the PC's but different
# contents: it adds those 15 and lacks 26 the PC's has (most of the spell
# scrolls). An extra mount never shadows the install, and replacing the PC
# sheet would lose those 26, so the Xbox sheet is written beside it under a name
# of its own - imageset `xbox_itemicons13`, its `Imagefile` rewritten to the
# renamed texture - which the engine loads after the PC's sheets (a name the
# PC's sheets already hold keeps the PC's picture).
ICON_SHEET = "media/ui/itemicons/itemicons13"
ICON_SHEET_RENAMED = "media/ui/itemicons/xbox_itemicons13"

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("pak", help="the Xbox release's pak.zip")
    ap.add_argument("out", help="directory to write the extracted tree into")
    args = ap.parse_args()

    out = os.path.abspath(args.out)
    for tracked in ("src", "scripts", "docs", "tests", "tools", "cmake"):
        if out == os.path.join(REPO, tracked) or out.startswith(os.path.join(REPO, tracked) + os.sep):
            print(f"refusing to write game assets into {out}", file=sys.stderr)
            return 1

    try:
        archive = zipfile.ZipFile(args.pak)
    except (OSError, zipfile.BadZipFile) as err:
        print(f"cannot read {args.pak}: {err}", file=sys.stderr)
        return 1

    # The archive is cased inconsistently, like everything else in this game's
    # data, so it is indexed by lower-cased path rather than opened by name.
    index = {name.lower(): name for name in archive.namelist()}

    # The Chakawary's model directory, whole: meshes, skeletons and its skin.
    for name in index:
        if (name.startswith("media/models/monsters/chakawary/") and not name.endswith("/")
                and name not in WANTED):
            WANTED.append(name)

    missing = [want for want in WANTED if want not in index]
    if missing:
        print(f"{args.pak} is not the Xbox release: it has none of", file=sys.stderr)
        for name in missing:
            print(f"  {name}", file=sys.stderr)
        return 1

    written = 0
    total = 0
    for want in WANTED + OPTIONAL:
        real = index.get(want)
        if real is None:
            continue
        data = archive.read(real)
        # Written under the archive's own casing, so the paths the engine asks
        # for resolve on the console's case-sensitive filesystem too.
        target = os.path.join(out, real.replace("/", os.sep))
        os.makedirs(os.path.dirname(target), exist_ok=True)
        with open(target, "wb") as handle:
            handle.write(data)
        written += 1
        total += len(data)
        print(f"  {real}  ({len(data) / 1024.0:.0f} KB)")

    xbox = [name for name in index
            for prefix, suffix in XBOX_TREES
            if name.startswith(prefix) and name.endswith(suffix)]
    xbox += [name for name in XBOX_FILES if name in index]
    for want in sorted(set(xbox)):
        real = index[want]
        data = archive.read(real)
        target = os.path.join(out, "xbox", real.replace("/", os.sep))
        os.makedirs(os.path.dirname(target), exist_ok=True)
        with open(target, "wb") as handle:
            handle.write(data)
        written += 1
        total += len(data)
    print(f"  xbox/: {len(set(xbox))} console copies of class, skill, affix and sound files")

    exclusive = [name for name in index
                 if (name.startswith(EXCLUSIVE_PREFIXES) and
                     os.path.basename(name).startswith("360_") and name.endswith(".adm")) or
                 (name.startswith(EXCLUSIVE_WARDROBE) and not name.endswith("/") and
                  "360_" in os.path.basename(name))]
    for want in sorted(exclusive):
        real = index[want]
        data = archive.read(real)
        target = os.path.join(out, real.replace("/", os.sep))
        os.makedirs(os.path.dirname(target), exist_ok=True)
        with open(target, "wb") as handle:
            handle.write(data)
        written += 1
        total += len(data)
    print(f"  {len(exclusive)} file(s) of the console-only sets (Thunderforge, Empyrean, Chronogear)")

    sheet = index.get(ICON_SHEET + ".imageset")
    texture = index.get(ICON_SHEET + ".dds")
    if sheet is not None and texture is not None:
        text = archive.read(sheet).decode("utf-8")
        text = text.replace('Name= "itemicons13"', 'Name= "xbox_itemicons13"', 1)
        text = text.replace('Name="itemicons13"', 'Name="xbox_itemicons13"', 1)
        text = text.replace(ICON_SHEET + ".dds", ICON_SHEET_RENAMED + ".dds", 1)
        if "xbox_itemicons13" not in text:
            print(f"{sheet}: unexpected header, icon sheet not written", file=sys.stderr)
            return 1
        # In the archive's own casing of the directory (`media/UI/itemicons/`).
        folder = os.path.dirname(sheet)
        renamed = folder + "/" + os.path.basename(ICON_SHEET_RENAMED)
        for rel, data in ((renamed + ".imageset", text.encode("utf-8")),
                          (renamed + ".dds", archive.read(texture))):
            target = os.path.join(out, rel.replace("/", os.sep))
            os.makedirs(os.path.dirname(target), exist_ok=True)
            with open(target, "wb") as handle:
                handle.write(data)
            written += 1
            total += len(data)
        print(f"  {ICON_SHEET_RENAMED}.imageset/.dds  (the sets' icons)")

    print(f"{written} file(s), {total / (1024.0 * 1024.0):.1f} MB -> {out}")
    print()
    print("mount it with:  --console-ui " + args.out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
