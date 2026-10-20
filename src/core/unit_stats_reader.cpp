// A unit's figures off its merged data group.
#include "core/unit_stats_reader.h"

#include "core/ai_stat_watcher.h"
#include "core/combat.h"
#include "core/inventory.h"
#include "core/level_memory.h"
#include "core/text.h"
#include "core/weapons.h"

#include <cctype>
#include <set>
#include <string>

namespace tl::core {

namespace {

std::string lower_ascii(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

}  // namespace

// `CCharacter::unitInit`, `CMonster::unitInit`,
// `CCharacter::loadModel`:
// what they read of the merged data group (`CUnitResourceList::parseChildFile`).
UnitStats read_unit_stats(const tl::formats::AdmFile& file, const std::string& name,
                          const UnitStatsSources& sources) {
    UnitStats stats;
    bool have_walk = false, have_run = false, have_scale = false, have_variation = false;
    bool have_radius = false, have_sight = false, have_reach = false;
    bool have_alignment = false, have_display = false, have_follow = false;
    bool have_motion = false, have_view = false;
    bool have_portrait = false;
    bool have_description = false;
    bool have_attack_speed = false;
    bool have_weapon_scale = false, have_shield_scale = false;
    bool have_armed = false, have_collideable = false, have_invincible = false;
    bool have_targetable = false;
    bool have_use_weapon_damage = false;
    std::set<std::string> taken_keys;
    std::set<std::string> skill_names;
    std::string spawn_right, spawn_left;
    std::string resource_directory;
    std::string mesh_file;
    // The file over its `BASEFILE` chain: the nearer file's keys win, both files' blocks are kept,
    // the base's first.
    if (file.ok) {
        // An authored 0 stands: `CMonster::idleAI` never wanders a unit whose walking speed is 0.
        const float walking = file.float_or("WALKINGSPEED", -1.0f);
        if (!have_walk && walking >= 0.0f) {
            stats.movement.walking_speed = walking;
            have_walk = true;
        }
        const float running = file.float_or("RUNNINGSPEED", -1.0f);
        if (!have_run && running >= 0.0f) {
            stats.movement.running_speed = running;
            have_run = true;
        }
        const float scale = file.float_or("SCALE", -1.0f);
        if (!have_scale && scale > 0.0f) {
            stats.scale = scale;
            have_scale = true;
        }
        if (!have_variation && file.root.find("SCALE_VARIATION") != nullptr) {
            stats.scale_variation = file.float_or("SCALE_VARIATION", 0.0f);
            have_variation = true;
        }
        if (resource_directory.empty()) {
            resource_directory = file.string_or("RESOURCEDIRECTORY", "");
        }
        if (mesh_file.empty()) mesh_file = file.string_or("MESHFILE", "");
        if (stats.themes_root.children.empty()) {
            for (const tl::formats::AdmSection& child : file.root.children) {
                if (child.name == "THEMES") stats.themes_root.children.push_back(child);
            }
        }
        const float radius = file.float_or("COLLISION_RADIUS", -1.0f);
        if (!have_radius && radius >= 0.0f) {
            stats.radius = radius;
            have_radius = true;
        }
        // An authored 0 is a value, not an absence (`CDataGroup::GetDataValue` returns it).
        const float sight = file.float_or("SIGHT_RADIUS", -1.0f);
        if (!have_sight && sight >= 0.0f) {
            stats.sight_radius = sight;
            have_sight = true;
        }
        const float motion = file.float_or("MOTION_RADIUS", -1.0f);
        if (!have_motion && motion >= 0.0f) {
            stats.motion_radius = motion;
            have_motion = true;
        }
        const float view = file.float_or("VIEW_ANGLE", -1.0f);
        if (!have_view && view >= 0.0f) {
            stats.view_angle = view;
            have_view = true;
        }
        const float follow = file.float_or("FOLLOW_RADIUS", -1.0f);
        if (!have_follow && follow >= 0.0f) {
            stats.follow_radius = follow;
            have_follow = true;
        }
        const float reach = file.float_or("REACH_BONUS", -1.0f);
        if (!have_reach && reach >= 0.0f) {
            stats.reach = reach;
            have_reach = true;
        }
        const std::string display = file.string_or("DISPLAYNAME", "");
        if (!have_display && !display.empty()) {
            stats.display_name = strip_markup(display);
            have_display = true;
        }
        // `CCharacter::getDescription` (inlined in `updateIngameUI` and
        // `CNewGameMenu::update`): the line under a target's health bar.
        const std::string description = file.string_or("DESCRIPTION", "");
        if (!have_description && !description.empty()) {
            stats.description = strip_markup(description);
            have_description = true;
        }
        const std::string portrait = file.string_or("PORTRAIT", "");
        if (!have_portrait && !portrait.empty()) {
            stats.portrait = portrait;
            have_portrait = true;
        }
        const float attack_speed = file.float_or("ATTACKSPEED", -1.0f);
        if (!have_attack_speed && attack_speed > 0.0f) {
            stats.attack_speed = attack_speed;
            have_attack_speed = true;
        }
        // Read by `attachToGivenLocation` off the wearer's data, 1.0 when unauthored.
        const float weapon_scale = file.float_or("WEAPON_SCALE", -1.0f);
        if (!have_weapon_scale && weapon_scale >= 0.0f) {
            stats.weapon_scale = weapon_scale;
            have_weapon_scale = true;
        }
        const float shield_scale = file.float_or("SHIELD_SCALE", -1.0f);
        if (!have_shield_scale && shield_scale >= 0.0f) {
            stats.shield_scale = shield_scale;
            have_shield_scale = true;
        }

        // The only place `PET` is written down (`UnitStats::unittype`).
        if (stats.unittype.empty()) stats.unittype = file.string_or("UNITTYPE", "");
        if (stats.dormant_name.empty()) stats.dormant_name = file.string_or("DORMANTNAME", "");
        if (stats.quests.empty()) {
            for (const tl::formats::AdmSection& child : file.root.children) {
                if (child.name != "QUESTS") continue;
                for (const tl::formats::AdmRecord& record : child.records) {
                    if (!record.text.empty()) stats.quests.push_back(record.text);
                }
            }
        }
        if (sources.unit_types != nullptr)
            parse_unit_ai_watchers(stats.ai_watchers, file.root, *sources.unit_types);

        // Its entrance from a spawner; the default is applied below.
        if (stats.spawn_anim.empty()) stats.spawn_anim = file.string_or("SPAWN_ANIM", "");

        // The rest of what `setLevel` reads, its curves among them.
        read_set_level_data(file.root, stats);

        // All 67 sound slots by definition name, inherited like the rest (voices are 42 to 65).
        stats.sounds.inherit([&file](const char* key) { return file.string_or(key, ""); });
        if (file.bool_or("CANMATCHSPEED", false)) stats.can_match_speed = true;

        // What the `unitInit`s and `loadModel` read for the AI, the swing and `setLevel`. An
        // authored value, 0 included, stands; the defaults are on `UnitStats`.
        auto take_any = [&file](const char* key, float& into, std::set<std::string>& taken) {
            if (taken.count(key) != 0 || file.root.find(key) == nullptr) return;
            into = file.float_or(key, into);
            taken.insert(key);
        };
        take_any("ATTACK_RANGE", stats.attack_range, taken_keys);
        take_any("STRIKE_RANGE", stats.strike_range, taken_keys);
        take_any("RANGE_MULTIPLIER", stats.range_multiplier, taken_keys);
        take_any("TURN_RATE", stats.turn_rate, taken_keys);
        take_any("NEAR_WALK_RADIUS", stats.near_walk_radius, taken_keys);
        take_any("DAMAGE_REACT_RADIUS", stats.damage_react_radius, taken_keys);
        take_any("BRAVERY", stats.bravery, taken_keys);
        take_any("ATTACKANGLE", stats.attack_angle, taken_keys);
        take_any("AI_ATTACKCOOLDOWN", stats.attack_cooldown, taken_keys);
        take_any("WALK_ANIM_MULT", stats.walk_anim_mult, taken_keys);
        take_any("RUN_ANIM_MULT", stats.run_anim_mult, taken_keys);
        take_any("JUMP_SPEED", stats.jump_speed, taken_keys);
        if (taken_keys.count("LEVEL") == 0 && file.root.find("LEVEL") != nullptr) {
            stats.level = file.integer_or("LEVEL", stats.level);
            stats.level_authored = true;
            taken_keys.insert("LEVEL");
        }
        if (taken_keys.count("AITYPE") == 0 && file.root.find("AITYPE") != nullptr) {
            static const char* const kAiTypes[] = {"NORMAL",       "RANGEDDEFENDER", "DEFENDER",
                                                   "RANGEDCASTER", "CIRCLER",        "RESURRECTER",
                                                   "DUMMY"};
            std::string type = file.string_or("AITYPE", "");
            for (char& c : type) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            for (int i = 0; i < 7; ++i) {
                if (type == kAiTypes[i]) stats.ai_type = i;
            }
            taken_keys.insert("AITYPE");
        }
        auto take_bool = [&file](const char* key, bool& into, std::set<std::string>& taken) {
            if (taken.count(key) != 0 || file.root.find(key) == nullptr) return;
            into = file.bool_or(key, into);
            taken.insert(key);
        };
        take_bool("DAMAGEABLE", stats.damageable, taken_keys);
        take_bool("INVINCIBLE", stats.invincible_flag, taken_keys);
        take_bool("OCCUPIESNODES", stats.occupies_nodes, taken_keys);
        take_bool("ATTACHES_TO_MASTER", stats.attaches_to_master, taken_keys);
        take_bool("DISPLAYDMG", stats.display_dmg, taken_keys);
        take_bool("DIES_ON_WARP", stats.dies_on_warp, taken_keys);
        if (taken_keys.count("MINION_SPAWNCLASS") == 0 &&
            file.root.find("MINION_SPAWNCLASS") != nullptr) {
            stats.minion_spawnclass = file.string_or("MINION_SPAWNCLASS", "");
            taken_keys.insert("MINION_SPAWNCLASS");
        }
        take_bool("DESTROY_ON_DEATH", stats.destroy_on_death, taken_keys);
        take_bool("RENDERALWAYS", stats.render_always, taken_keys);
        take_bool("BOSS_CHAMPION", stats.boss_champion, taken_keys);
        take_bool("SHADOWS", stats.shadows, taken_keys);
        // `CCharacter::loadModel`: the unit's own
        // particles, by the names `CResourceManager::createParticle` takes.
        auto take_particle = [&file, &taken_keys](const char* key, std::string& into) {
            if (taken_keys.count(key) != 0 || file.root.find(key) == nullptr) return;
            into = upper_ascii(file.string_or(key, ""));
            taken_keys.insert(key);
        };
        take_particle("PARTICLE_GETHIT", stats.particles.get_hit);
        take_particle("PARTICLE_GETHIT_CENSOR", stats.particles.get_hit_censor);
        take_particle("PARTICLE_DEATH", stats.particles.death);
        take_particle("PARTICLE_DEATH_CENSOR", stats.particles.death_censor);
        take_particle("PARTICLE_EXPLODE", stats.particles.explode);
        take_particle("PARTICLE_EXPLODE_CENSOR", stats.particles.explode_censor);
        take_particle("PARTICLE_SPAWN", stats.particles.spawn);
        if (taken_keys.count("MAXIMUM_PET_INSTANCES") == 0 &&
            file.root.find("MAXIMUM_PET_INSTANCES") != nullptr) {
            stats.maximum_pet_instances =
                static_cast<int>(file.integer_or("MAXIMUM_PET_INSTANCES", 5));
            taken_keys.insert("MAXIMUM_PET_INSTANCES");
        }
        if (file.root.find("COLLISIONFILE") != nullptr &&
            !file.string_or("COLLISIONFILE", "").empty()) {
            stats.has_collision_model = true;
        }
        if (!have_armed && file.root.find("ARMED") != nullptr) {
            stats.armed = file.bool_or("ARMED", true);
            have_armed = true;
        }
        // Walked through rather than walked round.
        if (!have_collideable && file.root.find("COLLIDEABLE") != nullptr) {
            stats.collideable = file.bool_or("COLLIDEABLE", true);
            have_collideable = true;
        }
        // `INVINCIBLE:True` or `DAMAGEABLE:False`: either marks the unit unkillable.
        if (!have_invincible &&
            (file.root.find("INVINCIBLE") != nullptr || file.root.find("DAMAGEABLE") != nullptr)) {
            stats.invincible =
                file.bool_or("INVINCIBLE", false) || !file.bool_or("DAMAGEABLE", true);
            have_invincible = true;
        }
        // The unit's own key, not a layout node's `TARGETABLE` (`UnitStats::targetable`).
        if (!have_targetable && file.root.find("TARGETABLE") != nullptr) {
            stats.targetable = file.bool_or("TARGETABLE", true);
            have_targetable = true;
        }
        // `CCharacter::unitInit`: `USEWEAPONDAMAGE`, true when
        // unauthored.
        if (!have_use_weapon_damage && file.root.find("USEWEAPONDAMAGE") != nullptr) {
            stats.use_weapon_damage = file.bool_or("USEWEAPONDAMAGE", true);
            have_use_weapon_damage = true;
        }

        // Every `[SKILL]`, the base's among them (its specials), and what it holds, which is where
        // its ordinary swing comes from.
        file.visit([&](const tl::formats::AdmSection& section) {
            // A passive `PERCENT ACTIVE DISTANCE BONUS` (effect 0x2c) widens active/follow range.
            if (section.name == "EFFECT") {
                if (section.string_or("TYPE", "") == "PERCENT ACTIVE DISTANCE BONUS" &&
                    section.string_or("ACTIVATION", "") == "PASSIVE") {
                    stats.active_distance_bonus += section.float_or("MIN", 0.0f);
                }
                return;
            }
            if (section.name == "SKILL") {
                const std::string skill = section.string_or("NAME", "");
                if (!skill.empty()) skill_names.insert(skill);
                return;
            }
            if (section.name != "EQUIPMENT") return;
            if (stats.right_hand.empty()) {
                stats.right_hand = section.string_or("RIGHTHAND", "");
            }
            if (stats.left_hand.empty()) {
                stats.left_hand = section.string_or("LEFTHAND", "");
            }
            // A monster usually carries a spawn class rather than a named item.
            if (spawn_right.empty()) {
                spawn_right = section.string_or("SPAWNRIGHTHAND", "");
            }
            if (spawn_left.empty()) {
                spawn_left = section.string_or("SPAWNLEFTHAND", "");
            }
        });

        const std::string alignment = file.string_or("ALIGNMENT", "");
        if (!have_alignment && !alignment.empty()) {
            // Three answers, not two (`UnitStats::Alignment`); anything else is hostile.
            if (alignment == "GOOD") {
                stats.alignment = UnitStats::Alignment::Friendly;
            } else if (alignment == "NEUTRAL") {
                stats.alignment = UnitStats::Alignment::Neutral;
            } else {
                stats.alignment = UnitStats::Alignment::Hostile;
            }
            have_alignment = true;
        }
        // A pet authors no `ALIGNMENT` and reads `EVIL` like any unit
        // (`GetDataValue(L"ALIGNMENT", L"EVIL")`); it is
        // `setMaster` that puts it on its master's side.
    }

    // `CCharacter::unitInit`: `DONTDROPGOLD` and the first
    // `[TREASURE]` group (a base's when both author one).
    if (file.ok) {
        bool have_dont_drop_gold = false;
        bool have_treasure = false;
        bool have_spawner = false;
        // A merchant's keys (`playerLevelForMerchantInventory`,
        // `generateMerchantInventory`,
        // `getDefaultMerchantTab`), off the same group.
        bool have_level_factor = false, have_uniques = false, have_magic = false, have_tab = false;
        {
            if (!have_dont_drop_gold && file.root.find("DONTDROPGOLD") != nullptr) {
                stats.dont_drop_gold = file.bool_or("DONTDROPGOLD", false);
                have_dont_drop_gold = true;
            }
            // `CCharacter::getSpawnInvisible()` (inlined
            // in `spawn`): `SPAWN_INVISIBLE`, false when
            // unauthored.
            if (file.root.find("SPAWN_INVISIBLE") != nullptr) {
                stats.spawn_invisible = file.bool_or("SPAWN_INVISIBLE", false);
            }
            // `CCharacter::unitInit`:
            // `RENDERBEHIND`, true when unauthored.
            if (file.root.find("RENDERBEHIND") != nullptr) {
                stats.render_behind = file.bool_or("RENDERBEHIND", true);
            }
            // `CUnitSpawner::setResourceSpawn`: `SPAWNER`, the
            // editor model of a hut or a trapdoor.
            if (!have_spawner && file.root.find("SPAWNER") != nullptr) {
                stats.spawner_model = file.string_or("SPAWNER", "");
                have_spawner = true;
            }
            if (!have_level_factor && file.root.find("PLAYER_LEVEL_FOR_MERCHANT") != nullptr) {
                stats.merchant_level_factor = file.root.float_or("PLAYER_LEVEL_FOR_MERCHANT", 0.0f);
                have_level_factor = true;
            }
            if (!have_uniques && file.root.find("ALLOWS_MERCH_UNIQUES") != nullptr) {
                stats.allows_merch_uniques = file.bool_or("ALLOWS_MERCH_UNIQUES", false);
                have_uniques = true;
            }
            if (!have_magic && file.root.find("ALLOWS_MERCH_MAGIC") != nullptr) {
                stats.allows_merch_magic = file.bool_or("ALLOWS_MERCH_MAGIC", true);
                have_magic = true;
            }
            if (!have_tab && file.root.find("MERCHANTTAB") != nullptr) {
                stats.merchant_tab = static_cast<int>(file.root.integer_or("MERCHANTTAB", 0));
                have_tab = true;
            }
            for (const tl::formats::AdmSection& group : file.root.children) {
                if (have_treasure || group.name != "TREASURE") continue;
                stats.treasure_class = group.string_or("SPAWNCLASS", "");
                stats.treasure_min = static_cast<int>(group.integer_or("MIN", 1));
                stats.treasure_max = static_cast<int>(group.integer_or("MAX", 1));
                have_treasure = true;
            }
        }
    }

    // `CCharacter::spawn()`'s own default (`UnitStats::spawn_anim`): a unit that authors nothing
    // still plays whatever `Spawn*` its model ships.
    if (stats.spawn_anim.empty()) stats.spawn_anim = "SPAWN";

    // `CCharacter::unitInit` makes a champion of
    // a unit whose data says `CHAMPION` or `BOSS_CHAMPION`.
    stats.is_champion = stats.champion || stats.boss_champion;
    // `CCharacter::createDefaultEquipment`'s `[EQUIPMENT]`.
    if (file.ok) stats.default_equipment = read_default_equipment(file.root);
    // `CBaseUnit::reapplyAffixes(false)`: the `[AFFIXES]`
    // group's values (`GetDataGroupByName`, the first), each as a string.
    if (file.ok) {
        for (const tl::formats::AdmSection& child : file.root.children) {
            if (upper_ascii(child.name) != "AFFIXES") continue;
            for (const tl::formats::AdmRecord& record : child.records)
                stats.authored_affixes.push_back(record.text);
            break;
        }
    }
    // `CBaseUnit::reapplyEffects(false)`'s `[EFFECT]`s.
    if (file.ok && sources.effect_types != nullptr)
        stats.own_effects = unit_own_effects(file.root, *sources.effect_types);

    // The model, joined from the two keys, either of which may be the
    // base's. See `UnitStats::mesh`.
    if (!resource_directory.empty() && !mesh_file.empty()) {
        std::string directory = resource_directory;
        for (char& c : directory) {
            if (c == '\\') c = '/';
        }
        if (directory.back() != '/') directory.push_back('/');
        stats.mesh = directory + mesh_file;
    }

    // The skills, resolved to what they play. A skill the index does not
    // know, or one with no animation, is dropped rather than guessed at.
    if (!skill_names.empty()) {
        for (const std::string& skill : skill_names) {
            const AttackSkill* entry = sources.skill ? sources.skill(lower_ascii(skill)) : nullptr;
            if (entry == nullptr) continue;
            stats.attacks.push_back(*entry);
        }
    }

    // The ordinary swing, from what is in hand; it goes first, being the one the unit opens with.
    // A unit with skills and no weapon keeps only its skills.
    if (stats.attacks.empty() || !stats.right_hand.empty() || !stats.left_hand.empty() ||
        !spawn_right.empty() || !spawn_left.empty()) {
        // `SPAWNRIGHTHAND` and `SPAWNLEFTHAND` are rolled for each
        // character as it is made (`roll_unit_equipment`), not for the type.
        stats.spawn_right_hand = spawn_right;
        stats.spawn_left_hand = spawn_left;

        std::string type;
        Hand hand = Hand::Right;
        const std::string* held = nullptr;
        if (!stats.right_hand.empty()) {
            held = &stats.right_hand;
        } else if (!stats.left_hand.empty()) {
            held = &stats.left_hand;
            hand = Hand::Left;
        }
        if (held != nullptr) {
            if (sources.item_unittype) {
                if (const std::optional<std::string> found = sources.item_unittype(*held)) {
                    type = *found;
                    hand = default_hand(type);
                }
            }
        }
        AttackSkill fists;
        fists.name = held == nullptr || held->empty() ? "(bare hands)" : *held;
        fists.animation = attack_animation_class(type, hand);

        // A mesh without that class plays the first attack class it ships, in Core.dll's order.
        std::string dir = resource_directory;
        for (char& c : dir) {
            if (c == 92) c = '/';
        }
        if (!dir.empty() && dir.back() != '/') dir.push_back('/');
        if (!dir.empty() &&
            !(sources.has_animation && sources.has_animation(dir, fists.animation, stats.armed))) {
            for (const char* alternative :
                 {"Polearm", "Rslash", "Lslash", "Rwand", "Lwand", "Rpistol", "Lpistol", "Rifle",
                  "Crossbow", "Bow", "Attack", "Sattack"}) {
                if (!(sources.has_animation &&
                      sources.has_animation(dir, alternative, stats.armed))) {
                    continue;
                }
                fists.animation = alternative;
                break;
            }
        }
        stats.attacks.insert(stats.attacks.begin(), fists);

        if (sources.weapon_attack) {
            stats.weapon_attack = sources.weapon_attack(stats.right_hand, Hand::Right);
            stats.left_weapon_attack = sources.weapon_attack(stats.left_hand, Hand::Left);
        }
    }
    if (!have_use_weapon_damage) stats.use_weapon_damage = true;

    return stats;
}

}  // namespace tl::core
