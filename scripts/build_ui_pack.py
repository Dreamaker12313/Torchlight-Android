"""Build the port's own interface pack: plain-line art and layouts of our own,
for running the console interface on a PC `Pak.zip` without any Xbox file.

    python scripts/build_ui_pack.py [--out build-ui-pack]
    torchlight --game-data <PC install> --console-ui build-ui-pack ...

Everything in the pack is drawn or written here: the sprites with Pillow (its
bundled font for the glyph letters), the layouts in Runic's `[UI]` text format.
The only thing kept from the console's files is the contract the engine reads:
the file paths, the `WIDGET NAME`s the code looks up, and the image names the
code asks for itself (the floor labels' frame, the pet stances' states).
Positions, sizes, nesting below those names and every picture are our own.

Pilot scope (2026-10-21): the HUD (`hud_xenon.layout`, `pethud_xenon.layout`)
and the floor labels. The other screens still need the console's files.
"""
import argparse
import os

from PIL import Image, ImageDraw, ImageFilter, ImageFont

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Supersampling: sprites are drawn this many times larger and reduced, for
# smooth edges. Sprites are stored at twice their layout size (the HUD stretches
# them to the layout's rectangle), except the label frame, whose corner size the
# engine reads off the sheet in console pixels.
SS = 4
HUD_RES = 2

INK = (232, 226, 210, 235)        # the lines
INK_DIM = (150, 146, 136, 200)
PANEL = (14, 16, 22, 185)         # panel fill
PANEL_DARK = (6, 7, 10, 215)
GOLD = (226, 186, 92, 255)
RED = (196, 44, 40, 255)
RED_LIGHT = (236, 96, 84, 255)
BLUE = (48, 102, 214, 255)
BLUE_LIGHT = (104, 156, 246, 255)
PAD_A = (98, 186, 72, 255)
PAD_B = (214, 66, 56, 255)
PAD_X = (56, 124, 222, 255)
PAD_Y = (234, 190, 50, 255)


def font(px):
    return ImageFont.load_default(size=px)


class Canvas:
    """A sprite being drawn: `w` x `h` output pixels, drawn at SS times that."""

    def __init__(self, w, h):
        self.w, self.h = w, h
        self.img = Image.new("RGBA", (w * SS, h * SS), (0, 0, 0, 0))
        self.d = ImageDraw.Draw(self.img)

    def s(self, *v):
        return [x * SS for x in v]

    def rrect(self, x0, y0, x1, y1, r, fill=None, outline=None, width=1.0):
        self.d.rounded_rectangle(self.s(x0, y0, x1, y1), radius=r * SS, fill=fill,
                                 outline=outline, width=max(1, round(width * SS)))

    def ellipse(self, x0, y0, x1, y1, fill=None, outline=None, width=1.0):
        self.d.ellipse(self.s(x0, y0, x1, y1), fill=fill, outline=outline,
                       width=max(1, round(width * SS)))

    def line(self, pts, fill=INK, width=1.0):
        self.d.line([c * SS for p in pts for c in p], fill=fill, width=max(1, round(width * SS)),
                    joint="curve")

    def polygon(self, pts, fill=None, outline=None, width=1.0):
        self.d.polygon([c * SS for p in pts for c in p], fill=fill, outline=outline,
                       width=max(1, round(width * SS)))

    def text(self, cx, cy, s, px, fill=INK):
        self.d.text((cx * SS, cy * SS), s, font=font(px * SS), fill=fill, anchor="mm")

    def done(self):
        return self.img.resize((self.w, self.h), Image.LANCZOS)


# --------------------------------------------------------------------------
# The sprites. Each function takes the output size in pixels.
# --------------------------------------------------------------------------

def orb_frame(n):
    c = Canvas(n, n)
    k = n / 100.0
    c.ellipse(2 * k, 2 * k, 98 * k, 98 * k, fill=PANEL_DARK, outline=INK, width=2.2 * k)
    c.ellipse(7 * k, 7 * k, 93 * k, 93 * k, outline=INK_DIM, width=0.8 * k)
    return c.done()


def over(under, top):
    """`top` blended onto `under` (ImageDraw itself replaces pixels, alpha included)."""
    return Image.alpha_composite(under, top)


def orb_fill(n, base, light):
    c = Canvas(n, n)
    c.ellipse(0, 0, n, n, fill=base)
    # A flat highlight: a lighter oval near the top.
    shine = Canvas(n, n)
    shine.ellipse(n * 0.22, n * 0.10, n * 0.62, n * 0.34, fill=light[:3] + (110,))
    return over(c.done(), shine.done())


def slot_frame(n):
    c = Canvas(n, n)
    k = n / 48.0
    c.rrect(1.5 * k, 1.5 * k, 46.5 * k, 46.5 * k, 6 * k, fill=PANEL, outline=INK, width=1.6 * k)
    return c.done()


def face_button(n, colour, letter):
    c = Canvas(n, n)
    k = n / 21.0
    c.ellipse(1 * k, 1 * k, 20 * k, 20 * k, fill=PANEL_DARK, outline=colour, width=2 * k)
    c.text(n / 2, n / 2 + 0.5 * k, letter, 12 * k, fill=colour)
    return c.done()


