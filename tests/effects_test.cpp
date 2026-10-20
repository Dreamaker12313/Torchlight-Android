// Effects, affixes and the stat lines a skill level prints, and the two `CSkillProperty`
// functions over them, on values in the shipped data's shape.
#include "core/effects.h"
#include "core/affix_book.h"
#include "core/random.h"
#include "core/unit_types.h"

#include <cmath>
#include "core/skill_text.h"
#include "test_support.h"

namespace {

using tl::formats::AdmFile;
using tl::formats::AdmRecord;
using tl::formats::AdmSection;
using tl::formats::AdmType;

AdmRecord str(const std::string& key, const std::string& value) {
    AdmRecord r;
    r.key = key;
    r.type = AdmType::String;
    r.text = value;
    return r;
}

AdmRecord translated(const std::string& key, const std::string& value) {
    AdmRecord r = str(key, value);
    r.type = AdmType::Translate;
    return r;
}

AdmRecord number(const std::string& key, float value) {
    AdmRecord r;
    r.key = key;
    r.type = AdmType::Float;
    r.number = value;
    return r;
}

AdmRecord integer(const std::string& key, uint32_t value) {
    AdmRecord r;
    r.key = key;
    r.type = AdmType::Integer;
    r.integer = value;
    return r;
}

AdmFile make_affix_file(const std::string& name, int min_spawn, int max_spawn, int weight = 1,
                        int slots = 1, const std::vector<std::string>& unit_types = {},
                        float min_val = 0.0f, float max_val = 0.0f) {
    AdmFile file;
    file.ok = true;
    file.root.name = "AFFIX";
    file.root.records.push_back(str("NAME", name));
    file.root.records.push_back(integer("MIN_SPAWN_RANGE", static_cast<uint32_t>(min_spawn)));
    file.root.records.push_back(integer("MAX_SPAWN_RANGE", static_cast<uint32_t>(max_spawn)));
    file.root.records.push_back(integer("WEIGHT", static_cast<uint32_t>(weight)));
    file.root.records.push_back(integer("SLOTS_OCCUPY", static_cast<uint32_t>(slots)));

    if (!unit_types.empty()) {
        AdmSection ut;
        ut.name = "UNITTYPES";
        for (const std::string& t : unit_types) {
            ut.records.push_back(str("TYPE", t));
        }
        file.root.children.push_back(ut);
    }

    if (min_val != 0.0f || max_val != 0.0f) {
        AdmSection eff;
        eff.name = "EFFECT";
        eff.records.push_back(str("TYPE", "DAMAGE"));
        eff.records.push_back(number("MIN", min_val));
        eff.records.push_back(number("MAX", max_val));
        file.root.children.push_back(eff);
    }

    return file;
}

tl::core::UnitTypeHierarchy make_test_hierarchy() {
    tl::formats::AdmSection root;
    root.name = "HIERACHY";
    tl::formats::AdmSection types;
    types.name = "UNITTYPES";

    auto make_type = [](const std::string& name, uint32_t id,
                        const std::vector<uint32_t>& ancestors) {
        tl::formats::AdmSection s;
        s.name = name;
        s.records.push_back(integer("ID", id));
        for (size_t i = 0; i < ancestors.size(); ++i) {
            s.records.push_back(
                integer(i == 0 ? "child" : "child" + std::to_string(i), ancestors[i]));
        }
        return s;
    };
    types.children.push_back(make_type("ANY", 0, {}));
    types.children.push_back(make_type("ITEM", 10, {0}));
    types.children.push_back(make_type("ARMOR", 20, {10, 0}));
    types.children.push_back(make_type("HELMET", 30, {20, 10, 0}));
    types.children.push_back(make_type("WEAPON", 40, {10, 0}));
    types.children.push_back(make_type("MONSTER", 50, {0}));
    root.children.push_back(types);

    tl::core::UnitTypeHierarchy hierarchy;
    hierarchy.parse(root);
    return hierarchy;
}

/// One `[EFFECT]` row of `media/EffectsList.dat`, with the columns the display
/// path reads.
AdmSection effect_row(const std::string& name, const std::string& good, const std::string& good_ot,
                      const std::string& bad, int min_value, int max_value, const std::string& type,
                      const std::vector<std::string>& value_tags,
                      const std::string& graph1 = std::string()) {
    AdmSection row;
    row.name = "EFFECT";
    row.records.push_back(str("NAME", name));
    row.records.push_back(translated("GOODDES", good));
    row.records.push_back(translated("GOODDESOT", good_ot));
    row.records.push_back(translated("BADDES", bad));
    row.records.push_back(translated("BADDESOT", bad));
    row.records.push_back(integer("MINVAL", static_cast<uint32_t>(min_value)));
    row.records.push_back(integer("MAXVAL", static_cast<uint32_t>(max_value)));
    for (size_t i = 0; i < value_tags.size(); ++i) {
        row.records.push_back(str("VALUE" + std::to_string(i + 1), value_tags[i]));
    }
    if (!graph1.empty()) {
        row.records.push_back(str("GRAPH1", graph1));
        row.records.push_back(str("GRAPH2", graph1));
    }
    row.records.push_back(str("TYPE", type));
    return row;
}

/// The four effect type rows this file needs, in their shipped relative order.
tl::core::EffectTypeTable shipped_types() {
    AdmFile file;
    file.ok = true;
    file.root.name = "EFFECTLIST";
    file.root.children.push_back(effect_row(
        "DAMAGE", "+[VALUE] [DMGTYPE] Damage", "+[VALUE] [DMGTYPE] Damage for [DURATION] seconds",
        "-[VALUE] [DMGTYPE] Damage", -10000, 10000, "Value",
        {"MIN", "MAX", "MINFORCE", "MAXFORCE", "NA"}, "DAMAGE_MONSTER"));
    file.root.children.push_back(effect_row(
        "KNOCK BACK EFFECT", "+[VALUE] Knockback", "+[VALUE] Knockback for [DURATION] seconds",
        "-[VALUE] Knockback", -100, 100, "Value", {"FORCE", "NA", "NA", "NA", "NA"}));
    file.root.children.push_back(
        effect_row("SHIELD BUFFER", "[VALUE] points of Damage Absorption",
                   "[VALUE] points of Damage Absorption for [DURATION] seconds",
                   "-[VALUE] points of Damage Absorption", -10000, 10000, "Value",
                   {"MIN", "MAX", "NA", "NA", "NA"}));
    file.root.children.push_back(effect_row(
        "PERCENT KNOCK BACK RESISTANCE", "[VALUE]% Knock Back Resistance",
        "[VALUE]% Knock Back Resistance for [DURATION] seconds", "-[VALUE]% Knock Back Resistance",
        -100, 100, "Percent", {"RESISTANCE", "NA", "NA", "NA", "NA"}));
    tl::core::EffectTypeTable table;
    table.load(file);
    return table;
}

/// `media/graphs/stats/DAMAGE_MONSTER.DAT.adm`: 1 -> 12, 2 -> 20, 3 -> 28.
tl::core::StatBook shipped_graphs() {
    AdmSection line;
    line.name = "LINE";
    line.records.push_back(str("NAME", "DAMAGE_MONSTER"));
    for (const std::pair<float, float>& point :
         std::vector<std::pair<float, float>>{{1.0f, 12.0f}, {2.0f, 20.0f}, {3.0f, 28.0f}}) {
        AdmSection node;
        node.name = "POINT";
        node.records.push_back(number("X", point.first));
        node.records.push_back(number("Y", point.second));
        line.children.push_back(node);
    }
    tl::core::StatBook book;
    book.add("DAMAGE_MONSTER", tl::core::StatGraph::from(line));
    return book;
}

/// Ember Fireball's affix: a poison `DAMAGE` of 45-65 scaled by half the caster's magic, and
/// a one-second knock back of 5.
AdmFile ember_fireball_affix() {
    AdmFile file;
    file.ok = true;
    file.root.name = "AFFIX";
    file.root.records.push_back(str("NAME", "EMBERFIREBALL"));
    file.root.records.push_back(integer("RANK", 1));
    file.root.records.push_back(integer("MIN_SPAWN_RANGE", 0));
    file.root.records.push_back(integer("MAX_SPAWN_RANGE", 100));

    AdmSection damage;
    damage.name = "EFFECT";
    damage.records.push_back(str("NAME", "EMBERFIREBALL DAMAGE"));
    damage.records.push_back(str("ACTIVATION", "DYNAMIC"));
    damage.records.push_back(str("DURATION", "INSTANT"));
    damage.records.push_back(str("TYPE", "DAMAGE"));
    damage.records.push_back(str("DAMAGE_TYPE", "POISON"));
    damage.records.push_back(str("STATMODIFYNAME", "MAGIC"));
    damage.records.push_back(str("STATMODIFYPERCENT", "50"));
    damage.records.push_back(number("MIN", 45.0f));
    damage.records.push_back(number("MAX", 65.0f));
    damage.records.push_back(number("MINFORCE", 3.0f));
    damage.records.push_back(number("MAXFORCE", 6.0f));
    file.root.children.push_back(damage);

    AdmSection knock;
    knock.name = "EFFECT";
    knock.records.push_back(str("ACTIVATION", "DYNAMIC"));
    knock.records.push_back(str("DURATION", "1"));
    knock.records.push_back(str("TYPE", "KNOCK BACK EFFECT"));
    knock.records.push_back(number("FORCE", 5.0f));
    file.root.children.push_back(knock);
    return file;
}

/// Ember Shield's affix: a 120-point shield scaled by half the caster's magic and half their
/// defense, and full knock-back resistance.
AdmFile ember_shield_affix() {
    AdmFile file;
    file.ok = true;
    file.root.name = "AFFIX";
    file.root.records.push_back(str("NAME", "EMBERSHIELD"));
    file.root.records.push_back(integer("RANK", 0));
    file.root.records.push_back(integer("MIN_SPAWN_RANGE", 0));
    file.root.records.push_back(integer("MAX_SPAWN_RANGE", 100));

    AdmSection shield;
    shield.name = "EFFECT";
    shield.records.push_back(str("NAME", "EMBERSHIELD1"));
    shield.records.push_back(str("ACTIVATION", "DYNAMIC"));
    shield.records.push_back(str("DURATION", "ALWAYS"));
    shield.records.push_back(str("TYPE", "SHIELD BUFFER"));
    shield.records.push_back(str("DAMAGE_TYPE", "PHYSICAL"));
    shield.records.push_back(str("STATMODIFYNAME", "MAGIC,DEFENSE"));
    shield.records.push_back(str("STATMODIFYPERCENT", "50,50"));
    shield.records.push_back(number("MIN", 120.0f));
    shield.records.push_back(number("MAX", 120.0f));
    file.root.children.push_back(shield);

    AdmSection knock;
    knock.name = "EFFECT";
    knock.records.push_back(str("NAME", "EMBERSHIELDKNOCKBACK"));
    knock.records.push_back(str("ACTIVATION", "DYNAMIC"));
    knock.records.push_back(str("DURATION", "ALWAYS"));
    knock.records.push_back(str("TYPE", "PERCENT KNOCK BACK RESISTANCE"));
    knock.records.push_back(number("RESISTANCE", 100.0f));
    file.root.children.push_back(knock);
    return file;
}

/// `[LEVEL1]`: an `[EVENT_TRIGGER]` with ten percent of weapon damage, and the affix at level 2.
AdmSection ember_bolt_level_one() {
    AdmSection level;
    level.name = "LEVEL1";
    level.records.push_back(number("MANACOST", 4.0f));

    AdmSection event;
    event.name = "EVENT_TRIGGER";
    event.records.push_back(
        str("FILE", "media/skills/alchemist/emberfireball/emberfireball.layout"));
    event.records.push_back(number("WEAPONDAMAGEPCT", 10.0f));
    event.records.push_back(number("SOAKSCALEPCT", 20.0f));
    level.children.push_back(event);

    AdmSection affixes;
    affixes.name = "AFFIXES";
    affixes.records.push_back(integer("AFFIXLEVEL", 2));
    affixes.records.push_back(str("AFFIX", "EMBERFIREBALL"));
    level.children.push_back(affixes);
    return level;
}

struct Fixture {
    tl::core::EffectTypeTable types = shipped_types();
    tl::core::StatBook graphs = shipped_graphs();
    tl::core::Affix fireball;
    tl::core::Affix shield;
    tl::core::EffectDisplayContext context;

