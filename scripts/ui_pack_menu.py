"""The master menu's part of the interface pack (`build_ui_pack.py`): the
sprites and the six layouts the menu cannot open without - the frame
(`mastermenu_xenon`), the character sheet, the bag, the stats page, the
journal and the quest log.

The menu builds these into fixed places of its own: three 301-wide columns at
0, 301 and 602 in a 902 x 630 container, the bag's selection frame at (-16, -18)
from its row, the quest rows 89 apart from -10, `LineFrame` corners fitted at
22 and the item tooltip's at 26. The layouts below are drawn to that grid;
every `WIDGET NAME` the console's files carry is kept, because the code builds
many of them from parts (`ItemTray` + n).
"""
from PIL import Image

from build_ui_pack import (BLUE, GOLD, GOLD_TEXT, INK, INK_DIM, PANEL, PANEL_DARK, RED, Canvas,
                           at, frame_pieces, full, header, obj, over, plate, socket)

GREEN = (92, 176, 72, 255)
TEAL = (64, 190, 200, 255)
PURPLE = (150, 92, 210, 255)
ORANGE = (230, 130, 40, 255)
YELLOW = (230, 210, 60, 255)
COL_BG = (16, 19, 25, 250)
COL_BG_LIT = (22, 27, 36, 252)
COL_BG_SHOP = (28, 22, 16, 250)
COL_BG_SHOP_LIT = (36, 29, 20, 252)
LABEL = "FFE6DCC4"
BRIGHT_GOLD = "FFF2B040"


# --------------------------------------------------------------------------
# Sprites. Panels and frames at 1x (they are big and flat), icons at 2x.
# --------------------------------------------------------------------------

def flat_tile(colour):
    return Image.new("RGBA", (8, 8), colour)


def outline_box(w, h, outline, width=1.4, radius=8, fill=None, inset=0.5):
    c = Canvas(w, h)
    c.rrect(inset, inset, w - inset, h - inset, radius, fill=fill, outline=outline, width=width)
    return c.done()


def surround(w, h, inner, outline=GOLD):
    """A selection outline drawn around `inner` = (x0, y0, x1, y1) of a larger
    transparent picture: the frame that moves onto the chosen row."""
    c = Canvas(w, h)
    x0, y0, x1, y1 = inner
    c.rrect(x0 - 3, y0 - 3, x1 + 3, y1 + 3, 10, outline=outline, width=2.4)
    return c.done()


def bar_line(w, h, where):
    """A thin rule along one edge: "top", "bottom", "left" or "right"."""
    c = Canvas(w, h)
    if where in ("top", "bottom"):
        y = 1.5 if where == "top" else h - 1.5
        c.line([(4, y), (w - 4, y)], fill=INK_DIM, width=1.2)
    else:
        x = 1.5 if where == "left" else w - 1.5
        c.line([(x, 4), (x, h - 4)], fill=INK_DIM, width=1.2)
    return c.done()


def chevron(w, h, up, colour=INK):
    c = Canvas(w, h)
    m = w / 2
    if up:
        c.line([(m - 9, h - 6), (m, 6), (m + 9, h - 6)], fill=colour, width=2.4)
    else:
        c.line([(m - 9, 6), (m, h - 6), (m + 9, 6)], fill=colour, width=2.4)
    return c.done()


def side_arrow(w, h, right, colour=GOLD):
    c = Canvas(w, h)
    if right:
        c.polygon([(4, 6), (w - 4, h / 2), (4, h - 6)], fill=colour)
    else:
        c.polygon([(w - 4, 6), (4, h / 2), (w - 4, h - 6)], fill=colour)
    return c.done()


def coin(n):
    c = Canvas(n, n)
    k = n / 26.0
    c.ellipse(5 * k, 8 * k, 21 * k, 22 * k, fill=(150, 112, 30, 255), outline=GOLD, width=1.4 * k)
    c.ellipse(5 * k, 3 * k, 21 * k, 17 * k, fill=GOLD, outline=(150, 112, 30, 255), width=1.4 * k)
    return c.done()


def triangle(w, h, up, colour):
    c = Canvas(w, h)
    if up:
        c.polygon([(w / 2, 2), (w - 2, h - 2), (2, h - 2)], fill=colour)
    else:
        c.polygon([(2, 2), (w - 2, 2), (w / 2, h - 2)], fill=colour)
    return c.done()


def glyph(c, kind, x0, y0, s, ink):
    """A small line icon in the square (x0, y0, s): the bag's filters, the
    stats and the resistances share these."""
    p = lambda u, v: (x0 + u * s, y0 + v * s)
    w = max(1.0, s * 0.08)
    if kind == "sword":
        c.line([p(0.2, 0.8), p(0.8, 0.2)], fill=ink, width=w)
        c.line([p(0.3, 0.55), p(0.45, 0.7)], fill=ink, width=w)
        c.line([p(0.12, 0.88), p(0.25, 0.75)], fill=ink, width=w)
    elif kind == "shield":
        c.polygon([p(0.5, 0.12), p(0.82, 0.24), p(0.76, 0.6), p(0.5, 0.88), p(0.24, 0.6),
                   p(0.18, 0.24)], outline=ink, width=w)
    elif kind == "ring":
        c.ellipse(*p(0.25, 0.38), *p(0.75, 0.88), outline=ink, width=w)
        c.polygon([p(0.5, 0.12), p(0.62, 0.3), p(0.5, 0.42), p(0.38, 0.3)], fill=ink)
    elif kind == "flask":
        c.line([p(0.42, 0.12), p(0.58, 0.12)], fill=ink, width=w)
        c.polygon([p(0.44, 0.15), p(0.56, 0.15), p(0.56, 0.4), p(0.8, 0.82), p(0.2, 0.82),
                   p(0.44, 0.4)], outline=ink, width=w)
    elif kind == "scroll":
        c.rrect(*p(0.25, 0.18), *p(0.75, 0.82), s * 0.06, outline=ink, width=w)
        for v in (0.35, 0.5, 0.65):
            c.line([p(0.35, v), p(0.65, v)], fill=ink, width=w * 0.7)
    elif kind == "fish":
        c.ellipse(*p(0.15, 0.32), *p(0.68, 0.68), outline=ink, width=w)
        c.polygon([p(0.66, 0.5), p(0.88, 0.3), p(0.88, 0.7)], outline=ink, width=w)
    elif kind == "gem":
        c.polygon([p(0.3, 0.3), p(0.7, 0.3), p(0.85, 0.45), p(0.5, 0.85), p(0.15, 0.45)],
                  outline=ink, width=w)
    elif kind == "bow":
        c.d.arc([c.s(*p(0.1, 0.12))[0], c.s(*p(0.1, 0.12))[1], c.s(*p(0.7, 0.88))[0],
                 c.s(*p(0.7, 0.88))[1]], start=-70, end=70, fill=ink, width=max(1, round(w * 4)))
        c.line([p(0.5, 0.18), p(0.5, 0.82)], fill=ink, width=w * 0.6)
        c.line([p(0.2, 0.5), p(0.9, 0.5)], fill=ink, width=w)
    elif kind == "crystal":
        c.polygon([p(0.5, 0.1), p(0.72, 0.4), p(0.5, 0.9), p(0.28, 0.4)], outline=ink, width=w)
        c.line([p(0.28, 0.4), p(0.72, 0.4)], fill=ink, width=w * 0.7)
    elif kind == "flame":
        c.polygon([p(0.5, 0.1), p(0.75, 0.5), p(0.68, 0.82), p(0.32, 0.82), p(0.25, 0.5)],
                  outline=ink, width=w)
    elif kind == "snow":
        for a, b in ((p(0.5, 0.12), p(0.5, 0.88)), (p(0.17, 0.3), p(0.83, 0.7)),
                     (p(0.17, 0.7), p(0.83, 0.3))):
            c.line([a, b], fill=ink, width=w)
    elif kind == "bolt":
        c.polygon([p(0.58, 0.08), p(0.3, 0.55), p(0.5, 0.55), p(0.42, 0.92), p(0.72, 0.42),
                   p(0.52, 0.42)], fill=ink)
    elif kind == "drop":
        c.polygon([p(0.5, 0.1), p(0.75, 0.55), p(0.65, 0.8), p(0.35, 0.8), p(0.25, 0.55)],
                  fill=ink)


def tab(w, h, kind, lit, colour=INK):
    c = Canvas(w, h)
    c.rrect(1.5, 1.5, w - 1.5, h - 1.5, 7, fill=(34, 40, 52, 235) if lit else PANEL_DARK,
            outline=GOLD if lit else INK_DIM, width=2 if lit else 1.4)
    glyph(c, kind, w * 0.18, h * 0.16, min(w, h) * 0.66, colour if lit else INK_DIM)
    return c.done()


