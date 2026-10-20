// A quest's `.DAT.adm` read into its QuestDefinition (core/quest_book.h); split
// from quest_book.cpp, which holds what is done with it.
#include "core/logic_events.h"
#include "core/quest_book.h"
#include "core/quest_names.h"
#include "core/runic_strings.h"
#include "core/unit_themes.h"
#include "core/unit_types.h"

#include "core/random.h"
#include "core/text.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>

namespace tl::core {
using quest_names::same_name;
using quest_names::upper;

// ---------------------------------------------------------------------------
// Reading a `.DAT.adm`
// ---------------------------------------------------------------------------

namespace {

/// `CDataGroup::GetDataValue(key, int)` with a sentinel for "unauthored": 0xffffffff,
/// compared unsigned and only then narrowed.
int optional_int(const tl::formats::AdmSection& section, const char* key, int fallback) {
    const uint32_t raw = section.integer_or(key, 0xffffffffu);
    return raw == 0xffffffffu ? fallback : static_cast<int>(raw);
}

/// Every `QUEST` key in a named child group, in file order.
// PORT: asset parsing, every `QUEST` key of a named child group, in file order.
std::vector<std::string> quest_list(const tl::formats::AdmSection& parent, const char* group) {
    std::vector<std::string> out;
    for (const tl::formats::AdmSection& child : parent.children) {
        if (!same_name(child.name, group)) continue;
        for (const tl::formats::AdmRecord& record : child.records) {
            if (same_name(record.key, "QUEST") && !record.text.empty()) out.push_back(record.text);
        }
    }
    return out;
}

// PORT: asset parsing, a child group by name.
const tl::formats::AdmSection* child_named(const tl::formats::AdmSection& parent,
                                           const char* name) {
    for (const tl::formats::AdmSection& child : parent.children) {
        if (same_name(child.name, name)) return &child;
    }
    return nullptr;
}

QuestObjective parse_objective(const tl::formats::AdmSection& unit) {
    QuestObjective out;
    out.item = unit.string_or("ITEM", "");
    out.monster = unit.string_or("MONSTER", "");
    out.spawn_class = unit.string_or("SPAWNCLASS", "");
    out.spawn_from_unittype = unit.string_or("SPAWNFROMUNITTYPE", "");
    out.unit_type = unit.string_or("UNITTYPE", "");
    out.unique_monster_name = unit.string_or("UNIQUE_MONSTER_NAME", "");

    out.relative_floor = static_cast<int>(unit.integer_or("RELATIVEFLOOR", 0));
    out.specific_floor = optional_int(unit, "SPECIFICFLOOR", -1);
    // `CQuestUnitData::parseDataGroup`: `MINCOUNT` defaults to 0 and is
    // raised to at least 1; `MAXCOUNT` defaults to it and is raised to it.
    out.min_count = std::max(1, static_cast<int>(unit.integer_or("MINCOUNT", 0)));
    out.max_count = std::max(out.min_count, static_cast<int>(unit.integer_or(
                                                "MAXCOUNT", static_cast<uint32_t>(out.min_count))));

    out.remove_from_inventory = unit.bool_or("REMOVEFROMINVENTORY", true);
    out.remove_only_questitems = unit.bool_or("REMOVE_ONLY_QUESTITEMS", true);
    out.never_destroy = unit.bool_or("NEVERDESTROY", true);
    out.create = unit.bool_or("CREATE", true);
    out.make_champion = unit.bool_or("MAKECHAMPION", false);

    // `calculateUnitsFromTag`: the unit's own `[ACQUIRE]` units, kind 1 whatever
    // the parent is (what a champion drops when it dies).
    if (const tl::formats::AdmSection* acquire = child_named(unit, "ACQUIRE")) {
        for (const tl::formats::AdmSection& child : acquire->children) {
            if (!same_name(child.name, "UNIT")) continue;
            out.children.push_back(parse_objective(child));
            out.children.back().kind = 1;
        }
    }
    return out;
}

// `CQuest::calculateUnitsFromTag`: one `CQuestUnitData`
// per `[UNIT]`, each a list of its own. Returns how many to create: `MINCREATE` when above
// 1, else 1, and 0 for a group with no `[UNIT]`.
uint32_t parse_objectives(const tl::formats::AdmSection& root, const char* group, int kind,
                          std::vector<QuestObjective>& out) {
    const tl::formats::AdmSection* section = child_named(root, group);
    if (section == nullptr) return 0;
    const int32_t min_create = static_cast<int32_t>(section->integer_or("MINCREATE", 0));
    const size_t before = out.size();
    for (const tl::formats::AdmSection& unit : section->children) {
        if (!same_name(unit.name, "UNIT")) continue;
        out.push_back(parse_objective(unit));
        out.back().kind = kind;
    }
    if (out.size() == before) return 0;
    return 1 < min_create ? static_cast<uint32_t>(min_create) : 1u;
}

}  // namespace

/// One `[INTRO]` / `[RETURN]` / `[COMPLETE]` / `[PASSIVE]` / `[DETAILS]` entry, with the
/// defaults `CQuestDialog::parseDialogTag` passes (most
/// booleans true). `DIALOG` strings join in file order, `\n` becoming a newline.
///
DialogLine parse_dialog_line(const tl::formats::AdmSection& entry) {
    DialogLine line;
    line.section = entry.name;
    line.unit = entry.string_or("UNITNAME", "");
    line.sound = entry.string_or("SOUND", "");
    line.make_pet_on_accept = entry.bool_or("MAKE_PET_ON_ACCEPT", false);
    line.remove_as_pet_on_complete = entry.bool_or("REMOVE_AS_PET_ON_COMPLETE", true);
    line.destroy_pet = entry.bool_or("DESTROY_PET", true);
    line.floaty = entry.bool_or("FLOATYTEXT", false);
    line.icon_above_head = entry.bool_or("ICONABOVEHEAD", true);
    // Upper-cased as it is read (`STRINGS::StringUpper`).
    line.theme_override = upper(entry.string_or("THEMEOVERRIDE", ""));
    line.look_at_player = entry.bool_or("LOOK_AT_PLAYER", true);
    line.camera_mod_x = entry.float_or("CAMERAMODX", 0.0f);
    line.camera_mod_y = entry.float_or("CAMERAMODY", 0.0f);
    line.camera_mod_z = entry.float_or("CAMERAMODZ", 0.0f);

    // `GIVEITEM` and `TAKEITEM`, each read by `CQuestDialogItem::parseDialogItemGroup`.
    //
    for (const tl::formats::AdmSection& group : entry.children) {
        const bool give = same_name(group.name, "GIVEITEM");
        if (!give && !same_name(group.name, "TAKEITEM")) continue;
        DialogLine::Item item;
        const int32_t count = static_cast<int32_t>(group.integer_or("COUNT", 1));
        item.count = count > 0 ? count : 1;
        item.name = upper(group.string_or("NAME", ""));
        item.gold = item.name == "GOLD";
        (give ? line.give_items : line.take_items).push_back(std::move(item));
    }

    for (const tl::formats::AdmRecord& rec : entry.records) {
        if (same_name(rec.key, "DIALOG") && (rec.type == tl::formats::AdmType::String ||
                                             rec.type == tl::formats::AdmType::Translate)) {
            std::string text = rec.text;
            text = replace_wstring(text, "\\n", "\n");
            line.texts.push_back(std::move(text));
        }
    }
    return line;
}

bool parse_quest_definition(const tl::formats::AdmSection& root, QuestDefinition& out) {
    out = QuestDefinition{};
    out.name = root.string_or("NAME", "");
    if (out.name.empty()) return false;

    out.display_name = root.string_or("DISPLAYNAME", "");
    out.dungeon = root.string_or("DUNGEON", "");
    out.quest_type = root.string_or("QUESTTYPE", "");
    out.alternative_complete_text = root.string_or("ALTERNATIVECOMPLETETEXT", "");
    out.alternative_complete_text_small = root.string_or("ALTERNATIVECOMPLETETEXTSMALL", "");
    out.guid = root.guid_or("QUEST_GUID", 0);

    out.completes_on_accept = root.bool_or("COMPLETESONACCEPT", false);
    out.force_accept = root.bool_or("FORCEACCEPT", false);
    out.show_quest_complete = root.bool_or("SHOWQUESTCOMPLETE", true);
    out.quest_controller_completes = root.bool_or("QUESTCONTROLLERCOMPLETES", false);
    out.no_notification = root.bool_or("NO_NOTIFICATION", false);
    out.clear_dungeon_history = root.bool_or("CLEAR_DUNGEON_HISTORY", false);
    out.repeatable = root.bool_or("REPEATABLE", false);
    out.can_abandon = root.bool_or("CANABANDON", false);
    // `CQuest::loadQuestData`: `SHOWTIPS` defaults to what
    // `CANABANDON` just read.
    out.show_tips = root.bool_or("SHOWTIPS", out.can_abandon);

    out.give_quests_on_complete = quest_list(root, "GIVE_QUESTS_ON_COMPLETE");
    out.complete_on_quest_accept = quest_list(root, "COMPLETE_ON_QUEST_ACCEPT");
    out.complete_on_quest_complete = quest_list(root, "COMPLETE_ON_QUEST_COMPLETE");

    // The kind `CQuest::loadQuestData` passes
    // each group: `DEFEAT` 0, `ACQUIRE` 1, `POPULATE` 0.
    out.defeat_create = parse_objectives(root, "DEFEAT", 0, out.defeat);
    out.acquire_create = parse_objectives(root, "ACQUIRE", 1, out.acquire);
    parse_objectives(root, "POPULATE", 0, out.populate);

    // `CQuestRequirements::parseRequirementTag`.
    if (const tl::formats::AdmSection* need = child_named(root, "REQUIREMENTS")) {
        QuestRequirementSpec& r = out.requirements;
        r.present = true;
        r.min_player_level = need->integer_or("MINPLAYERLEVEL", 0xffffffffu);
        r.max_player_level = need->integer_or("MAXPLAYERLEVEL", 0xffffffffu);
        r.min_dungeon_depth =
            static_cast<int32_t>(need->integer_or("MINDUNGEONDEPTH", 0xffffffffu));
        r.max_dungeon_depth =
            static_cast<int32_t>(need->integer_or("MAXDUNGEONDEPTH", 0xffffffffu));
        r.ruleset = upper(need->string_or("RULESET", ""));
        // The three groups of `gQUEST_REQUIREMENTS`: every value of type 5 or 8,
        // whatever its key, upper-cased; the first group of each name.
        std::vector<std::string>* lists[] = {&r.quests_complete, &r.quests_not_complete,
                                             &r.quests_active};
        const char* names[] = {"QUESTSCOMPLETE", "QUESTSNOTCOMPLETE", "QUESTSACTIVE"};
        for (int i = 0; i < 3; ++i) {
            const tl::formats::AdmSection* group = child_named(*need, names[i]);
            if (group == nullptr) continue;
            for (const tl::formats::AdmRecord& record : group->records) {
                if (record.type == tl::formats::AdmType::String ||
                    record.type == tl::formats::AdmType::Translate) {
                    lists[i]->push_back(upper(record.text));
                }
            }
        }
    }

    if (const tl::formats::AdmSection* pay = child_named(root, "REWARD")) {
        out.reward_spec.gold_min_pct = static_cast<float>(pay->integer_or("GOLDMINPCT", 0));
        out.reward_spec.gold_max_pct = static_cast<float>(pay->integer_or("GOLDMAXPCT", 0));
        out.reward_spec.xp_min_pct = static_cast<float>(pay->integer_or("XPMINPCT", 500));
        out.reward_spec.xp_max_pct = static_cast<float>(pay->integer_or("XPMAXPCT", 800));
        out.reward_spec.fame_min_pct = static_cast<float>(pay->integer_or("FAMEMINPCT", 0));
        out.reward_spec.fame_max_pct = static_cast<float>(pay->integer_or("FAMEMAXPCT", 0));
        out.reward_spec.treasure_class = pay->string_or("TREASURECLASS", "");
        // `CQuestRewards::parseRewardTag`. Quirk kept: the n-th
        // `UNITREWARD`'s `UNITNAME` is looked up in `gRESOURCE_GROUP_NAMES[n]`, so
        // the first is found among items, then monsters, players, props; one not found is left out.
        uint32_t index = 0;
        for (const tl::formats::AdmSection& child : pay->children) {
            if (!same_name(child.name, "UNITREWARD")) continue;
            QuestRewardSpec::UnitReward reward;
            reward.unit = child.string_or("UNITNAME", "");
            reward.identify = child.bool_or("IDENTIFY", true);
            reward.resource_group = index++;
            out.reward_spec.unit_rewards.push_back(std::move(reward));
        }
    }

    // `CQuest::getQuestStrings` for `[INTRO]`, `[RETURN]`,
    // `[COMPLETE]`, `[PASSIVE]` in file order, then the first `[DETAILS]`. A line naming nobody
    // takes the group's `UNITNAME`; one with no `DIALOG` text is dropped.
    if (const tl::formats::AdmSection* dialog = child_named(root, "DIALOG")) {
        const std::string speaker = dialog->string_or("UNITNAME", "");
        bool details_read = false;
        for (const tl::formats::AdmSection& entry : dialog->children) {
            DialogLine line = parse_dialog_line(entry);
            if (line.unit.empty()) line.unit = speaker;
            const bool details = same_name(entry.name, "DETAILS");
            if (details) {
                if (details_read) continue;
                details_read = true;
            }
            if (line.texts.empty()) continue;
            if (same_name(entry.name, "INTRO")) {
                out.intro.push_back(line);
            } else if (same_name(entry.name, "RETURN")) {
                out.ret.push_back(line);
            } else if (same_name(entry.name, "COMPLETE")) {
                out.complete.push_back(line);
            } else if (same_name(entry.name, "PASSIVE")) {
                out.passive.push_back(line);
            } else if (details) {
                out.details = line;
                out.has_details = true;
            }
        }
    }

    return true;
}

}  // namespace tl::core