    Fixture() {
        fireball = tl::core::parse_affix(ember_fireball_affix(), types);
        shield = tl::core::parse_affix(ember_shield_affix(), types);
        context.types = &types;
        context.graphs = &graphs;
        context.owner.magic = 10;
        context.owner.defense = 5;
        context.owner.level = 9;
        context.owner.present = true;
        context.affix = [this](const std::string& name, uint32_t) -> const tl::core::Affix* {
            if (name == "EMBERFIREBALL") return &fireball;
            if (name == "EMBERSHIELD") return &shield;
            return nullptr;
        };
    }
};

}  // namespace

/// `parseEffects`: a row's index *is* its type, and `TYPE` becomes
/// `gEFFECT_TYPE_BONUS` - 1 for `Percent`, 2 for `None`, 0 for `Value`.
TL_TEST(effect_type_table_indexes_rows_in_file_order) {
    const tl::core::EffectTypeTable types = shipped_types();
    TL_CHECK_EQ(types.size(), size_t{4});
    TL_CHECK_EQ(types.index_of("DAMAGE"), 0);
    TL_CHECK_EQ(types.index_of("damage"), 0);  // matched upper-cased
    TL_CHECK_EQ(types.index_of("SHIELD BUFFER"), 2);
    TL_CHECK_EQ(types.index_of("NOT A TYPE"), -1);
    TL_CHECK_EQ(types.at(0)->bonus, 0);
    TL_CHECK_EQ(types.at(3)->bonus, 1);
    TL_CHECK_EQ(types.at(0)->value_tags[0], std::string("MIN"));
}

