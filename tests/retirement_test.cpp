// Retirement and the heirloom: the level gate, the rename, `improveHeirloom` (1.1, up to
// heirloom level 10), the damage roll's top for an heirloom, the new character's fame.
#include "core/retirement.h"
#include "core/affix_book.h"
#include "core/effect_manager.h"
#include "core/effects.h"
#include "core/item_effects.h"
#include "core/item_roll.h"
#include "core/random.h"
#include "core/stats.h"
#include "core/unit_types.h"
#include "core/units.h"
#include "formats/adm.h"
#include "test_support.h"

#include <cmath>
#include <string>
#include <vector>

namespace {

using tl::core::Effect;
using tl::core::EffectActivation;
using tl::core::EffectManager;
using tl::core::EffectTypeTable;
using tl::core::ItemRollContext;
using tl::core::ItemStats;
using tl::core::RunicRandom;
using tl::core::StatBook;
using tl::core::StatGraph;
using tl::core::UnitTypeHierarchy;
using tl::formats::AdmFile;
using tl::formats::AdmRecord;
using tl::formats::AdmSection;
using tl::formats::AdmType;

constexpr int kStrength = 0x45;  // `STRENGTH BONUS`

AdmSection type(const std::string& name, uint32_t id, const std::vector<uint32_t>& ancestors) {
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

UnitTypeHierarchy hierarchy() {
    AdmSection root;
    root.name = "HIERACHY";
    AdmSection types;
    types.name = "UNITTYPES";
    types.children.push_back(type("any", 0, {}));
    types.children.push_back(type("item", 2, {0}));
    types.children.push_back(type("Weapon", 8, {2, 0}));
    types.children.push_back(type("Armor", 13, {2, 0}));
    types.children.push_back(type("Trinket", 39, {2, 0}));
    types.children.push_back(type("NORMAL SWORD", 300, {8, 2, 0}));
    types.children.push_back(type("POTION", 301, {2, 0}));
    root.children.push_back(types);
    UnitTypeHierarchy out;
    out.parse(root);
    return out;
}

AdmRecord str(const std::string& key, const std::string& value) {
    AdmRecord r;
    r.key = key;
    r.type = AdmType::String;
    r.text = value;
    return r;
}

EffectTypeTable effect_types() {
    AdmFile file;
    file.ok = true;
    file.root.name = "EFFECTLIST";
    for (int index = 0; index < 0x91; ++index) {
        AdmSection row;
        row.name = "EFFECT";
        row.records.push_back(str("NAME", "ROW " + std::to_string(index)));
        row.records.push_back(str("TYPE", "Value"));
        file.root.children.push_back(row);
    }
    EffectTypeTable table;
    table.load(file);
    return table;
}

StatGraph line(float y_at_100) {
    StatGraph g(tl::core::GraphType::Linear);
    g.add_point(0.0f, 0.0f);
    g.add_point(100.0f, y_at_100);
    g.process_points();
    return g;
}

/// A curve through (1, 100), (2, 300), ..., one control point per fame level.
StatGraph fame_gate(int points) {
    StatGraph g(tl::core::GraphType::Linear);
    for (int i = 1; i <= points; ++i) g.add_point(static_cast<float>(i), 100.0f * (2 * i - 1));
    g.process_points();
    return g;
}

Effect strength(float low, float high) {
    Effect e;
    e.name = "STR";
    e.type = kStrength;
    e.values[0] = low;
    e.values[1] = high;
    e.duration = tl::core::kDurationAlways;
    e.activation = EffectActivation::Passive;
    return e;
}

struct Rig {
    UnitTypeHierarchy types = hierarchy();
    EffectTypeTable table = effect_types();
    StatBook book;
    tl::core::ItemRollGlobals globals;
    tl::core::AffixBook affixes;
    RunicRandom vol{1};
    RunicRandom game{7};
    Rig() { book.add("BASE_WEAPON_DAMAGE", line(1000.0f)); }  // 100 at level 10
    ItemRollContext rolls() {
        ItemRollContext c;
        c.globals = &globals;
        c.affixes = &affixes;
        c.unit_types = &types;
        c.graphs = &book;
        c.volatile_rng = &vol;
        c.game_rng = &game;
        return c;
    }
    EffectManager::Context effects() const {
        EffectManager::Context c;
        c.types = &table;
        c.graphs = &book;
        c.owner.present = false;
        return c;
    }
};

ItemStats sword() {
    ItemStats it;
    it.name = "NORMAL SWORD";
    it.unittype = "NORMAL SWORD";
    it.display_name = "Rusty Sword";
    it.base_name = "Rusty Sword";
    it.level = 10;
    it.min_damage = 100.0f;
    it.max_damage = 120.0f;
    it.effects.push_back(strength(10.0f, 20.0f));
    return it;
}

bool near(float a, float b) { return std::fabs(a - b) < 0.001f; }

}  // namespace

TL_TEST(retirement_opens_at_the_retirement_age_and_not_below) {
    TL_CHECK(!tl::core::can_retire(19, 20));
    TL_CHECK(tl::core::can_retire(20, 20));
    TL_CHECK(tl::core::can_retire(57, 20));
    // A negative age is the `js` straight to the menu.
    TL_CHECK(tl::core::can_retire(1, -1));
    TL_CHECK_EQ(tl::core::cannot_retire_text(20),
                std::string("You cannot retire until you reach level 20"));
}

TL_TEST(retirement_takes_weapons_armour_and_trinkets_only) {
    Rig rig;
    ItemStats item = sword();
    TL_CHECK(tl::core::can_become_heirloom(item, rig.types));
    item.unittype = "Armor";
    TL_CHECK(tl::core::can_become_heirloom(item, rig.types));
    item.unittype = "Trinket";
    TL_CHECK(tl::core::can_become_heirloom(item, rig.types));
    item.unittype = "POTION";
    TL_CHECK(!tl::core::can_become_heirloom(item, rig.types));
}

TL_TEST(retirement_names_the_heirloom_once_and_raises_its_effects_by_a_tenth) {
    Rig rig;
    ItemStats item = sword();
    tl::core::make_heirloom(item, "Ada", rig.rolls(), rig.effects());
    TL_CHECK_EQ(item.display_name, std::string("Ada's Rusty Sword"));
    TL_CHECK_EQ(item.heirloom_level, 1);
    // The lists now live on the copy as saved effects.
    TL_CHECK(item.from_save);
    TL_CHECK(item.effects.empty());
    TL_CHECK_EQ(item.saved_effects.size(), static_cast<size_t>(1));
    const ItemStats::SavedEffect& held = item.saved_effects.front();
    TL_CHECK(near(held.effect.values[0], 11.0f));
    TL_CHECK(near(held.effect.values[1], 22.0f));
    // The applied value was `Roll`'s low end, 10.
    TL_CHECK(near(held.value, 11.0f));
    TL_CHECK(near(tl::core::item_effect_value(item, rig.effects(), EffectActivation::Passive,
                                              kStrength, tl::core::kDamageTypeAny),
                  11.0f));

    // Handed on again by the next generation: the name stays, the effects grow.
    tl::core::make_heirloom(item, "Bea", rig.rolls(), rig.effects());
    TL_CHECK_EQ(item.display_name, std::string("Ada's Rusty Sword"));
    TL_CHECK_EQ(item.heirloom_level, 2);
    TL_CHECK_EQ(item.saved_effects.size(), static_cast<size_t>(1));
    TL_CHECK(near(item.saved_effects.front().value, 12.1f));
    TL_CHECK(near(item.saved_effects.front().effect.values[1], 24.2f));
}

TL_TEST(retirement_stops_improving_effects_past_heirloom_level_nine) {
    Rig rig;
    ItemStats item = sword();
    item.heirloom_level = 9;
    tl::core::make_heirloom(item, "Ada", rig.rolls(), rig.effects());
    // `cmpl $9; jg` - level 10 changes no effect.
    TL_CHECK_EQ(item.heirloom_level, 10);
    TL_CHECK(!item.from_save);
    TL_CHECK_EQ(item.effects.size(), static_cast<size_t>(1));
    TL_CHECK(near(item.effects.front().values[0], 10.0f));
    // And an item handed on before keeps the name it already has.
    TL_CHECK_EQ(item.display_name, std::string("Rusty Sword"));
}

TL_TEST(retirement_rolls_the_heirloom_at_the_top_of_its_damage) {
    Rig rig;
    ItemStats item = sword();
    tl::core::make_heirloom(item, "Ada", rig.rolls(), rig.effects());
    // The roll's top, 120, plus ten points for heirloom level 1, of 100 at
    // level 10: ceil(100 * 130 / 100).
    TL_CHECK_EQ(item.damage, 130);

    // A loaded copy rolls as a fresh one, off its data: the
    // figure its save laid on is not read.
    ItemStats loaded = sword();
    loaded.from_save = true;
    loaded.damage = loaded.kept_damage = 150;
    loaded.heirloom_level = 1;
    tl::core::make_heirloom(loaded, "Bea", rig.rolls(), rig.effects());
    // ceil(100 * (120 + 20) / 100).
    TL_CHECK_EQ(loaded.damage, 140);
}

TL_TEST(retirement_gives_the_next_character_the_fame_gate_of_its_generation) {
    StatBook book;
    book.add(tl::core::kFameGateGraph, fame_gate(10));
    TL_CHECK_EQ(tl::core::heirloom_fame(0, book), 0);
    TL_CHECK_EQ(tl::core::heirloom_fame(1, book), 100);
    TL_CHECK_EQ(tl::core::heirloom_fame(3, book), 500);
    // `min(generation, 5)`.
    TL_CHECK_EQ(tl::core::heirloom_fame(8, book), 900);
    // And no further than the curve's control points.
    StatBook short_book;
    short_book.add(tl::core::kFameGateGraph, fame_gate(2));
    TL_CHECK_EQ(tl::core::heirloom_fame(4, short_book), 300);
}