def icon(n, kind, colour, ring=True):
    c = Canvas(n, n)
    if ring:
        c.ellipse(1.5, 1.5, n - 1.5, n - 1.5, fill=PANEL_DARK, outline=colour, width=1.6)
    glyph(c, kind, n * 0.18, n * 0.18, n * 0.64, colour)
    return c.done()


def rarity(w, h, colour):
    c = Canvas(w, h)
    fill = colour[:3] + (70,) if colour else (10, 12, 16, 200)
    c.rrect(1, 1, w - 1, h - 1, 5, fill=fill, outline=(colour or INK_DIM)[:3] + (200,), width=1.2)
    return c.done()


def sheet_with_window(w, h, hole):
    """The character sheet's panel: the menu draws the 3D model behind it, so
    the panel leaves `hole` = (x0, y0, x1, y1) open, with a faint rim."""
    c = Canvas(w, h)
    c.rrect(0.5, 0.5, w - 0.5, h - 0.5, 8, fill=COL_BG, outline=INK_DIM, width=1.4)
    x0, y0, x1, y1 = hole
    c.rrect(x0, y0, x1, y1, 10, fill=(0, 0, 0, 0))
    c.rrect(x0, y0, x1, y1, 10, outline=INK_DIM[:3] + (110,), width=1)
    return c.done()


def char_stats(w, h):
    """The character sheet's four attribute boxes, icons beside them."""
    c = Canvas(w, h)
    c.rrect(0.5, 0.5, w - 0.5, h - 0.5, 8, fill=PANEL, outline=INK_DIM, width=1.2)
    for (ix, iy), kind in zip(((8, 6), (138, 6), (8, 40), (138, 40)),
                              ("sword", "crystal", "bow", "shield")):
        glyph(c, kind, ix, iy, 26, INK)
        c.rrect(ix + 34, iy, ix + 94, iy + 24, 6, fill=PANEL_DARK, outline=INK_DIM, width=1)
    return c.done()


def start_button(w, h):
    c = Canvas(w, h)
    c.rrect(1, 2, w - 1, h - 2, 7, fill=PANEL_DARK, outline=INK, width=1.4)
    c.polygon([(w / 2 - 3, h / 2 - 4), (w / 2 + 4, h / 2), (w / 2 - 3, h / 2 + 4)], fill=INK)
    return c.done()


def dpad(n, lit="lr"):
    """The d-pad, arrows on the arms pushed: "lr" left and right, "" up and down."""
    c = Canvas(n, n)
    k = n / 24.0
    arm = [(9, 2), (15, 2), (15, 9), (22, 9), (22, 15), (15, 15), (15, 22), (9, 22), (9, 15),
           (2, 15), (2, 9), (9, 9)]
    c.polygon([(x * k, y * k) for x, y in arm], fill=PANEL_DARK, outline=INK_DIM, width=1.2 * k)
    if "l" in lit:
        c.polygon([(3 * k, 12 * k), (8 * k, 9.5 * k), (8 * k, 14.5 * k)], fill=INK)
        c.polygon([(21 * k, 12 * k), (16 * k, 9.5 * k), (16 * k, 14.5 * k)], fill=INK)
    else:
        c.polygon([(12 * k, 3 * k), (9.5 * k, 8 * k), (14.5 * k, 8 * k)], fill=INK)
        c.polygon([(12 * k, 21 * k), (9.5 * k, 16 * k), (14.5 * k, 16 * k)], fill=INK)
    return c.done()


def menu_sprites():
    r = 2
    out = {
        # Column backgrounds: flat, stretched. Frames over them carry the lines.
        "backroundbase_inventory": flat_tile(COL_BG),
        "backroundbase_inventory_rollover": flat_tile(COL_BG_LIT),
        "backroundbase_merchant": flat_tile(COL_BG_SHOP),
        "backroundbase_merchant_rollover": flat_tile(COL_BG_SHOP_LIT),
        "backroundbase_stats": flat_tile(COL_BG),
        "backroundbase_quests": flat_tile(COL_BG),
        "framea": outline_box(303, 629, INK_DIM),
        "framea_rollover": outline_box(303, 629, GOLD, width=2),
        # Open over `CharacterViewport` (16, 168, 270 x 410) less the rows drawn on it.
        "character_background": sheet_with_window(303, 629, (16, 202, 286, 462)),
        # The menu's own frame.
        "menu_frame_top": outline_box(920, 52, INK_DIM, fill=PANEL_DARK, radius=10),
        # A strip under the columns only, so the HUD's skill bar does not show through.
        "menu_frame_bottom": outline_box(920, 24, INK_DIM, fill=PANEL_DARK, radius=8),
        "menu_frame_left": bar_line(10, 600, "left"),
        "menu_frame_right": bar_line(8, 600, "right"),
        "menu_textwindow": plate(210 * r, 40 * r, outline=GOLD),
        "button_window": outline_box(50 * r, 44 * r, INK_DIM, fill=PANEL_DARK, radius=16),
        "decor_arrow_left": side_arrow(22 * r, 30 * r, False),
        "decor_arrow_right": side_arrow(22 * r, 30 * r, True),
        "start_button": start_button(28 * r, 22 * r),
        "dpad_leftright": dpad(24 * r),
        "panel_header": plate(291, 42, outline=INK_DIM),
        "submenu_tray_rollover": outline_box(291, 104, INK_DIM, fill=PANEL, radius=10),
        # Rows, and the frame that marks the chosen one.
        "item_tray": outline_box(273, 89, INK_DIM, fill=(20, 24, 32, 230)),
        "item_tray_equipped": outline_box(273, 89, (200, 170, 90, 255), fill=(34, 32, 26, 235)),
        "load_chartext_window_selected": outline_box(273, 89, INK_DIM, fill=(20, 24, 32, 230)),
        "load_chartext_window_rollover": outline_box(273, 89, GOLD, fill=(36, 33, 24, 240),
                                                     width=2),
        "facet": surround(314, 124, (16, 18, 289, 107)),
        "statwindow_facet": surround(314, 124, (13, 7, 301, 117)),
        "facet_panel_top": plate(169 * r, 36 * r, outline=GOLD),
        "textwindow360": plate(196 * r, 31 * r),
        "score_modifier": outline_box(150, 64, GOLD, fill=PANEL_DARK, radius=10),
        "statwindow_base": outline_box(288, 112, INK_DIM, fill=(20, 24, 32, 230), radius=10),
        "statwindow_base_rollover": outline_box(288, 112, GOLD, fill=(36, 33, 24, 240), radius=10,
                                                width=2),
        "resistance_score_window": outline_box(114, 35, INK_DIM, fill=PANEL_DARK, radius=8),
        "character_stats": char_stats(241, 70),
        "exp_fame_window": outline_box(263, 30, INK_DIM, fill=PANEL_DARK, radius=8),
        "frameb_top": bar_line(587, 6, "top"),
        "frameb_bottom": bar_line(587, 6, "bottom"),
        "frame_skill_top": bar_line(600, 38, "top"),
        "frame_skill_bottom": bar_line(600, 121, "bottom"),
        "frame_skill_left": bar_line(20, 470, "left"),
        "frame_skill_right": bar_line(19, 470, "right"),
        "menu_arrow_up": chevron(38 * r, 23 * r, True),
        "menu_arrow_down": chevron(38 * r, 23 * r, False),
        "arrow_transmute": chevron(58 * r, 25 * r, False, GOLD),
        "gold_icon": coin(26 * r),
        "comparrison_arrow_up": triangle(20 * r, 15 * r, True, GREEN),
        "comparrison_arrow_down": triangle(20 * r, 15 * r, False, RED),
        "socket_full": socket(16 * r),
        "tab_hilite": outline_box(33 * r, 38 * r, GOLD, radius=12, width=2.4),
        "icon_strength": icon(45 * r, "sword", INK),
        "icon_dexterity": icon(45 * r, "bow", INK),
        "icon_magic": icon(45 * r, "crystal", INK),
        "icon_defense": icon(45 * r, "shield", INK),
        "icon_fire": icon(27 * r, "flame", ORANGE, ring=False),
        "icon_ice": icon(27 * r, "snow", TEAL, ring=False),
        "icon_electricity": icon(27 * r, "bolt", YELLOW, ring=False),
        "icon_poison": icon(27 * r, "drop", GREEN, ring=False),
    }
    gem = socket(16 * r)
    out["socket_full"] = over(gem, triangle(16 * r, 16 * r, False, (120, 200, 255, 200)))
    for name, kind in (("weapons", "sword"), ("armor", "shield"), ("jewelry", "ring"),
                       ("potions", "flask"), ("spells", "scroll"), ("fish", "fish"),
                       ("gems", "gem")):
        out[f"tab_{name}"] = tab(43 * r, 46 * r, kind, False)
        out[f"tab_{name}_rollover"] = tab(43 * r, 46 * r, kind, True)
        out[f"merchant_tab_{name}"] = tab(43 * r, 46 * r, kind, False, GOLD)
        out[f"merchant_tab_{name}_rollover"] = tab(43 * r, 46 * r, kind, True, GOLD)
    for name, colour in (("blank", None), ("green", GREEN), ("blue", BLUE), ("purple", PURPLE),
                         ("red", RED), ("gold", GOLD), ("yellow", YELLOW),
                         ("orange", ORANGE)):
        out[f"traycolor_{name}"] = rarity(54, 80, colour)
    # The skills tab.
    out.update({
        "backroundbase_skillsa": flat_tile(COL_BG),
        "backroundbase_skillsb": flat_tile((24, 28, 38, 160)),
        "backroundbase_skillsc": flat_tile((20, 26, 40, 150)),
        "backroundbase_skillsd": flat_tile((30, 34, 46, 150)),
        "backroundbase_spells": flat_tile((26, 26, 30, 170)),
        "skills_window_active": outline_box(72 * r, 72 * r, INK_DIM, fill=PANEL_DARK, radius=14,
                                            width=2.4),
        "skills_window_active_rollover": outline_box(72 * r, 72 * r, GOLD, fill=(40, 36, 24, 240),
                                                     radius=14, width=3.6),
        "skills_window_passive": outline_box(72 * r, 72 * r, (130, 120, 220, 255),
                                             fill=PANEL_DARK, radius=36, width=2.4),
        "skills_window_passive_rollover": outline_box(72 * r, 72 * r, GOLD, fill=(40, 36, 24, 240),
                                                      radius=36, width=3.6),
        "skills_window_active_lg": outline_box(64 * r, 64 * r, GOLD, fill=PANEL_DARK, radius=12,
                                               width=2.4),
        "spells_windowb": outline_box(72 * r, 72 * r, INK_DIM, fill=PANEL_DARK, radius=36,
                                      width=2.4),
        "spells_windowb_rollover": outline_box(72 * r, 72 * r, GOLD, fill=(40, 36, 24, 240),
                                               radius=36, width=3.6),
        "level_tab": plate(64 * r, 38 * r, outline=INK_DIM),
        "dpad_updown": dpad(35 * r, ""),
        "skillmap_background": flat_tile((0, 0, 0, 0)),
        "stat_icon_strength": icon(32 * r, "sword", INK),
        "stat_icon_dexterity": icon(32 * r, "bow", INK),
        "stat_icon_magic": icon(32 * r, "crystal", INK),
        "stat_icon_defense": icon(32 * r, "shield", INK),
    })
    # The pet-command dialog's stances, and the replace-spell dialog's lit slot.
    for kind, glyph_kind in (("aggressive", "sword"), ("defensive", "shield"),
                             ("passive", "ring")):
        out[f"pet_{kind}"] = icon(50 * r, glyph_kind, GOLD)
        out[f"pet_{kind}_inactive"] = icon(50 * r, glyph_kind, INK_DIM)
    out["spells_window_rollover"] = outline_box(48 * r, 48 * r, GOLD, fill=(40, 36, 24, 240),
                                                radius=12, width=3.6)
    # The `LineFrame` look, fitted at 22: the PC has none like it. The item tooltips' two frames
    # (`itemtooltip*`, `itemtooltip2*`) are the PC's `GuiLook` ones, found by name.
    out.update(frame_pieces("frame", 22, fill=(10, 11, 16, 245), inner=INK_DIM))
    return out