/// `CEffect::CEffect`: values under the type's `VALUEn` keys; `DURATION` -900 for
/// `ALWAYS`, -1000 if not positive; `STATMODIFYPERCENT` / 100.
TL_TEST(effect_reads_its_numbers_under_its_types_tags) {
    const Fixture f;
    const tl::core::Effect& damage = f.fireball.effects[0];
    TL_CHECK_EQ(damage.type, f.types.index_of("DAMAGE"));
    TL_CHECK_EQ(damage.values[0], 45.0f);
    TL_CHECK_EQ(damage.values[1], 65.0f);
    TL_CHECK_EQ(damage.values[2], 3.0f);  // MINFORCE
    TL_CHECK_EQ(damage.values[3], 6.0f);  // MAXFORCE
    TL_CHECK_EQ(damage.duration, tl::core::kDurationInstant);
    TL_CHECK_EQ(damage.damage_type, tl::core::DamageType::Poison);
    TL_CHECK_EQ(damage.stat_ids.size(), size_t{1});
    TL_CHECK_EQ(damage.stat_ids[0], tl::core::EffectStat::Magic);
    TL_CHECK_EQ(damage.stat_values[0], 0.5f);

    const tl::core::Effect& knock = f.fireball.effects[1];
    TL_CHECK_EQ(knock.values[0], 5.0f);  // FORCE is the type's VALUE1
    TL_CHECK_EQ(knock.duration, 1.0f);

    const tl::core::Effect& shield = f.shield.effects[0];
    TL_CHECK_EQ(shield.duration, tl::core::kDurationAlways);
    TL_CHECK_EQ(shield.stat_ids.size(), size_t{2});
    TL_CHECK_EQ(shield.stat_ids[1], tl::core::EffectStat::Defense);
}

/// `getRangeOfValues`: two different ceilings print as a range, and
/// one value alone picks the larger or the smaller by the caller's flag.
TL_TEST(range_of_values_prints_a_range_only_when_the_ends_differ) {
    TL_CHECK_EQ(tl::core::range_of_values(9.45f, 13.65f, true), std::string("10-14"));
    // `ceilf` runs before the absolute value, so a negative pair rounds toward
    // zero rather than away from it.
    TL_CHECK_EQ(tl::core::range_of_values(-9.45f, -13.65f, true), std::string("9-13"));
    TL_CHECK_EQ(tl::core::range_of_values(5.0f, 5.0f, true), std::string("5"));
    TL_CHECK_EQ(tl::core::range_of_values(5.0f, 0.0f, true), std::string("5"));
    TL_CHECK_EQ(tl::core::range_of_values(5.0f, 0.0f, false), std::string("0"));
}

