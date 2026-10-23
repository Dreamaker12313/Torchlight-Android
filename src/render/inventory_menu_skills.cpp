// The master menu's skills tab: the console's `CSkillMenuXenon` over `CCharSkillsMenuXenon`, drawn
// from `media/UI/skillsmenu_xenon.layout` and `media/UI/skillmap_xenon.layout` (the sheet `X`
// opens to put a skill on a button). The book and its rules are `core/skills.*` and
// `core/skill_text.*`; this file only draws.
#include "render/inventory_menu.h"

#include "core/log.h"
#include "core/skill_text.h"
#include "core/text.h"
#include "render/menu_chrome.h"
#include "render/overlay_set.h"
#include "render/text_layout.h"

#include <OgreFontManager.h>
#include <OgreMaterialManager.h>
#include <OgreOverlayContainer.h>
#include <OgreOverlayManager.h>
#include <OgrePanelOverlayElement.h>
#include <OgreTextAreaOverlayElement.h>

#include <algorithm>
#include <cstdio>
#include <string>

namespace tl::render {
namespace {

constexpr const char* kSkillsLayout = "media/UI/skillsmenu_xenon.layout";
constexpr const char* kSkillMapLayout = "media/UI/skillmap_xenon.layout";

/// `skillbg%d_%d_%d`, `skillicon%d_%d_%d`, `skilllvl%d_%d_%d` (pane, row,
/// column) - the names the Xbox code looks its cells up.
std::string cell(const char* what, int pane, int row, int column) {
    char buf[48];
    std::snprintf(buf, sizeof(buf), "skills/%s%d_%d_%d", what, pane, row, column);
    return buf;
}

std::string spell_cell(const char* what, int index) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "skills/%s%d", what, index);
    return buf;
}

/// The icon of a skill the grid shows (`+0x94` `SKILL_ICON`, `+0xb0`
/// `SKILL_ICON_INACTIVE`).
const std::string& icon_of(const tl::core::KnownSkill& skill, bool inactive) {
    static const std::string kNone;
    if (skill.def == nullptr) return kNone;
    return inactive ? skill.def->icon_inactive : skill.def->icon;
}

/// The grid's dim alpha for an unlearned skill (`0x3eb33333`, 0.35)
/// and the legend's for a button that would do nothing (0.3).
constexpr float kUnlearnedAlpha = 0.35f;
constexpr float kLegendDimAlpha = 0.3f;

}  // namespace

void InventoryMenu::build_skills_tab(const std::string& group, int container_idx, size_t& serial) {
    have_skills_ = false;
    std::vector<uint8_t> bytes;
    if (!read_menu_resource(kSkillsLayout, group, bytes) || bytes.empty()) {
        TL_LOG_INFO(
            "inventory menu: '%s' not found - re-run scripts/extract_console_ui.py; "
            "the skills tab will be empty",
            kSkillsLayout);
        return;
    }
    const tl::formats::UiLayout skills = tl::formats::parse_ui_layout(bytes.data(), bytes.size());
    if (!skills.ok) {
        TL_LOG_WARN("inventory menu: %s: %s", kSkillsLayout, skills.error.c_str());
        return;
    }
    // Like the quest tab, the whole container: `background` is 901 x 629 with
    // the description column at the left and the panes at `OFFSET X: 300`.
    for (const tl::formats::UiObject& object : skills.objects) {
        build(object, container_idx, "skills/", serial, false, false, 0.0f, 0.0f);
    }
    have_skills_ = true;

    // The map sheet lies over the panes (`CSkillMenuXenon +0x7a0`, built from
    // `skillsmap_xenon.uilayout` and hidden until `X`), so it goes in the
    // floating pair of overlays.
    std::vector<uint8_t> map_bytes;
    if (read_menu_resource(kSkillMapLayout, group, map_bytes) && !map_bytes.empty()) {
        const tl::formats::UiLayout map =
            tl::formats::parse_ui_layout(map_bytes.data(), map_bytes.size());
        if (map.ok) {
            for (const tl::formats::UiObject& object : map.objects) {
                build(object, container_idx, "skillmap/", serial, false, false, 0.0f, 0.0f, true);
            }
        } else {
            TL_LOG_WARN("inventory menu: %s: %s", kSkillMapLayout, map.error.c_str());
        }
    } else {
        TL_LOG_INFO("inventory menu: '%s' not found - skills cannot be mapped", kSkillMapLayout);
    }

    rich_texts_.clear();
    rich_texts_.push_back({"skills/SkillNameText", "skills/SkillNameText", "", &skill_name_pool_});
    rich_texts_.push_back(
        {"skills/SkillDescriptionText", "skills/SkillTextFrame", "", &skill_desc_pool_});
    rich_texts_.push_back(
        {"skills/SkillDescriptionTextNext", "skills/SkillTextFrameNext", "", &skill_next_pool_});
    // The three are drawn as runs over their nodes; the nodes' own captions
    // stay empty.
    for (const RichText& rich : rich_texts_) set_widget_text(rich.text_widget, "");
}