# --------------------------------------------------------------------------
# Layouts.
# --------------------------------------------------------------------------

TOP = {"ALWAYS ON TOP": True}


def text(name, words="", widget=None, font=None, colour=None, align=None, valign=None,
         wrap=False, **placement):
    props = {}
    if widget:
        props["widget"] = widget
    if font:
        props["FONT"] = font
    if colour:
        props["COLOR"] = colour
    if align:
        props["HORIZONTAL ALIGN"] = align
    if valign:
        props["VERTICAL ALIGN"] = valign
    if wrap:
        props["WORD WRAP"] = True
    return obj("Text", name, TEXT=words, **props, **placement)


def image(name, picture=None, children=(), widget=None, **placement):
    props = {"widget": widget} if widget else {}
    if picture:
        props["IMAGE"] = picture
    return obj("Image", name, children, **props, **placement)


def window(name, children=(), widget=None, **placement):
    props = {"widget": widget} if widget else {}
    return obj("Window", name, children, **props, **placement)


def line_frame(name, children, widget=None, **placement):
    props = {"widget": widget} if widget else {}
    return obj("CEGUI Object", name, children, **props, **{"LOOK N FEEL": "ceguiWidget/LineFrame"},
               **placement)


def master_layout():
    def trigger(widget, button_widget, picture, arrow, x, right):
        return window(widget.lower(), [
            image("button", "button_window", [
                image("glyph", picture, widget=button_widget, **at(14, 6, 22, 32)),
            ], **at(4 if right else 28, 4, 50, 44)),
            image("arrow", arrow, **at(56 if right else 0, 11, 22, 30)),
        ], widget=widget, **at(x, 0, 80, 52))

    return [
        window("parent", [
            window("frame", [
                image("top bar", "menu_frame_top", [
                    trigger("lefttrigger", "lefttriggerbutton", "left_trigger", "decor_arrow_left",
                            60, False),
                    text("left", "INVENTORY", "MenuLeft", "Serif14", "FFC8B48C", "CENTER",
                         **at(165, 16, 170, 24)),
                    image("title", "menu_textwindow", [
                        text("name", "INVENTORY", "MenuName", "SerifBig", None, "CENTER",
                             **at(0, 8, 210, 26)),
                    ], **at(0, 6, 210, 40, ha="CENTER")),
                    text("right", "INVENTORY", "MenuRight", "Serif14", "FFC8B48C", "CENTER",
                         **at(585, 16, 170, 24)),
                    trigger("righttrigger", "righttriggerbutton", "right_trigger",
                            "decor_arrow_right", 780, True),
                ], **at(0, 0, 920, 52, ha="CENTER")),
                image("bottom bar", "menu_frame_bottom", [
                    line_frame("pet commands", [
                        image("glyph", "start_button", **at(-6, 1, 28, 22)),
                        text("words", "pet commands", font="SerifBig", **at(28, -1, 200, 26)),
                    ], widget="PetCommands", **at(40, -2, 230, 24)),
                    line_frame("compare", [
                        image("glyph", "dpad_leftright", **at(-8, 0, 24, 24)),
                        text("words", "item comparison: off", "itemcomparetext", "SerifBig",
                             **at(22, -1, 236, 26)),
                    ], widget="ToggleItemCompare", **at(330, -2, 256, 24)),
                    line_frame("close", [
                        image("glyph", "b_button", **at(46, 1, 21, 21)),
                        text("words", "Close", font="SerifBig", **at(74, -1, 120, 26)),
                    ], **at(660, -2, 200, 24)),
                ], **at(0, 6, 920, 24, "CENTER", "BOTTOM")),
                window("columns", widget="ContainerFrame", **at(0, 19, 902, 630, "CENTER",
                                                                 "CENTER")),
            ], **full()),
            image("left rule", "menu_frame_left", **at(0, 6, 10, 600, va="CENTER")),
            image("right rule", "menu_frame_right", **at(0, 6, 8, 600, "RIGHT", "CENTER")),
        ], **at(0, 0, 920, 690, "CENTER", "CENTER")),
    ]