/// `CEffect::calculateBaseValue` with a curve: a percentage of the curve at the
/// effect's level, scaled by the owner's stats.
TL_TEST(a_graphed_effect_is_a_percentage_of_its_curve_times_the_owners_stats) {
    const Fixture f;
    const tl::core::Affix affix = tl::core::affix_at_level(f.fireball, 2);
    TL_CHECK_EQ(affix.effects[0].level, 2u);
    const tl::core::EffectDisplayValues values =
        tl::core::effect_display_values(affix.effects[0], f.types, f.graphs, f.context.owner);
    // 45 / 100 * 20 * (1 + 10 / 100 * 0.5) = 9.45; 65 / 100 * 20 * 1.05 = 13.65
    TL_CHECK(std::fabs(values.min - 9.45f) < 0.001f);
    TL_CHECK(std::fabs(values.max - 13.65f) < 0.001f);
}

/// `CEffect::calculateBaseValue` with no curve: the number times
/// `CEffect::modifyEffectsByCharacter`.
TL_TEST(an_ungraphed_effect_is_its_number_times_the_owners_stats) {
    const Fixture f;
    const tl::core::Affix shield = tl::core::affix_at_level(f.shield, 5);
    tl::core::EffectOwner owner;
    owner.magic = 200;
    owner.defense = 100;
    owner.present = true;
    // 1 + 200 / 100 * 0.5 + 100 / 100 * 0.5 = 2.5
    TL_CHECK_EQ(tl::core::modify_effects_by_character(shield.effects[0], owner), 2.5f);
    TL_CHECK_EQ(tl::core::effect_base_value(shield.effects[0], tl::core::EffectCalculate::Roll,
                                            f.graphs, owner),
                300.0f);
    // No owner, or no list: the number stands.
    TL_CHECK_EQ(tl::core::effect_base_value(shield.effects[0], tl::core::EffectCalculate::Roll,
                                            f.graphs, tl::core::EffectOwner{}),
                120.0f);
    TL_CHECK_EQ(tl::core::modify_effects_by_character(shield.effects[1], owner), 1.0f);
}

/// `CEffect::getDisplayStats`: the type's template with the tags
/// filled in, and `[DMGTYPE] ` swallowed whole for physical damage.
TL_TEST(an_effect_prints_its_types_template) {
    const Fixture f;
    const tl::core::Affix fireball = tl::core::affix_at_level(f.fireball, 2);
    TL_CHECK_EQ(tl::core::effect_display_stats(fireball.effects[0], f.context),
                std::string("+10-14 Poison Damage"));
    TL_CHECK_EQ(tl::core::effect_display_stats(fireball.effects[1], f.context),
                std::string("+5 Knockback for 1 seconds"));

    // No graph: 120 * (1 + 10 / 100 * 0.5 + 5 / 100 * 0.5) = 129, the caster's
    // magic and defense through `modifyEffectsByCharacter`.
    const tl::core::Affix shield = tl::core::affix_at_level(f.shield, 5);
    TL_CHECK_EQ(tl::core::effect_display_stats(shield.effects[0], f.context),
                std::string("129 points of Damage Absorption"));
    TL_CHECK_EQ(tl::core::effect_display_stats(shield.effects[1], f.context),
                std::string("100% Knock Back Resistance"));
}

/// `CAffix::getDisplayStats`: one line per effect, joined with a
/// newline only between two that both have something to say.
TL_TEST(an_affix_prints_one_line_per_effect) {
    const Fixture f;
    const tl::core::Affix affix = tl::core::affix_at_level(f.fireball, 2);
    TL_CHECK_EQ(tl::core::affix_display_stats(affix, f.context),
                std::string("+10-14 Poison Damage\n+5 Knockback for 1 seconds"));
}

/// `CAffix::fillOutStatBonuses`, `CSkillEffectAndAffixes::fillOutStatBonuses`:
/// each of the six weights takes the largest it is given.
TL_TEST(stat_bonuses_take_the_largest_weight_each_stat_is_given) {
    const Fixture f;
    std::array<float, tl::core::kEffectStatCount> out{};
    tl::core::affix_fill_out_stat_bonuses(tl::core::affix_at_level(f.shield, 5), out, true);
    TL_CHECK_EQ(out[static_cast<size_t>(tl::core::EffectStat::Magic)], 0.5f);
    TL_CHECK_EQ(out[static_cast<size_t>(tl::core::EffectStat::Defense)], 0.5f);
    TL_CHECK_EQ(out[static_cast<size_t>(tl::core::EffectStat::Melee)], 0.0f);
    // The second effect names no stat; with the max rule the first one stands.
    TL_CHECK_EQ(out[static_cast<size_t>(tl::core::EffectStat::Magic)], 0.5f);

    // Without it the last effect wins outright, zero included - the original's
    // behaviour, which is why the screen always asks for the max.
    std::array<float, tl::core::kEffectStatCount> plain{};
    tl::core::affix_fill_out_stat_bonuses(tl::core::affix_at_level(f.shield, 5), plain, false);
    TL_CHECK_EQ(plain[static_cast<size_t>(tl::core::EffectStat::Magic)], 0.0f);
}