// ---- The cursor (stick up 3, down 4, right 5, left 6) ----

namespace {

tl::core::SkillCellShown shown_by(const tl::core::SkillBook& book) {
    return [&book](const tl::core::SkillCursor& c) {
        return book.at(c.pane, c.row, c.column) != nullptr;
    };
}

}  // namespace

// The skills tab's cursor moves (and its
// siblings, `tl::core::skill_cursor_up`): nothing in map mode.
void InventoryMenu::skill_cursor_up(const tl::core::SkillBook& book) {
    if (skill_map_open_) return;
    tl::core::skill_cursor_up(skill_cursor_, shown_by(book));
}

// The skills tab's cursor moves (and its
// siblings, `tl::core::skill_cursor_down`): nothing in map mode.
void InventoryMenu::skill_cursor_down(const tl::core::SkillBook& book) {
    if (skill_map_open_) return;
    tl::core::skill_cursor_down(skill_cursor_, shown_by(book));
}

// The skills tab's cursor moves (and its
// siblings, `tl::core::skill_cursor_left`): nothing in map mode.
void InventoryMenu::skill_cursor_left(const tl::core::SkillBook& book) {
    if (skill_map_open_) return;
    tl::core::skill_cursor_left(skill_cursor_, shown_by(book));
}

// The skills tab's cursor moves (and its
// siblings, `tl::core::skill_cursor_right`): nothing in map mode.
void InventoryMenu::skill_cursor_right(const tl::core::SkillBook& book) {
    if (skill_map_open_) return;
    tl::core::skill_cursor_right(skill_cursor_, shown_by(book));
}

const tl::core::KnownSkill* InventoryMenu::selected_skill(const tl::core::SkillBook& book) const {
    // the grid's cell, or the spell row's slot. A spell is found by
    // name among the book's skills; the class list is all this port has.
    if (!skill_cursor_.in_spells) {
        return book.at(skill_cursor_.pane, skill_cursor_.row, skill_cursor_.column);
    }
    const std::string& name = book.spells[static_cast<size_t>(skill_cursor_.spell)];
    return name.empty() ? nullptr : book.find_by_name(name);
}

// The skill under the cursor, the book writable.
tl::core::KnownSkill* InventoryMenu::selected_skill(tl::core::SkillBook& book) const {
    return const_cast<tl::core::KnownSkill*>(
        selected_skill(static_cast<const tl::core::SkillBook&>(book)));
}

void InventoryMenu::set_skill_map_open(bool open) {
    // the legend goes, the sheet comes (or the reverse). The "press" frames it would
    // dim (`PressButtonLT`..`B`, `+0x630`..`+0x63c`) and the equipped icons (`SkillEquipped*`)
    // name windows no layout authors, so they do nothing on the console either.
    skill_map_open_ = open;
}