def character_layout():
    return [
        image("sheet", "character_background", [
            window("name and title", [
                text("name", "Name", "characterName", "SerifBig", BRIGHT_GOLD, "CENTER",
                     **at(0, 0, 250, 25)),
                text("title", "", "CharacterTitle", None, "FFB4A88C", "CENTER",
                     **at(0, 28, 250, 17)),
            ], **at(26, 22, 250, 46)),
            window("experience and fame", [
                text("label", "Level", colour=GOLD_TEXT, align="CENTER", **at(0, -6, 263, 16)),
                image("experience box", "exp_fame_window", **at(0, 12, 263, 30)),
                image("fame box", "exp_fame_window", **at(0, 46, 263, 30)),
                text("level", "0", "Level", "Serif14", align="CENTER", **at(4, 16, 26, 22)),
                text("experience", "XP: 0", "XP", "Serif14", **at(44, 16, 200, 22)),
                text("fame level", "0", "FameLevel", "Serif14", align="CENTER",
                     **at(4, 50, 26, 22)),
                text("fame", "Fame: 0", "Fame", "Serif14", **at(44, 50, 200, 22)),
            ], **at(20, 84, 263, 80)),
            window("model", widget="CharacterViewport", **at(16, 168, 270, 410)),
            window("health and mana", [
                text("hp label", "HP", font="Serif14", colour=LABEL, **at(0, 6, 38, 24)),
                text("hp", "0", "HP", "Serif14", align="RIGHT", **at(30, 6, 60, 24)),
                text("mp label", "MP", font="Serif14", colour=LABEL, **at(140, 6, 38, 24)),
                text("mp", "0", "Mana", "Serif14", align="RIGHT", **at(164, 6, 60, 24)),
            ], **at(30, 170, 240, 26)),
            window("attributes", [
                text("label", "Stats", colour=GOLD_TEXT, align="CENTER", **at(50, 0, 165, 18)),
                image("boxes", "character_stats", **at(12, 22, 241, 70)),
                text("strength", "0", "strPts", "Serif14", align="CENTER", **at(54, 28, 60, 24)),
                text("magic", "0", "magicPts", "Serif14", align="CENTER", **at(184, 28, 60, 24)),
                text("dexterity", "0", "dexPts", "Serif14", align="CENTER", **at(54, 62, 60, 24)),
                text("defense", "0", "defPts", "Serif14", align="CENTER", **at(184, 62, 60, 24)),
            ], **at(18, 470, 265, 100)),
        ], widget="TopFrame", **at(0, 0, 303, 629)),
    ]


def bag_layout():
    def row(n, y):
        return image(f"row {n}", "item_tray", [
            image("rarity", "traycolor_green", [
                image("icon", widget=f"Slot{n}", **full()),
            ], widget=f"QualityBk{n}", **at(4, 4, 54, 80)),
            text("name", "Item", f"SlotText{n}", "SerifBig", **at(66, 14, 200, 23)),
            text("quality", "Quality", f"Quality{n}", "SerifSmall", align="RIGHT",
                 **at(-8, 5, 214, 19, ha="RIGHT")),
            text("equipped", "Equipped", f"Equipped{n}", align="RIGHT",
                 **at(-8, 4, 205, 19, ha="RIGHT")),
            image("compare", "comparrison_arrow_up", widget=f"qualityarrow{n}", **at(3, 6, 20, 15)),
            image("socket 0", "socket_empty", widget=f"socket0_{n}", **at(53, 21, 15, 16)),
            image("socket 1", "socket_empty", widget=f"socket1_{n}", **at(53, 43, 15, 16)),
            text("damage", "Damage and Armor", f"DamageArmor{n}", **at(78, 38, 179, 23)),
            text("enchantments", "Enchantments", f"Enchantments{n}", colour="FF40C8E8",
                 **at(78, 53, 179, 23)),
            text("requirement", "", f"Requirment{n}", "SerifSmall", "FFFF4040", valign="BOTTOM",
                 **at(-6, 61, 206, 19, ha="RIGHT")),
            image("price icon", "gold_icon", [
                text("price", "0", f"Gold{n}", align="RIGHT", valign="BOTTOM", colour="FFFFCC00",
                     **at(-25, 1, 163, 19, ha="RIGHT")),
            ], **at(0, -3, 26, 23, "RIGHT", "BOTTOM")),
        ], widget=f"ItemTray{n}", **at(0, y, 273, 89, ha="CENTER"))

    def button(widget, picture, words, x, w=15, h=15):
        return image(widget, picture, [
            text("words", words, f"{widget}text", colour="FFCCCACF", **at(15, 2, 120, 22,
                                                                          va="CENTER")),
        ], widget=widget, **at(x, 0, w, h, "CENTER", "CENTER"))

    tabs = [("weapons", "weaponHi"), ("armor", "armorHi"), ("jewelry", "trinketHi"),
            ("potions", "potionHi"), ("spells", "scrollHi"), ("fish", "fishHi")]
    return [
        image("bag", "backroundbase_inventory_rollover", [
            image("frame", "framea_rollover", [
                window("rows", [row(n, 92 * n) for n in range(5)] + [
                    image("selection", "facet", [
                        image("buttons", "facet_panel_bottom", [
                            button("buttonA", "a_button", "Unequip", -112),
                            button("buttonY", "y_button", "Drop", -29),
                            button("buttonX", "button_x", "Sell", 26),
                            button("buttonB", "dpad_leftright", "Info", 76, 16, 16),
                        ], widget="buttontray", **at(0, 100, 271, 36, ha="CENTER")),
                        image("buy or sell", "facet_panel_top", [
                            text("words", "SELL", "buyandselltext", "Serif14", "FFFF7040",
                                 "CENTER", **at(14, 6, 141, 26)),
                        ], widget="buyandsell", **at(0, -7, 169, 36, ha="CENTER")),
                        image("identify", "textwindow360", [
                            image("scroll", widget="IDENTIFYIMG", **at(12, 0, 16, 24)),
                            text("count", "x0", "IDENTIFYTEXT", "Serif14", valign="BOTTOM",
                                 **at(74, -7, 77, 18, va="BOTTOM")),
                            text("label", "Scrolls", colour="FF40E0F8", valign="BOTTOM",
                                 **at(28, -7, 109, 18, va="BOTTOM")),
                        ], widget="IDENTIFYWIN", **at(26, 133, 155, 31)),
                    ], widget="moveframe", **at(-16, -18, 314, 124), **TOP),
                ], widget="itemswindow", **at(1, 50, 282, 458, "CENTER", "CENTER")),
                text("size", "0/0", "inventorysize", "Serif14", align="CENTER",
                     **at(24, 596, 94, 26)),
                image("lb", "left_bumper", widget="leftbumper", VISIBLE=False,
                      **at(23, 86, 40, 24)),
                image("rb", "right_bumper", widget="rightbumper", VISIBLE=False,
                      **at(240, 86, 40, 24)),
                image("down", "menu_arrow_down", widget="downarrow", **at(0, 595, 38, 23,
                                                                          ha="CENTER")),
                window("pet in town", [
                    text("words", "Your pet has gone to town.", font="SerifBig", align="CENTER",
                         wrap=True, **at(0, 0, 250, 167, "CENTER", "CENTER")),
                    text("time", "0:00:00", "petreturntime2", "SerifHuge", align="CENTER",
                         valign="CENTER", VISIBLE=False, **at(0, 0, 250, 190, "CENTER", "CENTER")),
                ], widget="petintown", **full()),
                image("header", "submenu_tray_rollover", [
                    text("title", "MY BAG", "characterName", "SerifBig", BRIGHT_GOLD, "CENTER",
                         **at(16, 5, 257, 25)),
                    text("filter", "Weapons", "filtername", "Serif14", GOLD_TEXT, "CENTER",
                         **at(79, 79, 130, 22)),
                    # The transmuter's line, and under it the "Transmute" prompt.
                    obj("Text", "transmuter", [
                        obj("Text", "transmute", [
                            image("arrow", "arrow_transmute", **at(0, -27, 58, 25, ha="CENTER")),
                            image("glyph", "y_button", **at(0, 24, 21, 21, ha="CENTER")),
                        ], TEXT="Transmute", FONT="SerifBig",
                            **{"HORIZONTAL ALIGN": "CENTER", "WORD WRAP": True},
                            **at(0, 497, 268, 77)),
                    ], widget="transmutor",
                        TEXT="Swap items to the Transmuter to create new items.",
                        COLOR="FFE1CE1E", **{"HORIZONTAL ALIGN": "CENTER", "WORD WRAP": True},
                        **at(0, 31, 271, 40, ha="CENTER")),
                    window("tabs", [
                        image(name, f"tab_{name}_rollover" if i == 0 else f"tab_{name}",
                              widget=f"inventory_filter_{name}", **at(45 * i, 0, 43, 46))
                        for i, (name, _) in enumerate(tabs)
                    ] + [
                        window("highlights", [
                            image(hi, "tab_hilite", widget=hi, **at(5 + 45 * i, 4, 33, 38))
                            for i, (_, hi) in enumerate(tabs)
                        ], **full()),
                    ], widget="tabswindow", **at(0, 1, 269, 46, "CENTER", "CENTER")),
                ], **at(0, 7, 291, 104, ha="CENTER")),
                window("gold", [
                    text("amount", "0", "playergoldtext", "Serif14", "FFEEC111",
                         **at(31, 8, 94, 26)),
                    image("coin", "gold_icon", **at(-208, -11, 26, 23, "RIGHT", "BOTTOM")),
                ], widget="playergoldwin", **at(177, 586, 236, 44)),
                image("up", "menu_arrow_up", widget="uparrow", **at(0, 112, 38, 23, ha="CENTER")),
            ], widget="HiliteFrame", **at(0, 0, 303, 629, "CENTER", "CENTER")),
        ], widget="TopFrame", **at(0, 0, 301, 629)),
    ]