def bumper(w, h, label, right):
    c = Canvas(w, h)
    k = h / 24.0
    r = 9 * k
    c.rrect(1 * k, 3 * k, w - 1 * k, h - 1 * k, r, fill=PANEL_DARK, outline=INK, width=1.6 * k)
    c.text(w / 2, h / 2 + 1 * k, label, 11 * k)
    return c.done()


def trigger(w, h, label, right):
    c = Canvas(w, h)
    k = w / 26.0
    # A trigger seen from above: a rounded slab, higher on its outer side.
    outer, inner = (w - 1 * k, 1 * k) if right else (1 * k, w - 1 * k)
    pts = [(inner, 8 * k), (outer, 2 * k), (outer, h - 3 * k), (inner, h - 3 * k)]
    c.polygon(pts, fill=PANEL_DARK, outline=INK, width=1.6 * k)
    c.text(w / 2, h * 0.58, label, 10 * k)
    return c.done()


def back_button(w, h):
    c = Canvas(w, h)
    k = h / 22.0
    c.rrect(1 * k, 2 * k, w - 1 * k, h - 2 * k, 8 * k, fill=PANEL_DARK, outline=INK, width=1.6 * k)
    m = h / 2
    c.polygon([(w / 2 + 3 * k, m - 4 * k), (w / 2 - 4 * k, m), (w / 2 + 3 * k, m + 4 * k)], fill=INK)
    return c.done()


def bag(w, h):
    c = Canvas(w, h)
    k = w / 38.0
    # A sack: a wide rounded body, a gathered neck, a tie across it and the
    # flared mouth above.
    c.polygon([(12 * k, 16 * k), (26 * k, 16 * k), (35 * k, 30 * k), (32 * k, 41 * k),
               (6 * k, 41 * k), (3 * k, 30 * k)], fill=PANEL, outline=INK, width=2 * k)
    c.polygon([(11 * k, 6 * k), (27 * k, 6 * k), (23 * k, 14 * k), (15 * k, 14 * k)],
              fill=PANEL, outline=INK, width=2 * k)
    c.line([(10 * k, 16 * k), (28 * k, 16 * k)], fill=GOLD, width=2.6 * k)
    return c.done()


def plus(w, h, glow):
    c = Canvas(w, h)
    k = w / 31.0
    cx, cy = w / 2, h / 2
    arm, half = 10 * k, 3 * k
    pts = [(cx - half, cy - arm), (cx + half, cy - arm), (cx + half, cy - half), (cx + arm, cy - half),
           (cx + arm, cy + half), (cx + half, cy + half), (cx + half, cy + arm), (cx - half, cy + arm),
           (cx - half, cy + half), (cx - arm, cy + half), (cx - arm, cy - half), (cx - half, cy - half)]
    c.polygon(pts, fill=GOLD, outline=PANEL_DARK, width=1 * k)
    img = c.done()
    if glow:
        halo = img.filter(ImageFilter.GaussianBlur(radius=3 * k))
        return Image.alpha_composite(halo, halo)
    return img


def bar_frame(w, h):
    c = Canvas(w, h)
    c.rrect(0.5, 0.5, w - 0.5, h - 0.5, h / 2, outline=INK, width=1.2)
    return c.done()


def flat(w, h, colour, radius=0.0):
    c = Canvas(w, h)
    if radius > 0:
        c.rrect(0, 0, w, h, radius, fill=colour)
    else:
        c.d.rectangle([0, 0, w * SS, h * SS], fill=colour)
    return c.done()


def bar_fill(w, h, base, light):
    c = Canvas(w, h)
    c.d.rectangle([0, 0, w * SS, h * SS], fill=base)
    shine = Canvas(w, h)
    shine.d.rectangle([0, 0, w * SS, h * SS * 0.35], fill=light[:3] + (140,))
    return over(c.done(), shine.done())


def pet_base(w, h):
    c = Canvas(w, h)
    k = h / 46.0
    c.rrect(1 * k, 1 * k, w - 1 * k, h - 1 * k, 7 * k, fill=PANEL, outline=INK, width=1.4 * k)
    return c.done()


def stance(n, kind, active):
    c = Canvas(n, n)
    k = n / 23.0
    ink = INK if active else INK_DIM
    fill = PANEL_DARK if active else (6, 7, 10, 140)
    ring = GOLD if active else INK_DIM
    c.ellipse(1 * k, 1 * k, 22 * k, 22 * k, fill=fill, outline=ring, width=1.6 * k)
    m = n / 2
    if kind == "aggressive":
        # Crossed blades.
        c.line([(7 * k, 7 * k), (16 * k, 16 * k)], fill=ink, width=2 * k)
        c.line([(16 * k, 7 * k), (7 * k, 16 * k)], fill=ink, width=2 * k)
    elif kind == "defensive":
        # A shield.
        c.polygon([(m, 5 * k), (17 * k, 7.5 * k), (16 * k, 14 * k), (m, 18.5 * k), (7 * k, 14 * k),
                   (6 * k, 7.5 * k)], outline=ink, width=1.8 * k)
    else:
        # Passive: a pause mark.
        c.line([(9.5 * k, 7 * k), (9.5 * k, 16 * k)], fill=ink, width=2.2 * k)
        c.line([(13.5 * k, 7 * k), (13.5 * k, 16 * k)], fill=ink, width=2.2 * k)
    return c.done()