/// `CSkillProperty::getSkillStats`: its own block, the first event's weapon-damage
/// line, then each event's own lines.
TL_TEST(a_skill_level_prints_its_affixes_then_its_weapon_damage) {
    const Fixture f;
    const tl::formats::AdmSection level = ember_bolt_level_one();
    tl::core::SkillProperty property;
    property.section = &level;
    TL_CHECK_EQ(tl::core::skill_stats_text(property, f.context),
                std::string("+10-14 Poison Damage\n+5 Knockback for 1 seconds\n"
                            "Weapon Damage: 10%"));
}

/// `CSkillProperty::fillOutStatBonuses` reduced to the console's four:
/// `MELEE`, `RANGED`, `MAGIC`, `DEFENSE`.
TL_TEST(the_screens_stat_bonus_row_is_four_of_the_six_in_its_own_order) {
    const Fixture f;
    const tl::formats::AdmSection level = ember_bolt_level_one();
    tl::core::SkillDef def;
    def.levels.push_back(tl::core::SkillProperty{});
    def.levels[0].section = &level;
    tl::core::KnownSkill skill;
    skill.def = &def;
    skill.effective = 1;

    const std::array<float, tl::core::kEffectStatCount> all =
        tl::core::skill_stat_bonuses_all(skill, f.context);
    TL_CHECK_EQ(all[static_cast<size_t>(tl::core::EffectStat::Magic)], 0.5f);

    const std::array<float, tl::core::kSkillStatBonusCount> four =
        tl::core::skill_stat_bonuses(skill, f.context);
    TL_CHECK_EQ(four[0], 0.0f);  // strength, the array's MELEE
    TL_CHECK_EQ(four[1], 0.0f);  // dexterity, RANGED
    TL_CHECK_EQ(four[2], 0.5f);  // magic
    TL_CHECK_EQ(four[3], 0.0f);  // defense
}

// ---------------------------------------------------------------------------
// AffixBook (CEffectGroupManager) tests
// ---------------------------------------------------------------------------

TL_TEST(affix_book_finds_a_name_upper_cased) {
    tl::core::AffixBook book;
    tl::core::EffectTypeTable effect_types;
    const tl::core::UnitTypeHierarchy types = make_test_hierarchy();
    book.add(tl::core::parse_affix(make_affix_file("TEST_A", 1, 10), effect_types, &types));

    TL_CHECK(book.get_affix("test_a") != nullptr);
    TL_CHECK_EQ(book.get_affix("test_a")->name, std::string("TEST_A"));
    TL_CHECK(book.get_affix("TEST_A") != nullptr);
    TL_CHECK(book.get_affix("TeSt_A") != nullptr);
    TL_CHECK(book.get_affix("TEST_B") == nullptr);
}

TL_TEST(affix_book_keeps_the_binarys_duplicate) {
    tl::core::AffixBook book;
    tl::core::EffectTypeTable effect_types;
    const tl::core::UnitTypeHierarchy types = make_test_hierarchy();

    AdmFile first = make_affix_file("DUP_TEST", 1, 10, 100);
    AdmFile second = make_affix_file("DUP_TEST", 5, 20, 200);

    book.add(tl::core::parse_affix(first, effect_types, &types));
    book.add(tl::core::parse_affix(second, effect_types, &types));

    // The console's `map[name] = index`: the later file wins.
    const tl::core::Affix* found = book.get_affix("dup_test");
    TL_CHECK(found != nullptr);
    TL_CHECK_EQ(found->weight, 200);
    TL_CHECK_EQ(found->min_spawn_range, 5);
    TL_CHECK_EQ(book.size(), static_cast<size_t>(1));
}

TL_TEST(affix_needs_a_listed_unit_type_by_isa) {
    tl::core::AffixBook book;
    const tl::core::UnitTypeHierarchy types = make_test_hierarchy();
    tl::core::EffectTypeTable effect_types;

    // Listed for ARMOR (id 20), plus a name the hierarchy does not know, which
    // `parse_affix` drops, as `parseAffixGroup` does (`getTypeIDByName` -1).
    book.add(
        tl::core::parse_affix(make_affix_file("ARMOR_AFFIX", 1, 10, 1, 1, {"ARMOR", "NOSUCHTYPE"}),
                              effect_types, &types));
    book.add(
        tl::core::parse_affix(make_affix_file("NO_TYPES", 1, 10, 1, 1, {}), effect_types, &types));
    const tl::core::Affix& armor = *book.get_affix("ARMOR_AFFIX");
    const tl::core::Affix& none = *book.get_affix("NO_TYPES");
    TL_CHECK_EQ(armor.unit_types.size(), static_cast<size_t>(1));

    const uint32_t kHelmet = 30;  // child of ARMOR
    const uint32_t kWeapon = 40;  // sibling of ARMOR (unrelated)
    const uint32_t kArmor = 20;

    // Child passes through parent's listing
    TL_CHECK(tl::core::affix_can_be_applied_to_unit_type(armor, kHelmet, types));
    TL_CHECK(tl::core::affix_can_be_applied_to_unit_type(armor, kArmor, types));
    // Unrelated type fails
    TL_CHECK(!tl::core::affix_can_be_applied_to_unit_type(armor, kWeapon, types));
    // No unit types fails
    TL_CHECK(!tl::core::affix_can_be_applied_to_unit_type(none, kHelmet, types));
    TL_CHECK(!tl::core::affix_can_be_applied_to_unit_type(none, kArmor, types));
}