void InventoryMenu::set_widget_alpha(const std::string& widget_name, float alpha) {
    const int root = find_node_index_by_widget(widget_name);
    if (root < 0) return;
    // A CEGUI window's `Alpha` multiplies everything under it; it replaces the root's own.
    const float own = nodes_[static_cast<size_t>(root)].own_alpha;
    const float factor = own > 0.0f ? alpha / own : alpha;
    for (size_t i = 0; i < nodes_.size(); ++i) {
        int p = static_cast<int>(i);
        bool under = false;
        while (p >= 0) {
            if (p == root) {
                under = true;
                break;
            }
            p = nodes_[static_cast<size_t>(p)].parent;
        }
        if (!under) continue;
        Node& n = nodes_[i];
        if (n.panel != nullptr && !n.image.empty()) {
            const Sprite* s = sprite(n.image);
            if (s != nullptr && s->texture) {
                n.panel->setMaterialName(material_for(*s, n.alpha * factor));
            }
        }
        if (n.text != nullptr) {
            Ogre::ColourValue colour = n.colour;
            colour.a *= factor;
            n.text->setColour(colour);
        }
    }
}

float InventoryMenu::alpha_of(int node) const {
    return node < 0 ? 1.0f : nodes_[static_cast<size_t>(node)].alpha;
}

std::string InventoryMenu::material_for(const Sprite& sprite, float alpha) {
    if (alpha >= 1.0f) return sprite.material->getName();
    char key[32];
    std::snprintf(key, sizeof(key), "@a%03d", static_cast<int>(alpha * 100.0f + 0.5f));
    const std::string name = sprite.material->getName() + key;
    auto found = alpha_materials_.find(name);
    if (found == alpha_materials_.end()) {
        Ogre::MaterialPtr faded = build_menu_material(name, sprite.texture, alpha);
        found = alpha_materials_.emplace(name, faded).first;
    }
    return found->second->getName();
}

void InventoryMenu::draw_rich_text(const std::string& text_widget, const std::string& frame_widget,
                                   const std::string& text,
                                   std::vector<Ogre::TextAreaOverlayElement*>& pool) {
    for (RichText& rich : rich_texts_) {
        if (rich.text_widget == text_widget) {
            rich.frame_widget = frame_widget;
            rich.text = text;
            rich.pool = &pool;
        }
    }
    lay_out_rich_texts();
}

void InventoryMenu::lay_out_rich_texts() {
    const bool shown = built_ && visible_ && menu_mode_ == MenuMode::Skills && have_skills_;
    auto& overlays = Ogre::OverlayManager::getSingleton();
    Ogre::FontPtr font = Ogre::FontManager::getSingleton().getByName(kFontName, Ogre::RGN_INTERNAL);
    if (font && !font->isLoaded()) font->load();
    const float across = static_cast<float>(width_ > 0 ? width_ : 1);
    const float down = static_cast<float>(height_ > 0 ? height_ : 1);

    for (RichText& rich : rich_texts_) {
        if (rich.pool == nullptr) continue;
        for (Ogre::TextAreaOverlayElement* area : *rich.pool) area->hide();
        const int text_idx = find_node_index_by_widget(rich.text_widget);
        const int frame_idx = find_node_index_by_widget(rich.frame_widget);
        if (!shown || text_idx < 0 || frame_idx < 0 || rich.text.empty()) continue;
        if (is_node_hidden(text_idx)) continue;
        const Node& text_node = nodes_[static_cast<size_t>(text_idx)];
        const Node& frame = nodes_[static_cast<size_t>(frame_idx)];

        const float line = text_node.points * ui_points_to_pixels() * scale_ / down;
        const std::vector<std::vector<TextSpan>> lines =
            wrap_runs(font, tl::core::markup_runs(rich.text), text_node.width, line, across, down);
        // CEGUI clips a window's text to its parent: the frame is the text
        // node's parent here (`SkillTextFrame` round `SkillDescriptionText`),
        // and lines past its bottom are not drawn.
        const float bottom = frame.top + frame.height;
        const Ogre::ColourValue base = text_node.colour;
        size_t used = 0;
        for (size_t row = 0; row < lines.size(); ++row) {
            const float y = text_node.top + static_cast<float>(row) * line;
            if (y + line > bottom + 0.0001f) break;
            for (const TextSpan& span : lines[row]) {
                if (used == rich.pool->size()) {
                    char name[64];
                    std::snprintf(name, sizeof(name), "tl/inv/skills/rich_%04zu", rich_serial_++);
                    auto* area = static_cast<Ogre::TextAreaOverlayElement*>(
                        overlays.createOverlayElement("TextArea", name));
                    area->setMetricsMode(Ogre::GMM_RELATIVE);
                    area->setFontName(kFontName);
                    text_root_->addChild(area);
                    rich.pool->push_back(area);
                }
                Ogre::TextAreaOverlayElement* area = (*rich.pool)[used++];
                area->setCaption(span.text);
                set_char_height(area, line);
                area->setColour(span.coloured ? Ogre::ColourValue(span.r, span.g, span.b, span.a)
                                              : base);
                area->setPosition(text_node.left + span.x, y);
                area->show();
            }
        }
    }
}