def frame_pieces(prefix, corner, fill=PANEL, outline=INK, inner=None):
    """A nine-slice frame `<prefix>topleft` .. `<prefix>middle`, cut from one
    rounded box: corners `corner` px square, edges and middle `corner` px on
    their stretched side. Stored at 1x: the screens read the corner's size off
    the sheet in authored pixels. `inner` adds a second, fainter line inside."""
    n = corner * 3
    c = Canvas(n, n)
    c.rrect(0.5, 0.5, n - 0.5, n - 0.5, corner - 2, fill=fill, outline=outline, width=1.2)
    if inner is not None:
        c.rrect(3.5, 3.5, n - 3.5, n - 3.5, corner - 4, outline=inner, width=0.8)
    img = c.done()
    cut = lambda x, y: img.crop((x * corner, y * corner, (x + 1) * corner, (y + 1) * corner))
    names = [["topleft", "topedge", "topright"], ["leftedge", "middle", "rightedge"],
             ["bottomleft", "bottomedge", "bottomright"]]
    return {prefix + names[y][x]: cut(x, y) for y in range(3) for x in range(3)}


def plate(w, h, outline=INK, fill=PANEL_DARK):
    """A header or reward plate: a flat rounded bar."""
    c = Canvas(w, h)
    k = h / 36.0
    c.rrect(1 * k, 1 * k, w - 1 * k, h - 1 * k, h * 0.45, fill=fill, outline=outline, width=1.6 * k)
    return c.done()


def choice_button(w, h, lit):
    c = Canvas(w, h)
    k = h / 96.0
    c.rrect(2 * k, 2 * k, w - 2 * k, h - 2 * k, 12 * k, fill=PANEL if not lit else (40, 36, 24, 210),
            outline=GOLD if lit else INK_DIM, width=(3 if lit else 2) * k)
    return c.done()


def item_box(w, h, outline=INK_DIM, fill=PANEL):
    c = Canvas(w, h)
    k = w / 63.0
    c.rrect(1 * k, 1 * k, w - 1 * k, h - 1 * k, 6 * k, fill=fill, outline=outline, width=1.4 * k)
    return c.done()


def socket(n):
    c = Canvas(n, n)
    k = n / 16.0
    c.ellipse(2 * k, 2 * k, 14 * k, 14 * k, fill=PANEL_DARK, outline=INK_DIM, width=1.4 * k)
    return c.done()


def stick(w, h, arrows):
    """The left stick, with arrows for the directions it is pushed: "lr" or "ud"."""
    c = Canvas(w, h)
    k = h / 37.0
    cx, cy = w / 2, h / 2
    c.ellipse(cx - 13 * k, cy - 13 * k, cx + 13 * k, cy + 13 * k, fill=PANEL_DARK, outline=INK,
              width=1.6 * k)
    c.text(cx, cy + 0.5 * k, "L", 11 * k)
    a = 5 * k
    if "l" in arrows:
        c.polygon([(cx - 17 * k, cy - a), (cx - 25 * k, cy), (cx - 17 * k, cy + a)], fill=INK)
        c.polygon([(cx + 17 * k, cy - a), (cx + 25 * k, cy), (cx + 17 * k, cy + a)], fill=INK)
    return c.done()


def screen_sprites():
    """The pieces the death screen, the quest offer and the tip window add."""
    r = HUD_RES
    out = {
        "facet_panel_bottom": plate(240 * r, 32 * r),
        "quest_button_incomplete": choice_button(285 * r, 96 * r, False),
        "quest_button_rollover": choice_button(285 * r, 96 * r, True),
        "item_transmute_window": item_box(63 * r, 88 * r),
        "item_window_small": item_box(40 * r, 65 * r, outline=(0, 0, 0, 0), fill=PANEL_DARK),
        "item_window_rollover": item_box(60 * r, 86 * r, outline=GOLD, fill=(0, 0, 0, 0)),
        "socket_empty": socket(16 * r),
        "button_x": face_button(21 * r, PAD_X, "X"),
        "analog_button_left_lr": stick(67 * r, 37 * r, "lr"),
    }
    # `frame*` (`ceguiWidget/LineFrame`) and the item tooltips' frames are the
    # menu's (`ui_pack_menu.py`): the first sheet to name an image wins, so one copy.
    return out