TL_TEST(affix_roll_skips_weight_zero_and_other_unit_types) {
    tl::core::AffixBook book;
    tl::core::EffectTypeTable effect_types;
    const tl::core::UnitTypeHierarchy types = make_test_hierarchy();

    // ARMOR: weight 0 -> should skip
    book.add(tl::core::parse_affix(make_affix_file("ZERO_WEIGHT", 1, 50, 0, 1, {"ARMOR"}),
                                   effect_types, &types));
    // WEAPON: weight 10 -> different unit type, should skip when rolling for ARMOR
    book.add(tl::core::parse_affix(make_affix_file("WEAPON_AFFIX", 1, 50, 10, 1, {"WEAPON"}),
                                   effect_types, &types));
    // ARMOR: weight 10 -> valid candidate
    book.add(tl::core::parse_affix(make_affix_file("VALID_ARMOR", 1, 50, 10, 1, {"ARMOR"}),
                                   effect_types, &types));

    tl::core::RunicRandom rng(12345);
    const uint32_t kArmor = 20;
    std::vector<tl::core::Affix> rolled =
        book.roll_and_create_random_affixes(kArmor, 10, 5, rng, types);

    TL_CHECK_EQ(rolled.size(), static_cast<size_t>(1));
    TL_CHECK_EQ(rolled[0].name, std::string("VALID_ARMOR"));
}

TL_TEST(affix_roll_spawn_range_is_min_inclusive_max_exclusive) {
    tl::core::AffixBook book;
    tl::core::EffectTypeTable effect_types;
    const tl::core::UnitTypeHierarchy types = make_test_hierarchy();

    // Range [10, 20)
    book.add(tl::core::parse_affix(make_affix_file("RANGE_AFFIX", 10, 20, 1, 1, {"ARMOR"}),
                                   effect_types, &types));

    const uint32_t kArmor = 20;

    // Level 10 == min_spawn_range -> candidate (picked)
    {
        tl::core::RunicRandom rng(42);
        auto rolled = book.roll_and_create_random_affixes(kArmor, 10, 1, rng, types);
        TL_CHECK_EQ(rolled.size(), static_cast<size_t>(1));
        TL_CHECK_EQ(rolled[0].name, std::string("RANGE_AFFIX"));
    }
    // Level 19 -> candidate
    {
        tl::core::RunicRandom rng(42);
        auto rolled = book.roll_and_create_random_affixes(kArmor, 19, 1, rng, types);
        TL_CHECK_EQ(rolled.size(), static_cast<size_t>(1));
    }
    // Level 20 == max_spawn_range -> strictly exclusive, NOT a candidate
    {
        tl::core::RunicRandom rng(42);
        auto rolled = book.roll_and_create_random_affixes(kArmor, 20, 1, rng, types);
        TL_CHECK(rolled.empty());
    }
    // Level 9 < min_spawn_range -> NOT a candidate
    {
        tl::core::RunicRandom rng(42);
        auto rolled = book.roll_and_create_random_affixes(kArmor, 9, 1, rng, types);
        TL_CHECK(rolled.empty());
    }
}

TL_TEST(affix_roll_draws_once_per_pick_from_the_generator_it_is_given) {
    tl::core::AffixBook book;
    tl::core::EffectTypeTable effect_types;
    const tl::core::UnitTypeHierarchy types = make_test_hierarchy();

    book.add(tl::core::parse_affix(make_affix_file("AFFIX_A", 1, 50, 1, 1, {"ARMOR"}), effect_types,
                                   &types));
    book.add(tl::core::parse_affix(make_affix_file("AFFIX_B", 1, 50, 1, 1, {"ARMOR"}), effect_types,
                                   &types));
    book.add(tl::core::parse_affix(make_affix_file("AFFIX_C", 1, 50, 1, 1, {"ARMOR"}), effect_types,
                                   &types));

    const uint32_t kArmor = 20;

    // Roll with budget 2: will pick 2 affixes (2 draws)
    tl::core::RunicRandom rng(777);
    std::vector<tl::core::Affix> rolled =
        book.roll_and_create_random_affixes(kArmor, 10, 2, rng, types);
    TL_CHECK_EQ(rolled.size(), static_cast<size_t>(2));

    // Advance reference generator by exactly 2 draws
    tl::core::RunicRandom ref_rng(777);
    (void)ref_rng.between(0.0f, 1.0f);
    (void)ref_rng.between(0.0f, 1.0f);

    // Next draw from both generators must be identical
    TL_CHECK_EQ(rng.between(0.0f, 1.0f), ref_rng.between(0.0f, 1.0f));
}

/// Seed 12345 over PIN_A/B/C (weights 10, 30, 60; budget 3) picks PIN_C, PIN_A, PIN_B.
TL_TEST(affix_roll_is_pinned_for_a_seed) {
    tl::core::AffixBook book;
    tl::core::EffectTypeTable effect_types;
    const tl::core::UnitTypeHierarchy types = make_test_hierarchy();

    // Added in reverse: the candidates are in the map's key order, not the
    // order the files were read.
    book.add(tl::core::parse_affix(make_affix_file("PIN_C", 1, 50, 60, 1, {"ARMOR"}), effect_types,
                                   &types));
    book.add(tl::core::parse_affix(make_affix_file("PIN_B", 1, 50, 30, 1, {"ARMOR"}), effect_types,
                                   &types));
    book.add(tl::core::parse_affix(make_affix_file("PIN_A", 1, 50, 10, 1, {"ARMOR"}), effect_types,
                                   &types));

    const uint32_t kArmor = 20;
    tl::core::RunicRandom rng(12345);
    std::vector<tl::core::Affix> rolled =
        book.roll_and_create_random_affixes(kArmor, 10, 3, rng, types);

    TL_CHECK_EQ(rolled.size(), static_cast<size_t>(3));
    TL_CHECK_EQ(rolled[0].name, std::string("PIN_C"));
    TL_CHECK_EQ(rolled[1].name, std::string("PIN_A"));
    TL_CHECK_EQ(rolled[2].name, std::string("PIN_B"));
}