def stats_layout():
    def help_box(widget, y, words):
        return line_frame(widget.lower(), [
            text("words", words, font="Serif14", valign="CENTER", wrap=True, **full()),
        ], widget=widget, **at(326, y, 250, 48))

    def stat(widget, label, pts, added, value_label, value, y, picture):
        return image(widget, "statwindow_base", [
            text("label", label, align="CENTER", colour=LABEL, **at(16, 14, 100, 17)),
            image("icon", picture, **at(14, 38, 44, 46)),
            text("points", "0", pts, "SerifBig", align="CENTER", **at(64, 48, 55, 27)),
            text("added", "+0", added, align="RIGHT", **at(64, 70, 55, 17)),
            text("value label", value_label, align="CENTER", colour=LABEL,
                 **at(166, 14, 100, 17)),
            text("value", "0-0", value, "SerifBig", align="CENTER", **at(161, 48, 112, 27)),
        ], widget=widget, **at(0, y, 288, 110, ha="CENTER"))

    def resistance(name, widget, picture, x, y):
        return image(name, "resistance_score_window", [
            image("icon", picture, **at(5, 5, 27, 24)),
            text("points", "0", widget, "Serif14", align="CENTER", **at(43, 4, 61, 23)),
        ], **at(x, y, 114, 35, va="BOTTOM"))

    def point_button(widget, picture, words, y):
        return window(widget, [
            image("glyph", picture, **at(0, 0, 21, 21)),
            text("words", words, **at(25, 0, 115, 22)),
        ], widget=widget, **at(4, y, 140, 24))

    return [
        help_box("StrHelp", 140, "Strength improves damage dealt with melee weapons."),
        help_box("DexHelp", 258, "Dexterity improves damage dealt with ranged weapons."),
        help_box("MagicHelp", 368, "Magic improves damage dealt with magic weapons."),
        help_box("DefHelp", 486, "Defense improves the damage absorption of your armor."),
        image("page", "backroundbase_stats", [
            image("frame", "framea_rollover", [
                image("header", "panel_header", [
                    text("title", "Stats", font="SerifBig", colour=BRIGHT_GOLD, align="CENTER",
                         **at(0, 9, 247, 26, ha="CENTER")),
                ], **at(0, 7, 291, 42, ha="CENTER")),
                stat("strength", "Strength", "strPts", "strPtsAdded", "Damage", "strDMG", 48,
                     "icon_strength"),
                stat("dexterity", "Dexterity", "dexPts", "dexPtsAdded", "Damage", "dexDMG", 160,
                     "icon_dexterity"),
                stat("magic", "Magic", "magicPts", "magicPtsAdded", "Damage", "magicDMG", 272,
                     "icon_magic"),
                stat("defense", "Defense", "defPts", "defPtsAdded", "Armor", "defArmor", 384,
                     "icon_defense"),
                image("selection", "statwindow_facet", [
                    image("spend", "score_modifier", [
                        point_button("addPoint", "a_button", "add point", 10),
                        point_button("removePoint", "y_button", "remove point", 34),
                    ], widget="spendPoints", **at(312, 30, 150, 64)),
                ], widget="pieceToMove", **at(-6, 41, 314, 124)),
                image("points left", "textwindow360", [
                    text("label", "Points Remaining", **at(10, 5, 150, 20)),
                    text("points", "0", "ptsToSpend", "Serif14", align="RIGHT",
                         **at(106, 2, 80, 23)),
                ], **at(0, -100, 196, 30, "CENTER", "BOTTOM")),
                text("resistances", "Resistances", font="Serif14", colour=GOLD_TEXT,
                     align="CENTER", **at(73, 529, 156, 21)),
                resistance("poison", "poisonRes", "icon_poison", 34, -44),
                resistance("fire", "fireRes", "icon_fire", 155, -44),
                resistance("ice", "iceRes", "icon_ice", 34, -10),
                resistance("electric", "eletricRes", "icon_electricity", 155, -10),
            ], **at(0, 0, 303, 629, va="CENTER")),
        ], **at(0, 0, 301, 629)),
    ]


def journal_layout():
    return [
        image("page", "backroundbase_stats", [
            image("frame", "framea", [
                image("header", "panel_header", [
                    text("title", "Journal", font="SerifBig", colour=BRIGHT_GOLD, align="CENTER",
                         **at(0, 9, 247, 26, ha="CENTER")),
                ], **at(0, 7, 291, 42, ha="CENTER")),
                window("rows", widget="journalwindow", **at(0, 57, 277, 552, ha="CENTER")),
            ], **at(0, 0, 303, 629, va="CENTER")),
        ], **at(0, 0, 301, 629)),
    ]


def quest_layout():
    def row(n):
        return image(f"row {n}", "load_chartext_window_rollover" if n == 0
                     else "load_chartext_window_selected", [
            text("title", "Quest", f"SlotText{n}", "SerifBig", align="CENTER", wrap=True,
                 **at(0, 16, 0, 60), **{"SCALE WIDTH": 1}),
            text("complete", "Quest Complete", f"questcomplete{n}", colour="FFFFFF40",
                 align="CENTER", valign="BOTTOM", **at(0, 0, 0, -6),
                 **{"SCALE WIDTH": 1, "SCALE HEIGHT": 1}),
        ], widget=f"ItemTray{n}", **at(0, -10 + 89 * n, 273, 89, ha="CENTER"))

    def reward(widget, x, y):
        return image(widget.lower(), "facet_panel_bottom", [
            obj("Text Outlined", "figure", widget=widget, TEXT="0", FONT="SerifBig",
                **{"HORIZONTAL ALIGN": "CENTER"}, **at(0, 3, 160, 24)),
        ], widget=f"{widget}Box", **at(x, y, 160, 32, ha="CENTER"))

    def item(n, x):
        return image(f"item {n}", "item_transmute_window", [
            image("slot", "item_window_small", widget=f"ItemSlot{n}", **at(9, 8, 36, 58)),
            image("socket 0", "socket_empty", widget=f"EmptySocket{n}_0", **at(4, 14, 14, 14),
                  **TOP),
            image("socket 1", "socket_empty", widget=f"EmptySocket{n}_1", **at(4, 30, 14, 14),
                  **TOP),
        ], widget=f"ItemBkg{n}", **at(x, 11, 54, 76))

    return [
        image("log", "backroundbase_quests", [
            image("list frame", "framea", [
                image("header", "menu_textwindow", [
                    text("title", "Quests", font="SerifBig", colour="FFFFCA04", align="CENTER",
                         valign="CENTER", **at(0, 0, 0, -10, "CENTER", "CENTER"),
                         **{"SCALE WIDTH": 1, "SCALE HEIGHT": 1}),
                ], **at(4, 6, 186, 41, ha="CENTER")),
            ], **at(600, 0, 300, 629)),
            window("list", [row(n) for n in range(6)] + [
                image("selection", "facet", [
                    image("buttons", "facet_panel_bottom", [
                        image("abandon", "y_button", [
                            text("words", "Abandon Quest", "buttonBtext", colour="FFCCCACF",
                                 **at(19, 2, 218, 22, va="CENTER")),
                        ], widget="buttonB", **at(-111, 0, 16, 16, "CENTER", "CENTER")),
                    ], widget="buttontray", **at(0, 100, 271, 36, ha="CENTER")),
                ], widget="moveframe", **at(-16, -26, 314, 124), **TOP),
            ], **at(607, 59, 282, 533)),
            window("title", [
                image("bar", "black", [
                    text("title", "", "questtitle", "SerifBig", "FFFFCA04", "CENTER", "CENTER",
                         **full()),
                    image("top rule", "frameb_top", **at(0, 0, 587, 6)),
                    image("bottom rule", "frameb_bottom", **at(0, 40, 587, 6)),
                ], **at(8, 6, 587, 46)),
            ], widget="questdescription", **at(0, 0, 600, 629)),
            window("details", [
                text("description", "", "questdetails", "Serif14", wrap=True,
                     **at(21, 64, 560, 448)),
                window("rewards", [
                    text("label", "Rewards", font="Serif14", colour="FFBFBFBF", align="CENTER",
                         **at(0, 6, -175, 40), **{"SCALE WIDTH": 1}),
                    reward("RewardGold", -190, 26),
                    reward("RewardXP", -190, 58),
                    reward("RewardFame", -10, 26),
                    item(1, 407),
                    item(2, 461),
                    item(3, 515),
                    image("highlight", "item_window_rollover", widget="ItemHighlight",
                          **at(407, 11, 54, 76), **TOP),
                ], widget="rewards", **at(2, 526, 595, 102)),
            ], widget="questdetailswin", **at(0, 0, 603, 627)),
            window("rules", [
                image("right", "frame_skill_right", **at(0, -41, 19, 470, "RIGHT", "CENTER")),
                image("top", "frame_skill_top", **at(0, 0, 600, 38)),
                image("left", "frame_skill_left", **at(0, -41, 20, 470, va="CENTER")),
                image("bottom", "frame_skill_bottom", **at(0, 0, 600, 121, va="BOTTOM")),
            ], **at(0, 0, 600, 629)),
        ], widget="TopFrame", **at(0, 0, 901, 629)),
        window("abandon", [
            image("dim", "black", ALPHA=0.5, **full()),
            image("holder", children=[
                obj("CEGUI Object", "panel", [
                    text("question", "Are you sure you want to abandon this quest?",
                         font="SerifHuge", align="CENTER", wrap=True,
                         **at(0, 37, 0, 114, ha="CENTER"), **{"SCALE WIDTH": 1}),
                    window("buttons", [
                        image("yes", "a_button", [
                            text("words", "Abandon Quest", font="SerifBig",
                                 **at(25, -1, 170, 25)),
                        ], **at(0, 0, 21, 21)),
                        image("no", "b_button", [
                            text("words", "Cancel", font="SerifBig", **at(25, -1, 170, 25)),
                        ], **at(-80, 0, 21, 21, ha="RIGHT")),
                    ], **at(0, 0, 0, 73, "CENTER", "BOTTOM"), **{"SCALE WIDTH": 0.4}),
                ], **{"LOOK N FEEL": "ceguiWidget/ItemText"}, **at(0, 0, 0, -21),
                    **{"SCALE WIDTH": 1, "SCALE HEIGHT": 1}),
            ], **at(0, -65, 0, 250, "CENTER", "CENTER"), **{"SCALE WIDTH": 0.9}),
        ], widget="QuestAbandonConfirm", VISIBLE=False, **at(0, 0, 400, 300, "CENTER", "CENTER"),
            **{"SCALE WIDTH": 1, "SCALE HEIGHT": 1}),
    ]