def hud_sprites():
    r = HUD_RES
    out = {
        "hpmanaball": orb_frame(100 * r),
        "hpball": orb_fill(82 * r, RED, RED_LIGHT),
        "manaball": orb_fill(82 * r, BLUE, BLUE_LIGHT),
        "spells_window": slot_frame(48 * r),
        "a_button": face_button(21 * r, PAD_A, "A"),
        "b_button": face_button(21 * r, PAD_B, "B"),
        "x_button": face_button(21 * r, PAD_X, "X"),
        "y_button": face_button(21 * r, PAD_Y, "Y"),
        "left_bumper": bumper(36 * r, 22 * r, "LB", False),
        "right_bumper": bumper(36 * r, 22 * r, "RB", True),
        "left_trigger": trigger(24 * r, 34 * r, "LT", False),
        "right_trigger": trigger(24 * r, 34 * r, "RT", True),
        "back_button": back_button(29 * r, 22 * r),
        "bag": bag(38 * r, 44 * r),
        "plus": plus(31 * r, 36 * r, False),
        "plus_glow": plus(31 * r, 36 * r, True),
        "xpbar": bar_frame(280 * r, 6 * r),
        "xbarfill": bar_fill(280 * r, 6 * r, GOLD, (255, 230, 150, 255)),
        # Opaque, as the Xbox's: the layouts dim with it through `ALPHA`.
        "black": flat(16, 16, (0, 0, 0, 255)),
        "slider_bar": bar_frame(120 * r, 6 * r),
        "pethealth": bar_fill(120 * r, 6 * r, RED, RED_LIGHT),
        "petmana": bar_fill(120 * r, 6 * r, BLUE, BLUE_LIGHT),
        "pethud_base": pet_base(140 * r, 46 * r),
    }
    for kind in ("aggressive", "defensive", "passive"):
        out[f"petstance_{kind}"] = stance(23 * r, kind, True)
        out[f"petstance_{kind}_inactive"] = stance(23 * r, kind, False)
        out[f"petstance_{kind}_rollover"] = stance(23 * r, kind, True)
    # `ceguiWidget/ItemText`'s soft panel (`tooltip*`: the floor labels, the screens' frames) is
    # not drawn: every screen finds the PC's `GuiLook` `Tooltip*` by name.
    return out


# --------------------------------------------------------------------------
# Sheets: a shelf packer, a PNG and its `.imageset` per sheet.
# --------------------------------------------------------------------------

def write_sheet(out_dir, name, sprites, size=1024):
    sheet = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    placed = []
    x = y = shelf = 0
    pad = 2
    for key, img in sorted(sprites.items(), key=lambda kv: (-kv[1].height, kv[0])):
        w, h = img.size
        if x + w + pad > size:
            x, y, shelf = 0, y + shelf + pad, 0
        if y + h > size:
            raise SystemExit(f"sheet {name} is full at {key}")
        sheet.paste(img, (x + 1, y + 1))
        placed.append((key, x + 1, y + 1, w, h))
        x += w + pad
        shelf = max(shelf, h)
    texture = f"media/UI/icons2/pack_{name}.png"
    sheet.save(os.path.join(out_dir, texture))
    lines = ['<?xml version="1.0" encoding="UTF-8"?>',
             f'<Imageset Name="{name}" Imagefile="{texture}" NativeHorzRes="1024" '
             'NativeVertRes="768" AutoScaled="true" >']
    for key, px, py, w, h in placed:
        lines.append(f'\t<Image Name="{key}" XPos="{px}" YPos="{py}" Width="{w}" Height="{h}" />')
    lines.append("</Imageset>")
    with open(os.path.join(out_dir, f"media/UI/icons2/{name}.imageset"), "w", encoding="utf-8",
              newline="\r\n") as fh:
        fh.write("\n".join(lines) + "\n")


# --------------------------------------------------------------------------
# Layouts, in `[UI]` text. An object is (descriptor, name, {property: value},
# [children]); `widget` in the properties becomes `WIDGET NAME`.
# --------------------------------------------------------------------------

TYPES = {"OFFSET X": "FLOAT", "OFFSET Y": "FLOAT", "WIDTH": "FLOAT", "HEIGHT": "FLOAT",
         "SCALE WIDTH": "FLOAT", "SCALE HEIGHT": "FLOAT", "VISIBLE": "BOOL",
         "ALWAYS ON TOP": "BOOL", "CLIPPED": "BOOL", "WORD WRAP": "BOOL", "TEXT": "TRANSLATE",
         "ALPHA": "FLOAT"}


def obj(descriptor, name, children=(), **props):
    return (descriptor, name, props, list(children))


def at(x=0, y=0, w=None, h=None, ha=None, va=None):
    """Placement in the 1024 x 768 authored space: offset, extent, and which
    edge of the parent it hangs from (LEFT/CENTER/RIGHT, TOP/CENTER/BOTTOM)."""
    p = {}
    if x:
        p["OFFSET X"] = x
    if y:
        p["OFFSET Y"] = y
    if w is not None:
        p["WIDTH"] = w
    if h is not None:
        p["HEIGHT"] = h
    if ha:
        p["HORIZONTAL"] = ha
    if va:
        p["VERTICLE"] = va
    return p


def full():
    return {"WIDTH": 0, "HEIGHT": 0, "SCALE WIDTH": 1, "SCALE HEIGHT": 1}