TL_TEST(affix_roll_discards_what_does_not_fit_while_choices_remain) {
    tl::core::AffixBook book;
    tl::core::EffectTypeTable effect_types;
    const tl::core::UnitTypeHierarchy types = make_test_hierarchy();

    // Two affixes: FIT (1 slot) and NOFIT (2 slots). Budget 1.
    book.add(tl::core::parse_affix(make_affix_file("FIT_AFFIX", 1, 50, 10, 1, {"ARMOR"}),
                                   effect_types, &types));
    book.add(tl::core::parse_affix(make_affix_file("NOFIT_AFFIX", 1, 50, 10, 2, {"ARMOR"}),
                                   effect_types, &types));

    const uint32_t kArmor = 20;

    // NOFIT (slots 2 over a budget of 1) is never returned, whatever the order.
    for (uint32_t seed : {1u, 42u, 100u, 777u, 12345u}) {
        tl::core::RunicRandom rng(seed);
        std::vector<tl::core::Affix> rolled =
            book.roll_and_create_random_affixes(kArmor, 10, 1, rng, types);
        TL_CHECK_EQ(rolled.size(), static_cast<size_t>(1));
        TL_CHECK_EQ(rolled[0].name, std::string("FIT_AFFIX"));
    }
}

TL_TEST(affix_roll_takes_the_last_choice_even_over_budget) {
    tl::core::AffixBook book;
    tl::core::EffectTypeTable effect_types;
    const tl::core::UnitTypeHierarchy types = make_test_hierarchy();

    // One candidate costing 3 slots, budget is 1
    book.add(tl::core::parse_affix(make_affix_file("BIG_AFFIX", 1, 50, 10, 3, {"ARMOR"}),
                                   effect_types, &types));

    const uint32_t kArmor = 20;
    tl::core::RunicRandom rng(42);
    std::vector<tl::core::Affix> rolled =
        book.roll_and_create_random_affixes(kArmor, 10, 1, rng, types);

    // It is the last remaining choice (choice_count == 0 after remove),
    // so it is taken even though budget < slots, and budget goes negative (1 - 3 = -2).
    TL_CHECK_EQ(rolled.size(), static_cast<size_t>(1));
    TL_CHECK_EQ(rolled[0].name, std::string("BIG_AFFIX"));
}

TL_TEST(affix_roll_with_no_budget_draws_nothing) {
    tl::core::AffixBook book;
    tl::core::EffectTypeTable effect_types;
    const tl::core::UnitTypeHierarchy types = make_test_hierarchy();

    book.add(tl::core::parse_affix(make_affix_file("TEST_AFFIX", 1, 50, 10, 1, {"ARMOR"}),
                                   effect_types, &types));

    const uint32_t kArmor = 20;
    tl::core::RunicRandom rng(42);
    std::vector<tl::core::Affix> rolled =
        book.roll_and_create_random_affixes(kArmor, 10, 0, rng, types);
    TL_CHECK(rolled.empty());

    // Generator must be untouched
    tl::core::RunicRandom ref_rng(42);
    TL_CHECK_EQ(rng.between(0.0f, 1.0f), ref_rng.between(0.0f, 1.0f));
}

TL_TEST(affix_roll_caches_candidates_per_unit_type) {
    tl::core::AffixBook book;
    tl::core::EffectTypeTable effect_types;
    const tl::core::UnitTypeHierarchy types = make_test_hierarchy();

    book.add(tl::core::parse_affix(make_affix_file("FIRST_AFFIX", 1, 50, 10, 1, {"ARMOR"}),
                                   effect_types, &types));

    const uint32_t kArmor = 20;

    // First roll builds the cache for ARMOR containing only FIRST_AFFIX
    tl::core::RunicRandom rng(1);
    auto rolled1 = book.roll_and_create_random_affixes(kArmor, 10, 1, rng, types);
    TL_CHECK_EQ(rolled1.size(), static_cast<size_t>(1));
    TL_CHECK_EQ(rolled1[0].name, std::string("FIRST_AFFIX"));

    // Add a second matching affix
    book.add(tl::core::parse_affix(make_affix_file("SECOND_AFFIX", 1, 50, 10, 1, {"ARMOR"}),
                                   effect_types, &types));

    // Roll again: cache hit reuses stored candidate list, SECOND_AFFIX is not a candidate
    for (uint32_t s = 1; s <= 20; ++s) {
        tl::core::RunicRandom r(s);
        auto rolled2 = book.roll_and_create_random_affixes(kArmor, 10, 1, r, types);
        TL_CHECK_EQ(rolled2.size(), static_cast<size_t>(1));
        TL_CHECK_EQ(rolled2[0].name, std::string("FIRST_AFFIX"));
    }

    // After clear(), cache is emptied
    book.clear();
    book.add(tl::core::parse_affix(make_affix_file("FIRST_AFFIX", 1, 50, 10, 1, {"ARMOR"}),
                                   effect_types, &types));
    book.add(tl::core::parse_affix(make_affix_file("SECOND_AFFIX", 1, 50, 10, 1, {"ARMOR"}),
                                   effect_types, &types));

    // Now both are candidates; with budget 2 both will be rolled
    tl::core::RunicRandom r(1);
    auto rolled3 = book.roll_and_create_random_affixes(kArmor, 10, 2, r, types);
    TL_CHECK_EQ(rolled3.size(), static_cast<size_t>(2));
}