def skill_bank(widget, slot_prefix, slots, label, y=0, visible=True, ha=None, label_y=10):
    """The four mapped slots of a skill set, LT Y B RT: `slots` are their
    numbers in that order, the windows `<slot_prefix><n>`."""
    glyphs = [("left_trigger", 24, 34), ("y_button", 21, 21), ("b_button", 21, 21),
              ("right_trigger", 24, 34)]
    cells = []
    for (picture, gw, gh), slot, x in zip(glyphs, slots, (60, 140, 220, 300)):
        cells.append(image(f"slot {slot}", "spells_window", [
            window("icon", widget=f"{slot_prefix}{slot}", **at(0, 0, 36, 36, "CENTER", "CENTER")),
            image("glyph", picture, **at(0, -gh + 8, gw, gh, ha="CENTER"), **TOP),
        ], **at(x, 46, 48, 48)))
    props = {} if visible else {"VISIBLE": False}
    return window(widget.lower(), [
        text("label", label, font="Serif14", colour=GOLD_TEXT, align="CENTER",
             **at(40, label_y, 310, 22)),
    ] + cells, widget=widget, **props, **at(0, y, 400, 104, ha=ha))


def skills_layout():
    def grid_cell(pane, row, column):
        return image(f"cell {row} {column}", "skills_window_active", [
            image("icon", widget=f"skillicon{pane}_{row}_{column}", **at(9, 8, 52, 52)),
            text("level", "0", f"skilllvl{pane}_{row}_{column}", align="RIGHT",
                 **at(26, 46, 36, 20), **TOP),
        ], widget=f"skillbg{pane}_{row}_{column}", **at(18 + 88 * column, 26 + 70 * row, 70, 70))

    def pane(n, x):
        return window(f"pane {n}", [
            text("name", "SKILL TREE", f"PanelName{n}", align="CENTER", colour=LABEL,
                 **at(14, 2, 166, 17)),
        ] + [grid_cell(n, row, column) for row in range(6) for column in range(2)],
            widget="SkillPanel0", **at(x, 10, 194, 512))

    def tier(n, level, y):
        return image(f"tier {n}", "level_tab", [
            text("label", "level", colour=GOLD_TEXT, **at(8, 2, 40, 17)),
            text("number", str(level), font="Serif14", colour=BRIGHT_GOLD, align="CENTER",
                 **at(8, 12, 36, 22)),
        ], **at(-3, y, 64, 38))

    def spell(n, x):
        return image(f"spell {n}", "spells_windowb", [
            image("icon", widget=f"spellIcon{n}", **at(10, 9, 52, 52)),
        ], widget=f"spellbg{n}", **at(x, 4, 72, 72, va="CENTER"))

    def bonus(widget, picture, x):
        return image(widget.lower(), picture, [
            text("value", "0%", f"{widget}Val", colour=BRIGHT_GOLD, valign="CENTER",
                 **at(-3, 26, 44, 28)),
        ], widget=widget, **at(x, -24, 32, 32, va="BOTTOM"))

    def legend(widget, picture, words, y, text_widget=None, visible=True):
        props = {} if visible else {"VISIBLE": False}
        return image(widget.lower(), picture, [
            text("words", words, text_widget, "Serif14", "FFD8C090", **at(26, 0, 140, 24)),
        ], widget=widget, **props, **at(-8, y, 21, 21))

    help_dialog = image("help", "skillmap_background", [
        line_frame("frame", [
            window("buttons", [
                legend("ALearnSkill", "a_button", "learn skill", 4, "ALearnSkillText", False),
                legend("XMapSKill", "button_x", "map skill", 28),
                legend("ALevelUp", "a_button", "upgrade skill", 4, "ALearnSkillText"),
                obj("Text", "points label", [
                    text("points", "0", "SkillPts", "Serif14", align="CENTER",
                         **at(-157, 0, -100, 26, "RIGHT", "BOTTOM"), **{"SCALE WIDTH": 1}),
                ], TEXT="skill points", FONT="Serif14", COLOR="FFD8C090",
                    **at(16, 3, 9, 26, va="BOTTOM"), **{"SCALE WIDTH": 1}),
            ], widget="HideButtons", **at(-2, 0, 19, 0), **{"SCALE WIDTH": 1, "SCALE HEIGHT": 1}),
            text("too low", "You are not high enough level", "HighLevel", "Serif14",
                 align="CENTER", valign="CENTER", wrap=True, VISIBLE=False, **full()),
        ], **at(0, 0, 14, 0), **{"SCALE WIDTH": 1, "SCALE HEIGHT": 1}),
    ], widget="HelpSkillDialog", **at(-115, 31, 145, 74))

    description = window("description", [
        image("icon frame", "skills_window_active_lg", [
            window("icon", [
                image("picture", widget="CurrentActiveSkillIconImage", **full()),
            ], widget="CurrentActiveSkillIcon", **at(5, 5, 54, 54)),
        ], widget="CurrentActiveSkillFrame", **at(0, 0, 64, 64)),
        text("name", "", "SkillNameText", "SerifBig", "FFEAAA15", wrap=True,
             **at(72, 10, 200, 52)),
        obj("Text", "rank", [
            text("value", "0/0", "RankTextVal", **at(46, 0, 44, 22)),
        ], widget="RankText", TEXT="Rank", **at(72, -4, 80, 21)),
        text("passive", "Always Enabled", "PassiveDescText", colour="FF9C8CFF",
             **at(6, 64, 120, 21)),
        text("type", "Class Skill", "TypeDescText", colour="FFBFC7FF", align="RIGHT",
             **at(100, 64, 180, 21)),
        window("text box", [
            text("text", "", "SkillDescriptionText", colour="FFF5DB94", wrap=True,
                 **at(0, 0, 265, 500)),
        ], widget="SkillTextFrame", **at(2, 90, 265, 210)),
    ], **at(10, 40, 290, 586))

    next_level = image("next level", "backroundbase_skillsb", [
        obj("Text", "header", [
            obj("Text", "requires", [
                text("value", "0", "RequiredLevelVal", colour="FFEAAA15", align="RIGHT",
                     **at(112, 0, 30, 22)),
            ], TEXT="Requires Level", COLOR="FFF0A040", **at(118, 0, 160, 20)),
        ], widget="NextLevelHeader", TEXT="Next Level", COLOR="FFF2B040", **at(4, 2, 120, 20)),
        window("next text", [
            text("text", "", "SkillDescriptionTextNext", colour="FFA89060", wrap=True,
                 **at(0, 0, 265, 500)),
        ], widget="SkillTextFrameNext", **at(4, 26, 265, 180)),
        text("bonuses", "Bonuses from stat investment", "skillBonuses", colour="FFE6D11A",
             align="CENTER", **at(0, -48, 0, 25, "CENTER", "BOTTOM"), **{"SCALE WIDTH": 1}),
        bonus("SkillTypeStr", "stat_icon_strength", 28),
        bonus("SkillTypeDex", "stat_icon_dexterity", 94),
        bonus("SkillTypeMag", "stat_icon_magic", 160),
        bonus("SkillTypeDef", "stat_icon_defense", 226),
    ], **at(7, -2, 287, 280, va="BOTTOM"))

    stripes = window("stripes", [
        image("first pane", "backroundbase_skillsc", **at(0, 0, 195, 420)),
        image("third pane", "backroundbase_skillsc", **at(390, 0, 195, 420)),
        image("spell row", "backroundbase_spells", **at(0, -1, 585, 68, "CENTER", "BOTTOM")),
    ] + [image(f"band {y}", "backroundbase_skillsd", **at(0, y, 585, 70))
         for y in (70, 210, 350)], **at(8, 34, 585, 490))

    return [
        window("tab", [
            image("background", "backroundbase_skillsa", [
                window("panes frame", [
                    image("left", "frame_skill_left", **at(0, -41, 20, 470, va="CENTER")),
                    image("right", "frame_skill_right", **at(0, -41, 19, 470, "RIGHT",
                                                                 "CENTER")),
                    image("top", "frame_skill_top", **at(0, 0, 600, 38)),
                    image("bottom", "frame_skill_bottom", **at(0, 0, 600, 121, va="BOTTOM")),
                    window("panes", [
                        stripes,
                        pane(0, 8),
                        pane(1, 203),
                        pane(2, 398),
                        help_dialog,
                        window("tiers", [tier(n + 1, level, y) for n, (level, y) in enumerate(
                            ((1, 4), (5, 74), (10, 144), (15, 214), (20, 284), (25, 354)))],
                            **at(-23, 47, 62, 391)),
                        window("spells", [
                            text("label", "Spells", font="SerifBig", colour="FFF07020",
                                 align="CENTER", **at(24, 28, 70, 26)),
                        ] + [spell(n, x) for n, x in enumerate((108, 210, 312, 414))],
                            **at(0, 448, 583, 75, ha="CENTER")),
                        window("equipped", [
                            skill_bank("SkillSet1", "SkillSlot", (6, 4, 5, 7), "-Skill Set 1-"),
                            skill_bank("SkillSet2", "SkillSlot", (9, 3, 8, 10), "-Skill Set 2-",
                                       visible=False),
                            text("sets", "skill sets", font="SerifBig", colour="FFD6C760",
                                 align="CENTER", valign="CENTER", wrap=True,
                                 **at(437, 30, 152, 62)),
                            image("dpad", "dpad_updown", **at(400, 44, 35, 35)),
                        ], **at(0, 0, 598, 105, va="BOTTOM")),
                    ], **at(0, 0, 600, 629)),
                ], **at(300, 0, 600, 629)),
                image("description frame", "framea", [
                    image("points", "textwindow360", [
                        obj("Text", "label", [
                            text("value", "0", "skillPointsValue", align="CENTER",
                                 **at(154, 0, 45, 20)),
                        ], widget="skillPointsText", TEXT="Available Skill Points:",
                            **at(12, 5, 202, 20)),
                    ], **at(36, 8, 223, 31)),
                    description,
                ], **at(0, 0, 300, 629)),
                next_level,
            ], **at(0, 0, 901, 629)),
        ], widget="TopFrame", **full()),
    ]