def render(objects):
    serial = [0]
    lines = []

    def count(o):
        return 1 + sum(count(c) for c in o[3])

    def emit(o, depth, parent):
        descriptor, name, props, children = o
        serial[0] += 1
        me = serial[0]
        t = "\t" * depth
        lines.append(f"{t}[BASEOBJECT]")
        lines.append(f"{t}\t[PROPERTIES]")
        lines.append(f"{t}\t\t<STRING>DESCRIPTOR:{descriptor}")
        lines.append(f"{t}\t\t<STRING>NAME:{name}")
        if "widget" in props:
            lines.append(f"{t}\t\t<STRING>WIDGET NAME:{props['widget']}")
        lines.append(f"{t}\t\t<INTEGER64>ID:{me}")
        lines.append(f"{t}\t\t<INTEGER64>PARENTID:{parent}")
        for key, value in props.items():
            if key == "widget":
                continue
            kind = TYPES.get(key, "STRING")
            if isinstance(value, str):
                # One property a line: a break inside a text is written `\n`.
                value = value.replace("\n", "\\n")
            if kind == "BOOL":
                value = "true" if value else "false"
            elif kind == "FLOAT":
                value = f"{value:g}"
            lines.append(f"{t}\t\t<{kind}>{key}:{value}")
        lines.append(f"{t}\t[/PROPERTIES]")
        lines.append(f"{t}\t[CHILDREN]")
        for child in children:
            emit(child, depth + 2, me)
        lines.append(f"{t}\t[/CHILDREN]")
        lines.append(f"{t}[/BASEOBJECT]")

    total = sum(count(o) for o in objects)
    head = ["[UI]", "\t<INTEGER>VERSION:3", f"\t<UNSIGNED INT>COUNT:{total}", "\t[OBJECTS]"]
    for o in objects:
        emit(o, 2, -1)
    return "\n".join(head + lines + ["\t[/OBJECTS]", "[/UI]"]) + "\n"


def skill_slot(button_image, glyph_w, glyph_h, slot, x):
    """One of the four face slots at the bottom centre: its frame, the button
    glyph over its top edge, the slot window and its label."""
    return obj("Image", f"slot{slot}", [
        obj("Image", "glyph", **at(0, -glyph_h + 8, glyph_w, glyph_h, ha="CENTER"),
            IMAGE=button_image, **{"ALWAYS ON TOP": True, "CLIPPED": False}),
        obj("Window", "icon", widget=f"SkillSlot{slot}", **at(0, 0, 36, 36, "CENTER", "CENTER")),
        obj("Text", "label", widget=f"SkillText{slot}", **at(-4, -2), **full(),
            **{"ALWAYS ON TOP": True, "HORIZONTAL ALIGN": "RIGHT", "VERTICAL ALIGN": "BOTTOM"}),
    ], IMAGE="spells_window", **at(x, -34, 48, 48, "CENTER", "BOTTOM"))


def skill_set(widget, slots):
    """`SkillSet1` / `SkillSet2`: the four slots in the order LT, Y, B, RT."""
    glyphs = [("left_trigger", 24, 34), ("y_button", 21, 21), ("b_button", 21, 21),
              ("right_trigger", 24, 34)]
    xs = [-111, -37, 37, 111]
    return obj("Window", widget.lower(), [
        skill_slot(g[0], g[1], g[2], s, x) for g, s, x in zip(glyphs, slots, xs)
    ], widget=widget, **full())


def orb(widget, fill_widget, image, side):
    sign = 1 if side == "LEFT" else -1
    return obj("Image", widget.lower(), [
        obj("Window", "fill", [
            obj("Image", "liquid", widget=f"{fill_widget}Sub", IMAGE=image, **at(0, 0, 82, 82)),
        ], widget=fill_widget, **at(9, 9, 82, 82)),
    ], widget=widget, IMAGE="hpmanaball", **at(sign * 24, 20, 100, 100, side, "TOP"))


def potion(slot, count_widget, bumper_image, side):
    """A potion slot beside its orb, the bumper that drinks it under it."""
    sign = 1 if side == "LEFT" else -1
    return obj("Image", f"potion{slot}", [
        obj("Image", "glyph", IMAGE=bumper_image, **at(0, 42, 36, 22, ha="CENTER"),
            **{"ALWAYS ON TOP": True, "CLIPPED": False}),
        obj("Window", "icon", [
            obj("Text", "count", widget=count_widget, FONT="Serif14",
                **at(2, 2, 40, 18, "RIGHT", "BOTTOM"),
                **{"ALWAYS ON TOP": True, "HORIZONTAL ALIGN": "RIGHT", "VERTICAL ALIGN": "BOTTOM"}),
        ], widget=f"SkillSlot{slot}", **at(0, 0, 36, 36, "CENTER", "CENTER")),
    ], IMAGE="spells_window", **at(sign * 132, 34, 48, 48, side, "TOP"))