void InventoryMenu::update_skills_tab(const tl::core::SkillBook& book,
                                      const tl::core::CharacterProgression& character) {
    if (!built_ || !have_skills_) return;
    const uint32_t player_level = static_cast<uint32_t>(std::max(1, character.level()));
    const int points = character.unspent_skill_points();
    const tl::core::SkillCursor& cur = skill_cursor_;

    // CCharSkillsMenuXenon::vfunc_6: every cell's frame hidden;
    // the refresh (vfunc_8) shows the ones a skill sits in.
    for (int pane = 0; pane < tl::core::kSkillPanes; ++pane) {
        for (int row = 0; row < tl::core::kSkillRows; ++row) {
            for (int column = 0; column < tl::core::kSkillColumns; ++column) {
                set_widget_visible(cell("skillbg", pane, row, column), false);
            }
        }
    }

    // The panes' names: `CCharacter::getSkillTabName(1..3)` into `PanelName0`..
    // `PanelName2`.
    for (int pane = 0; pane < 3; ++pane) {
        set_widget_text("skills/PanelName" + std::to_string(pane),
                        book.tab_names[static_cast<size_t>(pane)]);
    }
    const tl::core::KnownSkill* selected = nullptr;
    for (const tl::core::KnownSkill& skill : book.skills()) {
        const int pane = skill.entry.pane;
        const int row = skill.entry.row;
        const int column = skill.entry.column;
        const int index = (pane * 6 + row) * 2 + column;
        if (index < 0 || pane < 0 || pane > 2 || row < 0 || row > 5 || column < 0 || column > 1) {
            continue;
        }
        const bool here =
            !cur.in_spells && cur.pane == pane && cur.row == row && cur.column == column;
        const std::string bg = cell("skillbg", pane, row, column);
        set_widget_visible(bg, true);
        // `+0x38 == 4` is a passive: its own frame, and a rollover for the cursor.
        const char* frame =
            skill.passive() ? (here ? "skills_window_passive_rollover" : "skills_window_passive")
                            : (here ? "skills_window_active_rollover" : "skills_window_active");
        set_widget_image(bg, frame);
        if (here) selected = &skill;

        // The icon at 0.35 until a point is in (`+0xa4 == 0`); the grey one and
        // no level while the level needed is out of reach and nothing runs.
        const std::string icon = cell("skillicon", pane, row, column);
        const std::string level = cell("skilllvl", pane, row, column);
        set_widget_visible(icon, true);
        const bool out_of_reach =
            player_level < tl::core::SkillBook::level_required_for_investment(skill) &&
            skill.effective == 0;
        set_widget_image(icon, icon_of(skill, out_of_reach));
        set_widget_alpha(icon, skill.invested == 0 ? kUnlearnedAlpha : 1.0f);
        set_widget_visible(level, !out_of_reach);
        if (!out_of_reach) set_widget_text(level, std::to_string(skill.effective));
    }

    // The spell row (the second loop).
    for (int i = 0; i < tl::core::kSkillSpellSlots; ++i) {
        const bool here = cur.in_spells && cur.spell == i;
        set_widget_image(spell_cell("spellbg", i),
                         here ? "spells_windowb_rollover" : "spells_windowb");
        const std::string& name = book.spells[static_cast<size_t>(i)];
        const tl::core::KnownSkill* spell = name.empty() ? nullptr : book.find_by_name(name);
        const std::string icon = spell_cell("spellIcon", i);
        if (spell != nullptr) {
            set_widget_image(icon, icon_of(*spell, true));
            set_widget_alpha(icon, spell->invested == 0 ? kUnlearnedAlpha : 1.0f);
            if (here) selected = spell;
        } else {
            set_widget_image(icon, "");
            set_widget_alpha(icon, kUnlearnedAlpha);
        }
    }

    // The eight function slots (the third loop): the mapped skill's icon in the
    // tab's own skill-set rows and in the map sheet's, blank when empty or
    // when the skill has no icon.
    for (int slot = 2; slot < tl::core::kSkillFunctionSlots; ++slot) {
        const tl::core::KnownSkill* mapped = book.find_by_guid(book.slot(slot));
        const std::string image = mapped != nullptr ? icon_of(*mapped, false) : std::string();
        set_widget_image("skills/SkillSlot" + std::to_string(slot + 1), image);
        set_widget_image("skillmap/MapSkillSlot" + std::to_string(slot + 1), image);
    }

    // CCharSkillsMenuXenon::vfunc_9: which set's row is up.
    const bool second = book.skill_set() != 0;
    set_widget_visible("skillmap/MapSkillSet2", second);
    set_widget_visible("skillmap/MapSkillSet1", !second);
    set_widget_visible("skills/SkillSet2", second);
    set_widget_visible("skills/SkillSet1", !second);

    // `skillPointsValue` (vfunc_8) and the legend's `SkillPts` (vfunc_9).
    set_widget_text("skills/skillPointsValue", std::to_string(points));

    if (selected != nullptr) describe_skill(*selected, book, character);
    update_skill_legend(skill_map_open_ ? nullptr : selected, character);

    for (size_t i = 0; i < nodes_.size(); ++i) {
        Node& n = nodes_[i];
        if (n.prefix == "skillmap/" && n.parent == container_idx_) {
            n.runtime_hidden = !(menu_mode_ == MenuMode::Skills && skill_map_open_);
        }
    }
    layout();
}