def skill_map_layout():
    return [
        image("sheet", "skillmap_background", [
            obj("CEGUI Object", "dim panel", [
                line_frame("frame", [
                    obj("Text", "close", [
                        image("glyph", "button_x", **at(-26, 0, 21, 21)),
                    ], TEXT="Close", FONT="Serif14", COLOR="FFD9A426",
                        **at(7, -2, 80, 24, "RIGHT", "BOTTOM")),
                    text("prompt", "Press Button to Assign Skill", font="Serif14", align="CENTER",
                         **at(0, 52, 0, 28), **{"SCALE WIDTH": 1}),
                    window("equipped", [
                        skill_bank("MapSkillSet1", "MapSkillSlot", (6, 4, 5, 7), "-Skill Set 1-",
                                   ha="CENTER", label_y=100),
                        skill_bank("MapSkillSet2", "MapSkillSlot", (9, 3, 8, 10),
                                   "-Skill Set 2-", visible=False, ha="CENTER", label_y=100),
                        obj("Text", "change set", [
                            image("dpad", "dpad_updown", **at(176, 28, 35, 35)),
                        ], TEXT="change skill set", FONT="SerifBig", COLOR="FFC2AB60",
                            **{"HORIZONTAL ALIGN": "CENTER", "WORD WRAP": True},
                            **at(0, 128, 386, 58, ha="CENTER")),
                    ], **at(0, -132, 0, 105, va="BOTTOM"), **{"SCALE WIDTH": 1}),
                    image("header", "facet_panel_bottom", [
                        text("title", "Map Skill", font="Serif14", align="CENTER",
                             **at(14, 6, -28, 23), **{"SCALE WIDTH": 1}),
                    ], **at(0, -17, 271, 36, ha="CENTER")),
                ], **at(1, -72, 602, 276, "CENTER", "CENTER")),
            ], **{"LOOK N FEEL": "ceguiWidget/ItemText"}, **full()),
        ], widget="MapSkillDialog", **full()),
    ]


def enchanter_layout():
    """The enchanter family's panes, in the vendor's column: one per service,
    the code shows the one open."""
    def header(title_widget, words, backdrop):
        return image("header", "submenu_tray_rollover", [
            image("band", backdrop, **at(0, 76, 291, 28)),
            text("title", words, title_widget, "SerifBig", BRIGHT_GOLD, "CENTER",
                 **at(0, 10, 235, 37, ha="CENTER")),
        ], **at(0, 7, 291, 104, ha="CENTER"))

    def action(name, words, widget=None):
        return obj("Text", name, [
            image("glyph", "a_button", **at(0, 30, 21, 21, ha="CENTER")),
        ], **({"widget": widget} if widget else {}), TEXT=words, FONT="SerifBig",
            **{"HORIZONTAL ALIGN": "CENTER", "WORD WRAP": True}, **at(0, 548, 270, 77, ha="CENTER"))

    def result(widget):
        return text("result", "", widget, "SerifBig", "FFFFFF40", "CENTER", wrap=True,
                    **at(0, 174, 259, 159, "CENTER", "CENTER"))

    def note(words, y, h, colour="FFBABABA", font="SerifBig"):
        return text("note", words, font=font, colour=colour, align="CENTER", wrap=True,
                    **at(0, y, -16, h, "CENTER", "CENTER"), **{"SCALE WIDTH": 1})

    hidden = {"VISIBLE": False}
    return [
        window("panes", [
            image("remove gems", "backroundbase_quests", [
                header("title", "Destroy Gems", "backroundbase_quests"),
                result("getsocketresult"),
                note("Select the item you wish to remove the gems from.\n\nNote - Removing the "
                     "gems will preserve the weapon or armor, but destroy the gems.", -30, 204),
                action("press", "Destroy Gems", "pressbuttongetsocket"),
            ], widget="getsocket", **full()),
            image("enchant", "backroundbase_inventory", [
                header("title", "Enchanter", "backroundbase_inventory"),
                text("ask", "Select the item you wish to enchant.", colour="FFBEBEBA",
                     align="CENTER", wrap=True, **at(0, 96, 270, 40, ha="CENTER")),
                window("shrine", [
                    text("words", "You found an enchant shrine. Select a single item to enchant.",
                         font="SerifBig", colour="FFD1D5D5", align="CENTER", wrap=True,
                         **at(0, 22, 259, 175, "CENTER", "CENTER")),
                ], widget="enchantshrine", **at(0, 0, 286, 250, "CENTER", "CENTER")),
                window("price", [
                    text("label", "The price of the enchantment is", font="SerifBig",
                         colour="FFAFB5B1", align="CENTER", wrap=True,
                         **at(0, 22, 259, 175, "CENTER", "CENTER")),
                    text("price", "", "priceofenchantment", "SerifHuge", "FFF7DF09", "CENTER",
                         wrap=True, **at(0, 126, 282, 208, ha="CENTER")),
                    text("chance", "", "disenchantChance", "SerifBig", "FFD1D5D5", "CENTER",
                         wrap=True, **at(0, 126, 259, 75, "CENTER", "CENTER")),
                ], widget="sellprice", **at(0, -106, 286, 250, "CENTER", "CENTER")),
                text("result", "", "resultofenchant", "SerifBig", "FFFFFF40", "CENTER",
                     wrap=True, **at(0, 115, 259, 159, "CENTER", "CENTER")),
                action("press", "Enchant", "pressbutton"),
            ], widget="enchant", **hidden, **full()),
            image("recover gems", "backroundbase_merchant", [
                header(None, "Recover Gems", "backroundbase_merchant"),
                result("destroysocketresult"),
                note("Select the item you wish to recover the gems from.\n\nNote - Removing the "
                     "gems will preserve them, but the weapon or armor will be destroyed.", -30,
                     220),
                action("press", "Recover Gems", "pressbuttondestroysocket"),
            ], widget="destroysocket", **hidden, **full()),
            image("retire", "backroundbase_inventory", [
                header("title", "Retirement", "backroundbase_inventory"),
                text("ask", "Select the item you wish to pass to your descendant. The item will "
                     "receive new bonuses as a result. Only weapons, armor, and trinkets may be "
                     "heirloomed.", font="Serif14", colour="FFBEBEBA", align="CENTER", wrap=True,
                     **at(0, 116, 270, 188, ha="CENTER")),
                window("warning", [
                    text("words", "WARNING!\nOnce a character is retired it may never be played "
                         "again!", font="SerifBig", colour="FFFF5040", align="CENTER", wrap=True,
                         **at(0, 81, 259, 175, "CENTER", "CENTER")),
                ], **at(0, 0, 286, 250, "CENTER", "CENTER")),
                action("press", "Retire"),
            ], widget="retire", **hidden, **full()),
        ], **at(0, 0, 303, 630)),
    ]