def hud_layout():
    return [
        orb("PlayerHealth", "PlayerHealthBar", "hpball", "LEFT"),
        orb("PlayerMana", "PlayerManaBar", "manaball", "RIGHT"),
        potion(1, "POTIONCOUNT", "left_bumper", "LEFT"),
        potion(2, "MANACOUNT", "right_bumper", "RIGHT"),
        # The two banks of four; the code shows one at a time.
        skill_set("SkillSet1", [6, 4, 5, 7]),
        skill_set("SkillSet2", [9, 3, 8, 10]),
        obj("Image", "bag", [
            obj("Image", "glyph", IMAGE="back_button", **at(18, -8, 24, 18),
                **{"ALWAYS ON TOP": True, "CLIPPED": False}),
            obj("Image", "glow", widget="AvailableSkillsGlowIcon", IMAGE="plus_glow",
                **at(-8, -10, 24, 28), CLIPPED=False),
            obj("Image", "plus", widget="AvailableSkillsIcon", IMAGE="plus",
                **at(-8, -10, 24, 28), CLIPPED=False),
        ], IMAGE="bag", **at(200, -36, 30, 35, "CENTER", "BOTTOM")),
        obj("Window", "experience", [
            obj("Image", "track", [
                obj("Image", "fill", widget="ExperienceBar", IMAGE="xbarfill", **at(0, 0, 600, 6)),
            ], IMAGE="black", **at(0, 0, 600, 6)),
            obj("Image", "rim", IMAGE="xpbar", **at(0, 0, 600, 6), **{"ALWAYS ON TOP": True}),
        ], widget="PlayerXP", **at(0, -18, 600, 6, "CENTER", "BOTTOM")),
        obj("Text", "level name", widget="LevelName", COLOR="C8FFFFFF",
            **at(-24, 126, 320, 18, ha="RIGHT"), **{"HORIZONTAL ALIGN": "RIGHT"}),
        obj("Image", "target", [
            obj("Text", "name", widget="TargetName", **at(0, -22, 420, 18, ha="CENTER"),
                **{"HORIZONTAL ALIGN": "CENTER"}),
            obj("Window", "track", [
                obj("Image", "fill", widget="TargetHealthBar", IMAGE="pethealth", **at(0, 0, 236, 6)),
            ], **at(2, 2, 236, 6)),
            obj("Text", "level", widget="TargetLevel", COLOR="B4FFFFFF",
                **at(0, 14, 240, 18, ha="CENTER"), **{"HORIZONTAL ALIGN": "CENTER"}),
            obj("Text", "description", widget="TargetDescription", COLOR="FFFBCA04",
                **at(0, 32, 420, 40, ha="CENTER"),
                **{"HORIZONTAL ALIGN": "CENTER", "WORD WRAP": True}),
        ], widget="TargetHealth", IMAGE="slider_bar", **at(0, 44, 240, 10, ha="CENTER"),
            **{"ALWAYS ON TOP": True}),
        obj("CEGUI Object", "interact", [
            obj("Image", "glyph", IMAGE="a_button", **at(2, 3, 21, 21), CLIPPED=False),
            obj("Text", "word", TEXT="Interact", **at(28, 4, 90, 20), CLIPPED=False),
        ], widget="TargetInteractHover", **at(0, 0, 110, 27)),
        obj("Text Outlined", "banner small", widget="SmallMessageText", FONT="SerifBig",
            **at(0, -70, 850, 55, "CENTER", "CENTER"),
            **{"ALWAYS ON TOP": True, "HORIZONTAL ALIGN": "CENTER"}),
        obj("Text Outlined", "banner large", widget="LargeMessageText", FONT="SerifHuge",
            **at(0, -125, 850, 55, "CENTER", "CENTER"),
            **{"ALWAYS ON TOP": True, "HORIZONTAL ALIGN": "CENTER"}),
    ]


def pethud_layout():
    def stance_button(widget, kind, x):
        return obj("Button", kind, widget=widget, NORMAL=f"PETSTANCE_{kind.upper()}",
                   DISABLED=f"PETSTANCE_{kind.upper()}_INACTIVE",
                   HOVER=f"PETSTANCE_{kind.upper()}_ROLLOVER", CLICKED=f"PETSTANCE_{kind.upper()}",
                   **at(x, 0, 22, 22))

    return [
        obj("Image", "pet panel", [
            obj("Window", "stances", [
                stance_button("PetAggressive", "aggressive", 0),
                stance_button("PetDefensive", "defensive", 25),
                stance_button("PetPassive", "passive", 50),
            ], **at(9, 6, 74, 22)),
            obj("Text", "fleeing", widget="FleeingText", TEXT="Fleeing", COLOR="FFF07060",
                **at(84, 7, 52, 18), **{"ALWAYS ON TOP": True}),
            obj("Window", "bars", [
                obj("Image", "health", widget="PetHealthBar", IMAGE="pethealth", **at(0, 0, 122, 4)),
                obj("Image", "mana", widget="PetManaBar", IMAGE="petmana", **at(0, 6, 122, 4)),
            ], **at(9, 32, 122, 10)),
        ], IMAGE="pethud_base", **at(24, -24, 140, 46, "LEFT", "BOTTOM")),
    ]


GOLD_TEXT = "FFF0C850"


