// An item copy through its `CItemSaveState` and back; a loaded effect's value is the saved
// `+0xc0`.
#include "core/item_save.h"
#include "core/combat.h"
#include "core/effect_manager.h"
#include "core/item_effects.h"
#include "core/item_roll.h"
#include "core/stats.h"
#include "core/unit_types.h"
#include "core/units.h"
#include "formats/adm.h"
#include "formats/byte_reader.h"
#include "formats/save_file.h"
#include "test_support.h"

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace {

using tl::core::Affix;
using tl::core::Effect;
using tl::core::EffectActivation;
using tl::core::EffectManager;
using tl::core::EffectTypeTable;
using tl::core::ItemStats;
using tl::core::kDamageTypeAny;
using tl::core::kDurationAlways;
using tl::formats::AdmFile;
using tl::formats::AdmRecord;
using tl::formats::AdmSection;
using tl::formats::AdmType;

constexpr int kStrength = 0x45;  // `STRENGTH BONUS`
constexpr int kArmor = 0x08;     // `ARMOR BONUS`

AdmRecord str(const std::string& key, const std::string& value) {
    AdmRecord r;
    r.key = key;
    r.type = AdmType::String;
    r.text = value;
    return r;
}

/// 0x91 rows; row 0x45 names `GRAPH1` so a loaded graph can be checked.
EffectTypeTable types() {
    AdmFile file;
    file.ok = true;
    file.root.name = "EFFECTLIST";
    for (int index = 0; index < 0x91; ++index) {
        AdmSection row;
        row.name = "EFFECT";
        row.records.push_back(str("NAME", "ROW " + std::to_string(index)));
        row.records.push_back(str("TYPE", "Value"));
        if (index == kStrength) row.records.push_back(str("GRAPH1", "STRCURVE"));
        file.root.children.push_back(row);
    }
    EffectTypeTable table;
    table.load(file);
    return table;
}

AdmSection unit_type(const std::string& name, uint32_t id, const std::vector<uint32_t>& ancestors) {
    AdmSection section;
    section.name = name;
    AdmRecord key;
    key.key = "ID";
    key.integer = id;
    section.records.push_back(key);
    for (size_t i = 0; i < ancestors.size(); ++i) {
        AdmRecord child;
        child.key = i == 0 ? "child" : "child" + std::to_string(i);
        child.integer = ancestors[i];
        section.records.push_back(child);
    }
    return section;
}

tl::core::UnitTypeHierarchy hierarchy() {
    AdmSection root;
    root.name = "HIERACHY";
    AdmSection all;
    all.name = "UNITTYPES";
    all.children.push_back(unit_type("any", 0, {}));
    all.children.push_back(unit_type("Weapon", 8, {0}));
    all.children.push_back(unit_type("SOCKETABLE", 120, {0}));
    all.children.push_back(unit_type("NORMAL SWORD", 300, {8, 0}));
    all.children.push_back(unit_type("EMBER", 330, {120, 0}));
    root.children.push_back(all);
    tl::core::UnitTypeHierarchy h;
    h.parse(root);
    return h;
}

Effect effect(const std::string& name, int type, float value) {
    Effect e;
    e.name = name;
    e.type = type;
    e.values[0] = value;
    e.values[1] = value;
    e.duration = kDurationAlways;
    e.activation = EffectActivation::Passive;
    return e;
}

Affix affix(const std::string& name, const std::vector<Effect>& effects) {
    Affix a;
    a.name = name;
    a.min_spawn_range = 0;
    a.max_spawn_range = 100;
    a.effects = effects;
    return a;
}

EffectManager::Context item_context(const EffectTypeTable& table,
                                    const tl::core::StatBook& graphs) {
    EffectManager::Context ctx;
    ctx.types = &table;
    ctx.graphs = &graphs;
    ctx.owner.present = false;
    return ctx;
}

bool near(float a, float b) { return std::fabs(a - b) < 0.0001f; }

/// Through the file's bytes, as `save`/`load` write and read them.
tl::formats::ItemSaveState through_bytes(const tl::formats::ItemSaveState& state) {
    tl::formats::ByteWriter out;
    tl::formats::write_item_state(out, state);
    tl::formats::ByteReader in(out.bytes().data(), out.bytes().size());
    tl::formats::ItemSaveState back;
    TL_CHECK(tl::formats::read_item_state(in, back));
    return back;
}

TL_TEST(item_save_keeps_an_enchant_as_plain_effects_and_nests_the_gem) {
    const EffectTypeTable table = types();
    const tl::core::StatBook graphs;
    const tl::core::UnitTypeHierarchy h = hierarchy();
    const EffectManager::Context ctx = item_context(table, graphs);

    ItemStats sword;
    sword.name = "Sword";
    sword.unittype = "NORMAL SWORD";
    sword.sockets = 2;
    sword.enchant_count = 3;
    sword.identified = false;
    sword.heirloom_level = 1;
    sword.combat_rolled = true;
    sword.graph_ac = sword.kept_armour = 7;
    sword.damage = sword.kept_damage = 21;
    tl::core::add_inherent_damage(sword, 2, 5);  // fire
    tl::core::add_damage_bonus(sword, 2, 3);
    tl::core::add_damage_bonus(sword, 4, 9);  // electric, no inherent part
    sword.affixes.push_back({affix("MIGHT", {effect("MIGHT_STR", kStrength, 10.0f)}), 4});
    ItemStats ember;
    ember.name = "Ember";
    ember.unittype = "EMBER";
    ember.affixes.push_back({affix("GEM_STR", {effect("GEM_STR", kStrength, 3.0f)}), 1});
    TL_CHECK(tl::core::add_container_item(sword, ember, h));

    const tl::formats::ItemSaveState state =
        through_bytes(tl::core::fill_item_save_state(sword, 1, 5, true, ctx, h));
    TL_CHECK_EQ(state.slot, 5);
    TL_CHECK_EQ(state.b5c, 1);
    TL_CHECK_EQ(state.i70, 2);
    TL_CHECK_EQ(state.i6c, 3);
    TL_CHECK_EQ(state.b75, 0);
    TL_CHECK_EQ(state.i28, 1);
    TL_CHECK_EQ(state.i7c, 7);
    TL_CHECK_EQ(state.i78, 21);
    // The affix's effect is written as an effect of the passive list.
    TL_CHECK_EQ(state.effects[0].size(), 1u);
    TL_CHECK_EQ(state.effects[0][0].names[0], std::string("MIGHT_STR"));
    // One pair per damage type, in the order met: the type and its bonus
    // (`+0x368`), written as a float. The inherent part is not saved.
    TL_CHECK_EQ(state.pairs.size(), 2u);
    TL_CHECK_EQ(state.pairs[0].second, 2);
    TL_CHECK_EQ(state.pairs[0].first, 3.0f);
    TL_CHECK_EQ(state.pairs[1].second, 4);
    TL_CHECK_EQ(state.pairs[1].first, 9.0f);
    // The gem at slot -1, not equipped; its affix effects are left out (a
    // `SOCKETABLE` that is not `RANDOMMAGIC` re-applies its affixes on load).
    TL_CHECK_EQ(state.sockets.size(), 1u);
    TL_CHECK_EQ(state.sockets[0].slot, -1);
    TL_CHECK_EQ(state.sockets[0].b5c, 0);
    TL_CHECK_EQ(state.sockets[0].effects[0].size(), 0u);

    // A fresh copy, made from a save, with the record applied: no affixes, the
    // enchant back as a saved effect of the same value.
    ItemStats copy;
    copy.name = "Sword";
    copy.unittype = "NORMAL SWORD";
    tl::core::apply_item_save_state(copy, state, table, nullptr, ctx, h);
    TL_CHECK(copy.from_save);
    // The pairs back through `addDamageBonus`.
    TL_CHECK_EQ(copy.damage_bonus[2], 3);
    TL_CHECK_EQ(copy.damage_bonus[4], 9);
    TL_CHECK_EQ(copy.damage_types.size(), 2u);
    TL_CHECK(copy.affixes.empty());
    TL_CHECK_EQ(copy.saved_effects.size(), 1u);
    TL_CHECK_EQ(copy.sockets, 2);
    TL_CHECK_EQ(copy.enchant_count, 3u);
    TL_CHECK(!copy.identified);
    TL_CHECK_EQ(copy.heirloom_level, 1);
    TL_CHECK_EQ(copy.graph_ac, 7);
    TL_CHECK(near(tl::core::item_effect_value(copy, ctx, EffectActivation::Passive, kStrength,
                                              kDamageTypeAny),
                  10.0f));
    TL_CHECK(tl::core::item_is_magical(copy, h));
}

TL_TEST(a_loaded_copy_takes_no_template_effects_or_block_chance) {
    const EffectTypeTable table = types();
    const tl::core::StatBook graphs;
    const EffectManager::Context ctx = item_context(table, graphs);
    ItemStats shield;
    shield.effects.push_back(effect("OWN", kArmor, 4.0f));
    shield.block_chance = 12;
    // Fresh: the template's block and the block chance.
    TL_CHECK(near(
        tl::core::item_effect_value(shield, ctx, EffectActivation::Passive, kArmor, kDamageTypeAny),
        4.0f));
    // From a save, with nothing saved: `unitInit` put neither on.
    shield.from_save = true;
    TL_CHECK(near(
        tl::core::item_effect_value(shield, ctx, EffectActivation::Passive, kArmor, kDamageTypeAny),
        0.0f));
    TL_CHECK(near(tl::core::item_effect_value(shield, ctx, EffectActivation::Passive,
                                              tl::core::item_effect::kBlockChance, kDamageTypeAny),
                  0.0f));
}

TL_TEST(effect_from_save_record_follows_the_loaders_quirks) {
    const EffectTypeTable table = types();
    tl::formats::EffectSaveState r;
    r.names = {"might", "linked", "FX", ""};
    r.i1c = kStrength;
    r.i20 = 0;
    r.i24 = -900;
    r.b37 = 1;
    r.i3c = 55;
    r.i40 = 99;  // read and dropped
    r.i10 = -1;
    float stored = 6.5f;
    std::memcpy(&r.ic0, &stored, sizeof(stored));
    float authored = 2.0f;
    std::memcpy(&r.ic4[0], &authored, sizeof(authored));

    const ItemStats::SavedEffect saved = tl::core::effect_from_save_record(r, table, nullptr);
    TL_CHECK_EQ(saved.effect.name, std::string("MIGHT"));
    TL_CHECK_EQ(saved.effect.link_name, std::string("LINKED"));
    TL_CHECK_EQ(saved.effect.particle_fx, std::string("FX"));
    TL_CHECK(near(saved.effect.duration, tl::core::kDurationInstant));
    TL_CHECK(near(saved.value, 6.5f));
    TL_CHECK(near(saved.effect.values[0], 2.0f));
    // +0x3c into both +0x3c and +0x40.
    TL_CHECK_EQ(saved.effect.i3c, 55);
    TL_CHECK_EQ(saved.effect.i40, 55);
    // -1 is level 0, and so is anything above 1000.
    TL_CHECK_EQ(saved.effect.level, 0u);
    r.i10 = 1001;
    TL_CHECK_EQ(tl::core::effect_from_save_record(r, table, nullptr).effect.level, 0u);
    r.i10 = 1000;
    TL_CHECK_EQ(tl::core::effect_from_save_record(r, table, nullptr).effect.level, 1000u);
    // `setGraphs` fills graph 0 only: the type's `GRAPH1`, no override.
    TL_CHECK_EQ(saved.effect.graphs[0], std::string("STRCURVE"));
    TL_CHECK(saved.effect.graphs[1].empty());
    TL_CHECK(saved.effect.has_graphs);
    // Without +0x37, no graph at all.
    r.b37 = 0;
    TL_CHECK(tl::core::effect_from_save_record(r, table, nullptr).effect.graphs[0].empty());
}

TL_TEST(a_saved_effect_round_trips_through_its_record) {
    const EffectTypeTable table = types();
    tl::core::LiveEffect live;
    live.effect = effect("KEEP", kStrength, 3.0f);
    live.effect.exclusive = true;
    live.effect.save = true;
    live.effect.level = 12;
    live.value = 4.25f;
    live.elapsed = 1.5f;
    const tl::formats::EffectSaveState record = tl::core::effect_save_record(live);
    TL_CHECK_EQ(record.i24, -1000);
    const ItemStats::SavedEffect back = tl::core::effect_from_save_record(record, table, nullptr);
    TL_CHECK(near(back.value, 4.25f));
    TL_CHECK(near(back.elapsed, 1.5f));
    TL_CHECK(back.effect.exclusive);
    TL_CHECK(back.effect.save);
    TL_CHECK_EQ(back.effect.level, 12u);
    TL_CHECK_EQ(back.effect.type, kStrength);
}

TL_TEST(item_save_writes_computed_name_parts) {
    const tl::core::UnitTypeHierarchy h = hierarchy();
    const EffectTypeTable table = types();
    const tl::core::StatBook graphs;
    const EffectManager::Context ctx = item_context(table, graphs);

    ItemStats sword;
    sword.name = "TEST_SWORD";
    sword.display_name = "Testblade";
    sword.base_name = "Testblade";
    sword.unittype = "MAGIC SWORD";
    sword.identified = false;

    Affix affix;
    affix.name = "TEST_AFFIX";
    affix.prefix = "Glowing [ITEM]";
    affix.suffix = "[ITEM] of Tests";
    affix.rank = 1;
    sword.affixes.push_back({affix, 1});

    const tl::formats::ItemSaveState state =
        tl::core::fill_item_save_state(sword, 1, 0, false, ctx, h);
    TL_CHECK_EQ(state.s18, std::string("Glowing [ITEM]"));
    TL_CHECK_EQ(state.s20, std::string("[ITEM] of Tests"));
}

}  // namespace