def housing_layout():
    """The dim screen the item-select, pet-command and replace-spell dialogs
    open in; they go into `parent`."""
    return [
        window("screen", [
            image("dim", "black", ALPHA=0.7, **at(-1, -1, 2, 2, "CENTER", "CENTER"),
                  **{"SCALE WIDTH": 1, "SCALE HEIGHT": 1}),
            window("dialog", [
                line_frame("close", [
                    image("glyph", "b_button", [
                        text("words", "Close", font="Serif14", **at(25, 0, 124, 21)),
                    ], **at(-29, 3, 21, 21, ha="CENTER")),
                ], widget="closebutton", **at(0, 0, 192, 27)),
            ], widget="parent", **full()),
        ], **at(-1, -1, 2, 2), **{"SCALE WIDTH": 1, "SCALE HEIGHT": 1}),
    ]


def pet_commands_layout():
    def stance(widget, picture, x):
        return image(widget.lower(), picture, widget=widget, **at(x, 0, 50, 50))

    return [
        obj("CEGUI Object", "panel", [
            header("Title", "Pet Commands", 220),
            window("stances", [
                stance("PetAggressive", "pet_aggressive_inactive", 0),
                stance("PetDefensive", "pet_defensive_inactive", 54),
                stance("PetPassive", "pet_passive_inactive", 108),
            ], **at(0, 34, 158, 50, ha="CENTER")),
            image("stick left", "analog_button_left_lr", **at(-130, 44, 46, 26, ha="CENTER")),
            image("stick right", "analog_button_left_lr", **at(130, 44, 46, 26, ha="CENTER")),
            text("stance", "", "stancetext", "Serif14", "FFFFCC00", "CENTER",
                 **at(0, 92, 263, 24, ha="CENTER")),
            obj("Text", "send to town", [
                image("glyph", "y_button", **at(-28, 2, 21, 21)),
                text("words", "Have your pet sell all items in their inventory", font="Serif14",
                     colour="FFC8CACA", align="CENTER", wrap=True,
                     **at(-100, 28, 415, 40, ha="CENTER")),
            ], widget="sendtotown", TEXT="Send Pet To Town", FONT="SerifBig", COLOR="FFFFE600",
                **at(108, 128, 300, 26, ha="CENTER")),
            obj("Text", "close", [
                image("glyph", "b_button", **at(-24, 0, 21, 21)),
            ], TEXT="Close", FONT="Serif14", COLOR="FFBAC6BE", **at(200, -8, 100, 24,
                                                                    "CENTER", "BOTTOM")),
        ], widget="FrameB", **{"LOOK N FEEL": "ceguiWidget/ItemText"},
            **at(0, 162, 500, 262, ha="CENTER")),
    ]


def spell_remove_layout():
    def slot(n, x):
        return image(f"spell {n}", "spells_window", [
            image("icon", widget=f"spellimage{n}", **at(0, 0, 0, 0, "CENTER", "CENTER"),
                  **{"SCALE WIDTH": 0.65, "SCALE HEIGHT": 0.65}),
        ], widget=f"spell{n}", **at(x, 0, 66, 66, ha="CENTER"))

    return [
        window("dialog", [
            line_frame("frame", [
                header("Title", "Replace Spell", 200),
                window("spells", [slot(n, x) for n, x in enumerate((-105, -35, 35, 105))],
                       **at(0, 26, 0, 72), **{"SCALE WIDTH": 1}),
                obj("Text", "replace", [
                    image("glyph", "a_button", **at(-26, 2, 21, 21)),
                ], TEXT="Replace Spell", FONT="SerifBig", COLOR="FFFFE600",
                    **at(30, 104, 200, 26, ha="CENTER")),
                obj("Text", "cancel", [
                    image("glyph", "b_button", **at(-26, 0, 21, 21)),
                ], TEXT="Cancel", FONT="Serif14", COLOR="FFBAC6BE",
                    **at(200, -2, 100, 24, "CENTER", "BOTTOM")),
            ], **full()),
        ], **at(0, 171, 541, 178, ha="CENTER")),
    ]


def pet_page_layout():
    """The pet's page in the character sheet's column: name, health and mana, its two spells
    and the 3D model, under the console's names for them."""
    def spell(n, y, text_name):
        return image(f"spell{n}", "skills_window_active", [
            text(text_name, "", f"SpellText{n}", **at(58, 6, 194, 40)),
            image(f"spellicon{n}", widget=f"spellIcon{n}", **at(8, 8, 32, 32)),
        ], widget=f"spellbg{n}", **at(0, y, 48, 48))

    return [
        image("Image0", "character_background", [
            window("name/title", [
                text("name", "Name", "characterName", "SerifBig", BRIGHT_GOLD, "CENTER",
                     **at(0, 0, 250, 25)),
                text("Text1_clone1", "Pet", font="Serif14", colour="FFB4A88C", align="CENTER",
                     **at(0, 26, 250, 20)),
            ], **at(26, 22, 250, 46)),
            image("spells", children=[
                spell(0, 0, "Text"),
                spell(1, 52, "Text_clone"),
            ], **at(16, 76, 268, 110)),
            window("character render window", widget="CharacterViewport",
                   **at(16, 168, 270, 410)),
            window("hp/mp", [
                text("hp", "HP", font="Serif14", colour=LABEL, **at(0, 6, 38, 24)),
                text("hp values", "0", "HP", "Serif14", align="RIGHT", **at(30, 6, 60, 24)),
                text("mp", "MP", font="Serif14", colour=LABEL, **at(140, 6, 38, 24)),
                text("mp values", "0", "Mana", "Serif14", align="RIGHT", **at(164, 6, 60, 24)),
            ], **at(30, 170, 240, 26)),
            window("resistances", [
                text("resistances", "Stats", colour=GOLD_TEXT, align="CENTER",
                     **at(50, 0, 165, 18)),
                image("Image0", "character_stats", **at(12, 22, 241, 70)),
                text("poison", "0", "strPts", "Serif14", align="CENTER", **at(54, 28, 60, 24)),
                text("fire", "0", "magicPts", "Serif14", align="CENTER", **at(184, 28, 60, 24)),
                text("electricty", "0", "dexPts", "Serif14", align="CENTER",
                     **at(54, 62, 60, 24)),
                text("ice", "0", "defPts", "Serif14", align="CENTER", **at(184, 62, 60, 24)),
            ], **at(18, 470, 265, 100)),
        ], widget="TopFrame", **at(0, 0, 303, 629)),
    ]


LAYOUTS = (
    ("media/UI/mastermenu_xenon.layout", master_layout),
    ("media/UI/charctermenu_xenon.layout", character_layout),
    ("media/UI/petmenu_xenon.layout", pet_page_layout),
    ("media/UI/inventorymenu_xenon.layout", bag_layout),
    ("media/UI/statsmenu_xenon.layout", stats_layout),
    ("media/UI/journalmenu_xenon.layout", journal_layout),
    ("media/UI/questmenu_xenon.layout", quest_layout),
    ("media/UI/skillsmenu_xenon.layout", skills_layout),
    ("media/UI/skillmap_xenon.layout", skill_map_layout),
    ("media/UI/enchantermenu_xenon.layout", enchanter_layout),
    ("media/UI/messagehousing_xenon.layout", housing_layout),
    ("media/UI/petcommands_xenon.layout", pet_commands_layout),
    ("media/UI/spellremove_xenon.layout", spell_remove_layout),
)