void InventoryMenu::describe_skill(const tl::core::KnownSkill& skill,
                                   const tl::core::SkillBook& book,
                                   const tl::core::CharacterProgression& character) {
    //
    (void)book;
    (void)character;
    if (skill.def == nullptr) return;
    const tl::core::SkillDef& def = *skill.def;

    set_widget_image("skills/CurrentActiveSkillIconImage", def.icon);
    set_widget_alpha("skills/CurrentActiveSkillIconImage", 1.0f);
    set_widget_visible("skills/PassiveDescText", skill.passive());
    set_widget_text("skills/RankTextVal",
                    std::to_string(skill.effective) + "/" + std::to_string(skill.max_rank()));
    set_widget_text("skills/TypeDescText", tl::core::skill_type_caption(def.type));
    draw_rich_text("skills/SkillNameText", "skills/SkillNameText", def.display_name,
                   skill_name_pool_);

    // The level it runs at now, and the next one while there is one: with
    // nothing in, or the rank full, the header goes and the box holds a space.
    const tl::core::UnitTypeHierarchy& types = unit_types_ != nullptr ? *unit_types_ : empty_types_;
    const tl::core::EffectDisplayContext& effects =
        skill_effects_ != nullptr ? *skill_effects_ : empty_effects_;
    draw_rich_text("skills/SkillDescriptionText", "skills/SkillTextFrame",
                   tl::core::skill_description_text(skill, -1, types, effects), skill_desc_pool_);
    const bool has_next = skill.effective != 0 && skill.effective < skill.max_rank();
    set_widget_visible("skills/NextLevelHeader", has_next);
    draw_rich_text("skills/SkillDescriptionTextNext", "skills/SkillTextFrameNext",
                   has_next ? tl::core::skill_description_text(
                                  skill, static_cast<int>(skill.effective) + 1, types, effects)
                            : std::string(" "),
                   skill_next_pool_);
    set_widget_text("skills/RequiredLevelVal",
                    std::to_string(tl::core::SkillBook::level_required_for_investment(skill)));

    // "Bonuses from stat investment": the four icons and percentages, each
    // shown only for a stat that scales the skill, and the caption with any.
    static const char* const kIcons[] = {"skills/SkillTypeStr", "skills/SkillTypeDex",
                                         "skills/SkillTypeMag", "skills/SkillTypeDef"};
    static const char* const kValues[] = {"skills/SkillTypeStrVal", "skills/SkillTypeDexVal",
                                          "skills/SkillTypeMagVal", "skills/SkillTypeDefVal"};
    const std::array<float, tl::core::kSkillStatBonusCount> bonuses =
        tl::core::skill_stat_bonuses(skill, effects);
    bool any = false;
    for (int i = 0; i < tl::core::kSkillStatBonusCount; ++i) {
        const bool shown = 0.0f < bonuses[static_cast<size_t>(i)];
        set_widget_visible(kIcons[i], shown);
        if (shown) {
            set_widget_text(
                kValues[i],
                std::to_string(static_cast<int>(bonuses[static_cast<size_t>(i)] * 100.0f)) + "%");
            any = true;
        }
    }
    set_widget_visible("skills/skillBonuses", any);
}