// `CEquipment::createElementalDamages`: a weapon's passive `DAMAGE` and
// `DAMAGE BONUS` effects become its damage bonus; `getDamageBonus` adds sockets'.
TL_TEST(a_weapons_damage_effects_fold_into_its_damage_bonus) {
    const EffectTypeTable table = types();
    const tl::core::StatBook graphs;
    const tl::core::UnitTypeHierarchy h = hierarchy();
    const EffectManager::Context ctx = item_context(table, graphs);

    const auto fire = [](const std::string& name, int type, float value) {
        Effect e = effect(name, type, value);
        e.damage_type = tl::core::DamageType::Fire;
        return e;
    };

    ItemStats sword;
    sword.name = "Sword";
    sword.unittype = "NORMAL SWORD";
    tl::core::add_inherent_damage(sword, 4, 6);  // electric, rolled
    sword.affixes.push_back(
        {affix("OF FLAME", {fire("FLAME", 0xa, 7.9f), effect("FLAME_STR", kStrength, 2.0f)}), 1});
    sword.effects.push_back(fire("OWN", 0x34, 4.0f));
    Effect timed = fire("TIMED", 0x34, 50.0f);
    timed.activation = EffectActivation::Dynamic;
    sword.effects.push_back(timed);

    tl::core::create_elemental_damages(sword, ctx, h);
    // 4 and 7 (the value is cut to an integer), by type; the list keeps the
    // order the types were met in.
    TL_CHECK_EQ(sword.damage_bonus[2], 11);
    TL_CHECK_EQ(sword.damage_types.size(), 2u);
    TL_CHECK_EQ(sword.damage_types[0], 4);
    TL_CHECK_EQ(sword.damage_types[1], 2);
    // The two are off the item; the strength effect and the dynamic one stay.
    TL_CHECK_EQ(sword.affixes[0].affix.effects.size(), 1u);
    TL_CHECK_EQ(sword.effects.size(), 1u);
    TL_CHECK_EQ(sword.effects[0].name, std::string("TIMED"));
    TL_CHECK(near(tl::core::item_effect_value(sword, ctx, EffectActivation::Passive, kStrength,
                                              kDamageTypeAny),
                  2.0f));
    // A second pass finds nothing more.
    tl::core::create_elemental_damages(sword, ctx, h);
    TL_CHECK_EQ(sword.damage_bonus[2], 11);

    TL_CHECK_EQ(tl::core::weapon_damage_bonus(sword, 2, ctx), 11);
    TL_CHECK_EQ(tl::core::weapon_damage_bonus(sword, 4, ctx), 6);
    // `isMagical` answers on the bonus alone: a copy with no
    // affix and no passive effect left is still magical.
    ItemStats plain;
    plain.unittype = "NORMAL SWORD";
    TL_CHECK(!tl::core::item_is_magical(plain, h));
    tl::core::add_inherent_damage(plain, 3, 4);
    TL_CHECK(!tl::core::item_is_magical(plain, h));
    tl::core::add_damage_bonus(plain, 3, 1);
    TL_CHECK(tl::core::item_is_magical(plain, h));
    TL_CHECK_EQ(tl::core::weapon_damage_bonus(sword, 3, ctx), 0);

    // A gem in a socket keeps its effects; they are counted where they sit.
    sword.sockets = 1;
    ItemStats ember;
    ember.name = "Ember";
    ember.unittype = "EMBER";
    // (Its affix is one a weapon takes: `addContainerItem` drops the others.)
    Affix gem_fire = affix("GEM_FIRE", {fire("GEM_FIRE", 0x34, 5.5f)});
    gem_fire.unit_types = {8};
    ember.affixes.push_back({gem_fire, 1});
    TL_CHECK(tl::core::add_container_item(sword, ember, h));
    tl::core::create_elemental_damages(sword, ctx, h);
    TL_CHECK_EQ(sword.damage_bonus[2], 11);
    TL_CHECK_EQ(sword.socketed[0].affixes.size(), 1u);
    TL_CHECK_EQ(sword.socketed[0].affixes[0].affix.effects.size(), 1u);
    TL_CHECK_EQ(tl::core::weapon_damage_bonus(sword, 2, ctx), 16);  // (int)(11 + 5.5)

    // Not a weapon: left as it is.
    ItemStats loose = ember;
    tl::core::create_elemental_damages(loose, ctx, h);
    TL_CHECK(loose.damage_types.empty());
    TL_CHECK_EQ(loose.affixes[0].affix.effects.size(), 1u);

    // `clearDamageBonuses` empties all three lists, the inherent
    // damage with them.
    tl::core::clear_damage_bonuses(sword, ctx, h);
    TL_CHECK(sword.damage_types.empty());
    TL_CHECK_EQ(sword.damage_bonus[2], 0);
    TL_CHECK_EQ(sword.inherent_damage[4], 0);
    TL_CHECK_EQ(tl::core::weapon_damage_bonus(sword, 2, ctx), 5);  // the gem's alone
}