def prompt(name, glyph, words, x, y, w=150, widget=None, glyph_w=21, glyph_h=21):
    """A pad prompt: the button's glyph and what it does, in gold."""
    props = {"widget": widget} if widget else {}
    return obj("Window", name, [
        obj("Image", "glyph", IMAGE=glyph, **at(0, (22 - glyph_h) / 2, glyph_w, glyph_h),
            **{"ALWAYS ON TOP": True, "CLIPPED": False}),
        obj("Text", "words", TEXT=words, FONT="Serif14", COLOR=GOLD_TEXT,
            **at(glyph_w + 6, 0, w - glyph_w - 6, 24), **{"ALWAYS ON TOP": True}),
    ], **props, **at(x, y, w, 24))


def header(title_widget, text, w=220):
    """The plate across a frame's top edge, with the screen's name on it."""
    title = {"widget": title_widget} if title_widget else {}
    return obj("Image", "header", [
        obj("Text Outlined", "title", TEXT=text, FONT="SerifBig", **title,
            **at(0, 3, w, 26, ha="CENTER"), **{"HORIZONTAL ALIGN": "CENTER", "CLIPPED": False}),
    ], IMAGE="facet_panel_bottom", **at(0, -17, w, 32, ha="CENTER"),
        **{"ALWAYS ON TOP": True, "CLIPPED": False})


def death_layout():
    def choice(n, y):
        return obj("Button", f"choice {n}", [
            obj("Text", "words", widget=f"Choice{n}", **at(14, 8, 392, 56),
                **{"HORIZONTAL ALIGN": "CENTER", "VERTICAL ALIGN": "CENTER", "WORD WRAP": True}),
        ], widget=f"Resurrect{n}", NORMAL="QUEST_BUTTON_INCOMPLETE",
            HOVER="QUEST_BUTTON_ROLLOVER", CLICKED="QUEST_BUTTON_ROLLOVER",
            **at(0, y, 420, 72, ha="CENTER"))

    return [
        obj("CEGUI Object", "frame", [
            header("Title", "You have died"),
            choice(1, 36),
            choice(2, 120),
            choice(3, 204),
            obj("Window", "exit", [
                prompt("exit", "y_button", "Exit to Title", 0, 0, 170),
                obj("Text", "warning", TEXT="(progress since last autosave will be lost)",
                    COLOR="B4FFFFFF", **at(0, 26, 420, 20)),
            ], **at(0, 296, 420, 50, ha="CENTER")),
            obj("Window", "hardcore", [
                obj("Text", "words",
                    TEXT="This character was killed in hardcore mode. They are permanently "
                         "dead. You are not able to continue playing with them.",
                    FONT="SerifBig", **at(0, 0, -60, 0, ha="CENTER"),
                    **{"SCALE WIDTH": 1, "SCALE HEIGHT": 1, "HORIZONTAL ALIGN": "CENTER",
                       "VERTICAL ALIGN": "CENTER", "WORD WRAP": True}),
            ], widget="hardcoretext", **at(0, 0, 0, 0), **{"SCALE WIDTH": 1, "SCALE HEIGHT": 1}),
        ], widget="FrameB", **{"LOOK N FEEL": "ceguiWidget/ItemText"},
            **at(0, 170, 520, 360, ha="CENTER")),
    ]


def quest_dialog_layout():
    def item(n, x):
        return obj("Image", f"item {n}", [
            obj("Image", "slot", widget=f"ItemSlot{n}", IMAGE="item_window_small",
                **at(9, 8, 36, 58)),
            obj("Image", "socket 0", widget=f"EmptySocket{n}_0", IMAGE="socket_empty",
                **at(4, 14, 14, 14), **{"ALWAYS ON TOP": True}),
            obj("Image", "socket 1", widget=f"EmptySocket{n}_1", IMAGE="socket_empty",
                **at(4, 30, 14, 14), **{"ALWAYS ON TOP": True}),
        ], widget=f"ItemBkg{n}", IMAGE="item_transmute_window", **at(x, 344, 54, 74))

    def reward(widget, x):
        return obj("Image", widget.lower(), [
            obj("Text Outlined", "figure", widget=widget, TEXT="0", FONT="SerifBig",
                **at(0, 3, 150, 24), **{"HORIZONTAL ALIGN": "CENTER"}),
        ], widget=f"{widget}Box", IMAGE="facet_panel_bottom", **at(x, 362, 150, 30))

    return [
        obj("CEGUI Object", "frame", [
            obj("Image", "portrait", widget="Portrait", **at(-36, -36, 104, 104),
                **{"ALWAYS ON TOP": True, "CLIPPED": False}),
            obj("Image", "text box", [
                obj("Text", "text", widget="DialogText", FONT="SerifBig",
                    **at(0, 0, 600, 260), **{"WORD WRAP": True}),
            ], widget="DialogTextFrame", **at(84, 18, 560, 316)),
            reward("RewardGold", 20),
            reward("RewardXP", 180),
            reward("RewardFame", 340),
            item(1, 512),
            item(2, 570),
            item(3, 628),
            obj("Image", "highlight", widget="ItemHighlight", IMAGE="item_window_rollover",
                **at(512, 344, 54, 74), **{"ALWAYS ON TOP": True}),
            obj("Window", "prompts", [
                prompt("rewards", "analog_button_left_lr", "Rewards", 0, 0, 140,
                       widget="RewardsTooltip", glyph_w=46, glyph_h=26),
                prompt("fast scroll", "button_x", "Fast Scroll", 150, 0, 150, widget="Fast Scroll"),
                prompt("accept", "a_button", "Accept", 410, 0, 120, widget="Accept"),
                prompt("decline", "b_button", "Decline", 540, 0, 120, widget="Decline"),
                prompt("ok", "a_button", "OK", 540, 0, 120, widget="Ok"),
            ], **at(20, 432, 660, 24)),
        ], widget="FrameB", FONT="Serif14", **{"LOOK N FEEL": "ceguiWidget/ItemText"},
            **at(0, 120, 700, 470, ha="CENTER")),
    ]