TL_TEST(affix_roll_with_no_budget_still_builds_the_cache) {
    tl::core::AffixBook book;
    tl::core::EffectTypeTable effect_types;
    const tl::core::UnitTypeHierarchy types = make_test_hierarchy();
    book.add(tl::core::parse_affix(make_affix_file("FIRST_AFFIX", 1, 50, 10, 1, {"ARMOR"}),
                                   effect_types, &types));

    // No early return on the budget: this roll draws nothing but fixes the
    // ARMOR candidate list, so an affix added afterwards is never rolled.
    const uint32_t kArmor = 20;
    tl::core::RunicRandom rng(1);
    TL_CHECK(book.roll_and_create_random_affixes(kArmor, 10, 0, rng, types).empty());
    book.add(tl::core::parse_affix(make_affix_file("SECOND_AFFIX", 1, 50, 10, 1, {"ARMOR"}),
                                   effect_types, &types));
    for (uint32_t s = 1; s <= 20; ++s) {
        tl::core::RunicRandom r(s);
        const auto rolled = book.roll_and_create_random_affixes(kArmor, 10, 2, r, types);
        TL_CHECK_EQ(rolled.size(), static_cast<size_t>(1));
        TL_CHECK_EQ(rolled[0].name, std::string("FIRST_AFFIX"));
    }
}

TL_TEST(affix_roll_returns_clones_at_the_level) {
    tl::core::AffixBook book;
    const Fixture f;
    const tl::core::UnitTypeHierarchy types = make_test_hierarchy();

    // Create affix with an effect: min 10, max 50, range [1, 10)
    AdmFile file = make_affix_file("SCALED_AFFIX", 1, 10, 1, 1, {"ARMOR"}, 10.0f, 50.0f);
    tl::core::Affix base_affix = tl::core::parse_affix(file, f.types, &types);
    book.add(base_affix);

    const uint32_t kArmor = 20;
    const uint32_t kLevel = 5;
    tl::core::RunicRandom rng(42);
    std::vector<tl::core::Affix> rolled =
        book.roll_and_create_random_affixes(kArmor, kLevel, 1, rng, types);

    TL_CHECK_EQ(rolled.size(), static_cast<size_t>(1));
    TL_CHECK_EQ(rolled[0].name, std::string("SCALED_AFFIX"));

    // Compare against affix_at_level
    const tl::core::Affix expected = tl::core::affix_at_level(base_affix, kLevel);
    TL_CHECK_EQ(rolled[0].effects.size(), expected.effects.size());
    TL_CHECK_EQ(rolled[0].effects[0].min_value(), expected.effects[0].min_value());
    TL_CHECK_EQ(rolled[0].effects[0].max_value(), expected.effects[0].max_value());

    // Stored affix in the book must remain unscaled base values
    const tl::core::Affix* stored = book.get_affix("SCALED_AFFIX");
    TL_CHECK(stored != nullptr);
    TL_CHECK_EQ(stored->effects[0].min_value(), 10.0f);
    TL_CHECK_EQ(stored->effects[0].max_value(), 50.0f);
}

TL_TEST(new_affix_by_name_is_null_for_a_missing_name) {
    tl::core::AffixBook book;
    tl::core::EffectTypeTable effect_types;
    const tl::core::UnitTypeHierarchy types = make_test_hierarchy();

    book.add(tl::core::parse_affix(make_affix_file("EXISTING", 1, 10, 1, 1, {"ARMOR"}),
                                   effect_types, &types));

    auto missing = tl::core::new_affix_by_name(book, "NONEXISTENT", 5);
    TL_CHECK(!missing.has_value());

    auto existing = tl::core::new_affix_by_name(book, "existing", 5);
    TL_CHECK(existing.has_value());
    TL_CHECK_EQ(existing->name, std::string("EXISTING"));
}

TL_TEST(parse_affix_reads_prefix_and_suffix) {
    tl::core::EffectTypeTable effect_types;
    AdmFile file;
    file.ok = true;
    file.root.name = "AFFIX";
    file.root.records.push_back(str("NAME", "TEST_AFFIX"));
    file.root.records.push_back(str("PREFIX", "Glowing [ITEM]"));
    file.root.records.push_back(str("SUFFIX", "[ITEM] of Tests"));
    file.root.records.push_back(integer("RANK", 2));

    const tl::core::Affix affix = tl::core::parse_affix(file, effect_types, nullptr);
    TL_CHECK_EQ(affix.name, std::string("TEST_AFFIX"));
    TL_CHECK_EQ(affix.prefix, std::string("Glowing [ITEM]"));
    TL_CHECK_EQ(affix.suffix, std::string("[ITEM] of Tests"));
    TL_CHECK_EQ(affix.rank, 2);
}

/// `CEffect::setAffixOwner` skips a `USEOWNERLEVEL` effect, but
/// `CAffix::setPercent` does not ask; `CAffix::setSkillOwner`.
TL_TEST(an_affix_level_reaches_an_owner_level_effect_too) {
    const Fixture f;
    tl::core::Affix source = f.fireball;
    source.effects[0].use_owner_level = true;
    source.effects[0].level = 7;
    const tl::core::Affix affix = tl::core::affix_at_level(source, 2);
    TL_CHECK_EQ(affix.effects[0].level, 2u);
    tl::core::Effect effect = source.effects[0];
    tl::core::set_owner_level(effect, 12);
    TL_CHECK_EQ(effect.level, 12u);
}