void InventoryMenu::update_skill_legend(const tl::core::KnownSkill* skill,
                                        const tl::core::CharacterProgression& character) {
    //
    const std::string dialog = "skills/HelpSkillDialog";
    if (skill == nullptr) {
        set_widget_visible(dialog, false);
        return;
    }
    // Under the cell under the cursor: its bottom edge less seven, centred on it
    // horizontally, in the coordinates of the window the panes share.
    const std::string bg = skill_cursor_.in_spells ? spell_cell("spellbg", skill_cursor_.spell)
                                                   : cell("skillbg", skill_cursor_.pane,
                                                          skill_cursor_.row, skill_cursor_.column);
    const int cell_idx = find_node_index_by_widget(bg);
    Node* help = find_node_by_widget(dialog);
    if (cell_idx >= 0 && help != nullptr) {
        const Node& c = nodes_[static_cast<size_t>(cell_idx)];
        const float parent_x = c.parent >= 0 ? nodes_[static_cast<size_t>(c.parent)].off_x : 0.0f;
        const float parent_y = c.parent >= 0 ? nodes_[static_cast<size_t>(c.parent)].off_y : 0.0f;
        help->off_x = parent_x + c.off_x - (help->wide - c.wide) * 0.5f;
        help->off_y = parent_y + c.off_y + c.tall - 7.0f;
    }
    set_widget_visible(dialog, true);
    set_widget_visible("skills/HideButtons", true);
    set_widget_visible("skills/HighLevel", false);

    const int points = character.unspent_skill_points();
    set_widget_text("skills/SkillPts", std::to_string(points));
    const bool active =
        skill->def != nullptr && skill->def->activation == tl::core::SkillActivation::Normal;
    set_widget_visible("skills/XMapSKill", active);
    const uint32_t required = tl::core::SkillBook::level_required_for_investment(*skill);
    set_widget_text("skills/HighLevel", "Requires Level " + std::to_string(required));

    const bool class_skill =
        skill->def == nullptr || skill->def->type == tl::core::SkillType::Skill;
    if (class_skill) {
        if (skill->invested == 0) {
            if (points == 0) {
                set_widget_visible(dialog, false);
            } else if (static_cast<uint32_t>(std::max(1, character.level())) < required) {
                set_widget_visible("skills/HideButtons", false);
                set_widget_visible("skills/HighLevel", true);
            } else {
                set_widget_visible("skills/ALearnSkill", true);
                set_widget_alpha("skills/ALearnSkill", 1.0f);
                set_widget_visible("skills/ALevelUp", false);
                set_widget_visible("skills/XMapSKill", true);
                set_widget_alpha("skills/XMapSKill", kLegendDimAlpha);
            }
        } else {
            set_widget_visible("skills/ALearnSkill", false);
            set_widget_visible("skills/ALevelUp", true);
            set_widget_alpha("skills/ALevelUp", 0 < points ? 1.0f : kLegendDimAlpha);
            set_widget_visible("skills/XMapSKill", true);
            set_widget_alpha("skills/XMapSKill", active ? 1.0f : kLegendDimAlpha);
        }
    } else {
        set_widget_visible("skills/ALearnSkill", false);
        set_widget_visible("skills/ALevelUp", true);
        set_widget_alpha("skills/ALevelUp", kLegendDimAlpha);
        set_widget_visible("skills/XMapSKill", true);
        set_widget_alpha("skills/XMapSKill", 1.0f);
    }
}

}  // namespace tl::render