def tip_layout():
    return [
        obj("Image", "dim", IMAGE="black", ALPHA=0.45, **full()),
        obj("CEGUI Object", "frame", [
            header("Title", "Tip", 160),
            obj("Text", "words", widget="Dialog", FONT="Serif14", TEXT="",
                **at(0, 26, 420, 196, ha="CENTER"),
                **{"HORIZONTAL ALIGN": "CENTER", "VERTICAL ALIGN": "CENTER", "WORD WRAP": True}),
            obj("Window", "prompts", [
                prompt("disable", "y_button", "Disable Tips", 0, 0, 170),
                prompt("close", "b_button", "Close", 280, 0, 100),
            ], **at(0, 236, 380, 24, ha="CENTER")),
        ], widget="FrameB", **{"LOOK N FEEL": "ceguiWidget/LineFrame"},
            **at(0, 230, 480, 280, ha="CENTER")),
    ]


def check_case(out_dir):
    """The engine asks for the console's paths exactly (`media/UI/...`), and the
    phone's and the Switch's file systems match case. Windows keeps the case of
    a folder that already exists, so a pack written over an older one can come
    out spelt differently from what was asked: stop then."""
    for path in ("media", "media/UI", "media/UI/icons2"):
        parent, name = os.path.split(os.path.join(out_dir, path))
        if name not in os.listdir(parent):
            raise SystemExit(f"{os.path.join(out_dir, path)} is spelt differently on disk "
                             f"(an older pack's folder); delete {out_dir} and run again")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(REPO, "build-ui-pack"))
    args = ap.parse_args()
    os.makedirs(os.path.join(args.out, "media", "UI", "icons2"), exist_ok=True)

    import ui_pack_front
    import ui_pack_menu

    # Every screen needs all four sheets present. The in-game screens look an
    # image up by name across them; the front end's CEGUI files name its sheet.
    sheets = {
        "widgets": hud_sprites(),
        "widgets1": screen_sprites(),
        "widgets2": ui_pack_menu.menu_sprites(),
        "ratings": {"pack_blank": Image.new("RGBA", (4, 4), (0, 0, 0, 0))},
    }
    for name, sprite in ui_pack_front.front_sprites().items():
        sheets[ui_pack_front.FIXED_SHEET.get(name, "widgets1")][name] = sprite
    # What the PC has the like of is drawn in the PC's art: not in the pack.
    for sprites in sheets.values():
        for name in [n for n in sprites if n.lower() in ui_pack_front.PC_HAS]:
            del sprites[name]
    # Some code looks an image up on one sheet only: move those there.
    for sheet, sprites in sheets.items():
        for name in [n for n in sprites if ui_pack_front.FIXED_SHEET.get(n, sheet) != sheet]:
            sheets[ui_pack_front.FIXED_SHEET[name]][name] = sprites.pop(name)
    sheet_of = {}
    for sheet, sprites in sheets.items():
        for name in sprites:
            if name.lower() in sheet_of:
                raise SystemExit(f"'{name}' is on both {sheet_of[name.lower()]} and {sheet}")
            sheet_of[name.lower()] = sheet
        write_sheet(args.out, sheet, sprites, size=16 if sheet == "ratings" else 2048)

    layouts = [("media/UI/hud_xenon.layout", hud_layout()),
               ("media/UI/pethud_xenon.layout", pethud_layout()),
               ("media/UI/resurrect_xenon.layout", death_layout()),
               ("media/UI/questdialogmenu_xenon.layout", quest_dialog_layout()),
               ("media/UI/tipmenu_xenon.layout", tip_layout())]
    layouts += [(path, build()) for path, build in ui_pack_menu.LAYOUTS]
    layouts += [(path, build()) for path, build in ui_pack_front.UI_LAYOUTS]
    for path, objects in layouts:
        with open(os.path.join(args.out, path), "w", encoding="utf-8", newline="\r\n") as fh:
            fh.write(render(objects))
    for path, build in ui_pack_front.CEGUI_LAYOUTS:
        ui_pack_front.write_cegui(os.path.join(args.out, path), build(), sheet_of)
    check_case(args.out)
    print(f"ui pack written to {args.out}")


if __name__ == "__main__":
    main()
