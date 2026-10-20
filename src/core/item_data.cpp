#include "core/item_data.h"

#include "core/item_label.h"
#include "core/item_roll.h"
#include "core/level_memory.h"
#include "core/spawn_class.h"
#include "core/text.h"

#include <cstdlib>

namespace tl::core {

ItemStats read_item_stats(const tl::formats::AdmSection& section,
                          const EffectTypeTable& effect_types) {
    ItemStats item;
    item.name = section.string_or("NAME", "");
    if (item.name.empty()) return item;
    item.display_name = strip_markup(section.string_or("DISPLAYNAME", item.name));
    item.base_name = item_label_text(item.display_name);
    item.unidentified_name = item_label_text(section.string_or("UNIDENTIFIED_NAME", ""));
    item.unittype = section.string_or("UNITTYPE", "");
    std::string directory = section.string_or("RESOURCEDIRECTORY", "");
    const std::string mesh = section.string_or("MESHFILE", "");
    if (!directory.empty() && !mesh.empty()) {
        for (char& c : directory) {
            if (c == '\\') c = '/';
        }
        if (directory.back() != '/') directory.push_back('/');
        item.mesh = directory + mesh;
    }
    item.level = static_cast<int>(section.integer_or("LEVEL", 1));
    if (const tl::formats::AdmRecord* collideable = section.find("COLLIDEABLE")) {
        item.collideable = collideable->word_first_byte() != 0;
    }
    // `CEquipment::calculateCombatStats`
    // reads these with its own defaults: 0, 0, 100, 100, and -1 for
    // `DAMAGE_PHYSICAL`.
    item.min_damage = static_cast<float>(section.integer_or("MINDAMAGE", 0));
    item.max_damage = static_cast<float>(section.integer_or("MAXDAMAGE", 0));
    item.rarity_damage_mod = static_cast<int>(section.integer_or("RARITY_DMG_MOD", 100));
    item.speed_damage_mod = static_cast<int>(section.integer_or("SPEED_DMG_MOD", 100));
    for (int type = 0; type < kDamageTypeCount; ++type) {
        const std::string key = std::string("DAMAGE_") + kDamageTypeKeys[type];
        item.damage_split[static_cast<size_t>(type)] =
            static_cast<int>(section.integer_or(key, type == 0 ? -1 : 0));
    }
    // `CEquipment::calculateCombatStats`
    // defaults: 0.45 and 0.6.
    item.range = section.float_or("RANGE", 0.45f);
    item.strike_range = section.float_or("STRIKERANGE", 0.6f);
    item.attack_cooldown = section.float_or("AI_ATTACKCOOLDOWN", 0.0f);
    item.speed = static_cast<float>(section.integer_or("SPEED", 100));
    item.attack_sound = section.string_or("ATTACK_SOUND", "");
    item.strike_sound = section.string_or("STRIKE_SOUND", "");
    item.missile = upper_ascii(section.string_or("MISSILE", ""));
    // `CBaseUnit::unitInit`: each
    // `[SKILL]`'s `NAME`, its `LEVEL` (the root's by default) and `ENABLED`
    // (the root's, true).
    for (const tl::formats::AdmSection& group : section.children) {
        if (group.name != "SKILL") continue;
        ItemStats::Skill skill;
        skill.name = group.string_or("NAME", "");
        if (skill.name.empty()) continue;
        skill.level = static_cast<uint32_t>(group.integer_or("LEVEL", item.level));
        skill.enabled = group.bool_or("ENABLED", section.bool_or("ENABLED", true));
        item.skills.push_back(std::move(skill));
    }
    // `CEquipment::skillDescription`: the
    // `[SKILL_TO_GIVE]` groups, `NAME` then `DISPLAYNAME` over it, and
    // `LEVEL` with the root's `LEVEL` (1 when unauthored) behind it.
    for (const tl::formats::AdmSection& group : section.children) {
        if (group.name != "SKILL_TO_GIVE") continue;
        ItemStats::SkillToGive give;
        give.name = group.string_or("DISPLAYNAME", group.string_or("NAME", ""));
        give.level = static_cast<int>(
            group.integer_or("LEVEL", static_cast<uint32_t>(section.integer_or("LEVEL", 1))));
        item.skills_to_give.push_back(std::move(give));
    }
    item.fall_sound = section.string_or("FALL_SOUND", "");
    item.land_sound = section.string_or("LAND_SOUND", "");
    item.take_sound = section.string_or("TAKE_SOUND", "");
    for (const tl::formats::AdmSection& group : section.children) {
        if (group.name != "WARDROBE") continue;
        ItemStats::Wardrobe entry;
        entry.character_class = group.string_or("CLASS", "");
        entry.has_mesh = group.find("MESH") != nullptr;
        entry.has_texture = group.find("TEXTURE") != nullptr;
        entry.mesh = group.string_or("MESH", "");
        entry.texture = group.string_or("TEXTURE", "");
        item.wardrobe.push_back(std::move(entry));
    }
    item.icon = section.string_or("ICON", "");
    item.rarity = static_cast<int>(section.integer_or("RARITY", 0));
    // `CEquipment::unitInit`: `MAXSTACKSIZE` (1 by default)
    // into +0x1e8, then any stack above 1 is made 99999: the
    // console stacks without limit.
    item.max_stack = static_cast<int>(section.integer_or("MAXSTACKSIZE", 1));
    if (item.max_stack > 1) item.max_stack = 99999;
    item.description = section.string_or("DESCRIPTION", "");
    // `VALUE`, the armour keys, `SET`, `BLOCK_CHANCE`, sockets and the
    // authored requirements; a copy's own figures come from
    // `init_item_copy` (`create_item`).
    read_item_roll_data(item, section);
    {
        const SpawnUnitData spawn = SpawnUnitData::from_adm(section);
        item.spawn_rarity = spawn.rarity;
        item.spawn_min_level = spawn.min_level;
        item.spawn_max_level = spawn.max_level;
        item.create_as = spawn.create_as;
        item.rarity_hardcore = spawn.rarity_hardcore;
    }
    // The use half, `CEquipment::unitInit`.
    // `UNIT_GUID` and `USES` are
    // authored as strings.
    item.unit_guid = std::strtoll(section.string_or("UNIT_GUID", "-1").c_str(), nullptr, 10);
    item.uses = static_cast<int>(std::strtol(section.string_or("USES", "0").c_str(), nullptr, 10));
    // INVENTED: `TARGET_TYPE` is an index into `gTARGET_TYPES`, which is not
    // read; `USER` (the default, and every item but one) is taken as 0.
    item.target_type = upper_ascii(section.string_or("TARGET_TYPE", "USER")) == "USER" ? 0 : 1;
    item.dont_use_on_full = section.bool_or("DONT_USE_ON_FULL", false);
    item.use_sound = section.string_or("USE_SOUND", "");

    // Its `[EFFECT]`s: what a use puts on its target. A transforming
    // fish's one names a `UNITTYPE:PET` unit and a duration in seconds.
    for (const tl::formats::AdmSection& effect : section.children) {
        if (effect.name == "EFFECT" && !effect_types.empty()) {
            item.effects.push_back(parse_effect(effect, effect_types));
        }
    }
    return item;
}

}  // namespace tl::core
