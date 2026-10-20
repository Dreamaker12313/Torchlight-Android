#include "core/unit_themes.h"
#include "core/effect_manager.h"
#include "core/effects.h"
#include "core/random.h"
#include "formats/adm.h"
#include "test_support.h"

#include <string>
#include <utility>
#include <vector>

namespace {

using tl::core::Effect;
using tl::core::EffectActivation;
using tl::core::EffectManager;
using tl::core::UnitTheme;
using tl::core::UnitThemeBook;
using tl::core::UnitThemeParticle;
using tl::core::UnitThemeSet;
using tl::formats::AdmFile;
using tl::formats::AdmRecord;
using tl::formats::AdmSection;
using tl::formats::AdmType;

AdmRecord str(const std::string& key, const std::string& value, AdmType type = AdmType::String) {
    AdmRecord r;
    r.key = key;
    r.type = type;
    r.text = value;
    return r;
}

AdmRecord integer(const std::string& key, uint32_t value) {
    AdmRecord r;
    r.key = key;
    r.type = AdmType::Integer;
    r.integer = value;
    return r;
}

AdmRecord boolean(const std::string& key, bool value) {
    AdmRecord r;
    r.key = key;
    r.type = AdmType::Bool;
    r.integer = value ? 1 : 0;
    return r;
}

AdmRecord guid_rec(const std::string& key, int64_t value) {
    AdmRecord r;
    r.key = key;
    r.type = AdmType::Guid;
    r.guid = value;
    return r;
}

AdmFile make_theme_file(const std::string& name, int64_t guid_val,
                        const std::vector<AdmSection>& particles = {}) {
    AdmFile file;
    file.ok = true;
    file.root.name = "UNITTHEME";
    file.root.records.push_back(str("NAME", name));
    file.root.records.push_back(guid_rec("GUID", guid_val));
    file.root.children = particles;
    return file;
}

}  // namespace

TL_TEST(theme_book_keeps_the_callers_file_order) {
    UnitThemeBook book;
    std::vector<AdmFile> files = {
        make_theme_file("BETA", 100),
        make_theme_file("ALPHA", 200),
        make_theme_file("GAMMA", 300),
    };
    book.reload(files);

    TL_CHECK_EQ(book.size(), 3u);
    TL_CHECK_EQ(book.themes()[0].name, "BETA");
    TL_CHECK_EQ(book.themes()[1].name, "ALPHA");
    TL_CHECK_EQ(book.themes()[2].name, "GAMMA");
}

TL_TEST(theme_book_finds_by_name_and_by_guid) {
    UnitThemeBook book;
    std::vector<AdmFile> files = {
        make_theme_file("champion", 1234567890LL),  // lower-case in data, upper-cased by reload
        make_theme_file("STUNNED", 9876543210LL),
    };
    book.reload(files);

    // Finding by upper-case name (since reload converts NAME to uppercase):
    const UnitTheme* champion = book.get_unit_theme("CHAMPION");
    TL_CHECK(champion != nullptr);
    TL_CHECK_EQ(champion->name, "CHAMPION");
    TL_CHECK_EQ(champion->guid, 1234567890LL);

    // Finding by GUID:
    const UnitTheme* by_guid = book.get_unit_theme(1234567890LL);
    TL_CHECK_EQ(by_guid, champion);

    const UnitTheme* stunned = book.get_unit_theme(9876543210LL);
    TL_CHECK(stunned != nullptr);
    TL_CHECK_EQ(stunned->name, "STUNNED");

    // Misses:
    TL_CHECK(book.get_unit_theme("NONEXISTENT") == nullptr);
    TL_CHECK(book.get_unit_theme(111111LL) == nullptr);
}

TL_TEST(theme_particle_block_reads_every_key_with_its_default) {
    // 1. All defaults:
    AdmSection empty_part;
    empty_part.name = "PARTICLE";
    UnitThemeParticle p1 = UnitThemeParticle::load(empty_part);
    TL_CHECK_EQ(p1.file, "");
    TL_CHECK_EQ(p1.count, 1u);
    TL_CHECK_EQ(p1.unit_type, "ANY");
    TL_CHECK(p1.attaches);
    TL_CHECK(!p1.follows);  // default is !attaches -> !true = false
    TL_CHECK_EQ(p1.bone, "");

    // 2. ATTACHES = false, FOLLOWS omitted -> FOLLOWS defaults to !attaches = true:
    AdmSection unattached_part;
    unattached_part.name = "PARTICLE";
    unattached_part.records.push_back(boolean("ATTACHES", false));
    UnitThemeParticle p2 = UnitThemeParticle::load(unattached_part);
    TL_CHECK(!p2.attaches);
    TL_CHECK(p2.follows);

    // 3. Explicit ATTACHES = true and FOLLOWS = true:
    AdmSection both_true;
    both_true.name = "PARTICLE";
    both_true.records.push_back(boolean("ATTACHES", true));
    both_true.records.push_back(boolean("FOLLOWS", true));
    UnitThemeParticle p3 = UnitThemeParticle::load(both_true);
    TL_CHECK(p3.attaches);
    TL_CHECK(p3.follows);

    // 4. Custom authored values:
    AdmSection custom_part;
    custom_part.name = "PARTICLE";
    custom_part.records.push_back(str("FILE", "particles/spark.layout"));
    custom_part.records.push_back(integer("COUNT", 4));
    custom_part.records.push_back(str("UNITTYPE", "MONSTER"));
    custom_part.records.push_back(str("BONE", "Bip01 Head"));
    UnitThemeParticle p4 = UnitThemeParticle::load(custom_part);
    TL_CHECK_EQ(p4.file, "particles/spark.layout");
    TL_CHECK_EQ(p4.count, 4u);
    TL_CHECK_EQ(p4.unit_type, "MONSTER");
    TL_CHECK_EQ(p4.bone, "Bip01 Head");
}

TL_TEST(unit_theme_add_refuses_a_duplicate) {
    UnitThemeBook book;
    book.reload(std::vector<AdmFile>{make_theme_file("FIRE", 1)});
    const UnitTheme* fire = book.get_unit_theme("FIRE");

    UnitThemeSet set;
    TL_CHECK(set.add_unit_theme(fire));
    TL_CHECK_EQ(set.own_themes().size(), 1u);
    TL_CHECK_EQ(set.added_themes().size(), 1u);

    // Second add of the same theme is refused:
    TL_CHECK(!set.add_unit_theme(fire));
    TL_CHECK_EQ(set.own_themes().size(), 1u);
    TL_CHECK_EQ(set.added_themes().size(), 1u);
}

TL_TEST(unit_theme_remove_swaps_the_last_into_the_hole) {
    UnitThemeBook book;
    book.reload(std::vector<AdmFile>{
        make_theme_file("ONE", 1),
        make_theme_file("TWO", 2),
        make_theme_file("THREE", 3),
    });
    const UnitTheme* t1 = book.get_unit_theme("ONE");
    const UnitTheme* t2 = book.get_unit_theme("TWO");
    const UnitTheme* t3 = book.get_unit_theme("THREE");

    UnitThemeSet set;
    set.add_unit_theme(t1);
    set.add_unit_theme(t2);
    set.add_unit_theme(t3);

    TL_CHECK_EQ(set.own_themes().size(), 3u);
    TL_CHECK_EQ(set.own_themes()[0], t1);
    TL_CHECK_EQ(set.own_themes()[1], t2);
    TL_CHECK_EQ(set.own_themes()[2], t3);

    // Remove the first theme (t1). Swap-with-last moves t3 into index 0.
    TL_CHECK(set.remove_unit_theme(t1));
    TL_CHECK_EQ(set.own_themes().size(), 2u);
    TL_CHECK_EQ(set.own_themes()[0], t3);
    TL_CHECK_EQ(set.own_themes()[1], t2);

    TL_CHECK_EQ(set.removed_themes().size(), 1u);
    TL_CHECK_EQ(set.removed_themes()[0], t1);
}

TL_TEST(unit_theme_add_after_remove_before_update) {
    UnitThemeBook book;
    book.reload(std::vector<AdmFile>{
        make_theme_file("THEME1", 1),
        make_theme_file("THEME2", 2),
    });
    const UnitTheme* t1 = book.get_unit_theme("THEME1");
    const UnitTheme* t2 = book.get_unit_theme("THEME2");

    UnitThemeSet set;
    set.add_unit_theme(t1);
    set.update({});  // clear pending lists, t1 is now held in own_

    TL_CHECK_EQ(set.own_themes().size(), 1u);
    TL_CHECK(set.added_themes().empty());
    TL_CHECK(set.removed_themes().empty());

    // Remove t1:
    TL_CHECK(set.remove_unit_theme(t1));
    TL_CHECK(set.own_themes().empty());
    TL_CHECK_EQ(set.removed_themes().size(), 1u);
    TL_CHECK_EQ(set.removed_themes()[0], t1);

    // Add t1 back before update: rescued from removed_ via swap-with-last
    TL_CHECK(set.add_unit_theme(t1));
    TL_CHECK(set.removed_themes().empty());
    TL_CHECK_EQ(set.added_themes().size(), 1u);
    TL_CHECK_EQ(set.added_themes()[0], t1);
    TL_CHECK_EQ(set.own_themes().size(), 1u);
    TL_CHECK_EQ(set.own_themes()[0], t1);

    // An add followed by a remove of the same theme before update:
    TL_CHECK(set.add_unit_theme(t2));
    TL_CHECK_EQ(set.added_themes().size(), 2u);
    TL_CHECK(set.remove_unit_theme(t2));
    // t2 was removed from added_ and removed from own_:
    TL_CHECK(!set.has_unit_theme(t2));
    TL_CHECK_EQ(set.added_themes().size(), 1u);
    TL_CHECK_EQ(set.added_themes()[0], t1);
}

TL_TEST(unit_theme_has_sees_own_and_effect_themes) {
    UnitThemeBook book;
    book.reload(std::vector<AdmFile>{
        make_theme_file("OWNED", 1),
        make_theme_file("EFFECT", 2),
        make_theme_file("NEITHER", 3),
    });
    const UnitTheme* owned = book.get_unit_theme("OWNED");
    const UnitTheme* effect = book.get_unit_theme("EFFECT");
    const UnitTheme* neither = book.get_unit_theme("NEITHER");

    UnitThemeSet set;
    set.add_unit_theme(owned);
    set.update({effect});  // effect passed from effect manager

    // Both are seen by has_unit_theme:
    TL_CHECK(set.has_unit_theme(owned));
    TL_CHECK(set.has_unit_theme(effect));
    TL_CHECK(!set.has_unit_theme(neither));

    // Effect theme is in effect_themes, NOT in own_themes:
    TL_CHECK_EQ(set.own_themes().size(), 1u);
    TL_CHECK_EQ(set.own_themes()[0], owned);
    TL_CHECK_EQ(set.effect_themes().size(), 1u);
    TL_CHECK_EQ(set.effect_themes()[0], effect);

    // remove_unit_theme only removes from own_, so it cannot remove an effect theme:
    TL_CHECK(!set.remove_unit_theme(effect));
    TL_CHECK(set.has_unit_theme(effect));
}

TL_TEST(unit_theme_by_unknown_name_stores_a_null_once) {
    // `CBaseUnit::addUnitTheme(const std::wstring&)` passes the miss on
    // as a null theme, and the pointer overload does not test for null.
    UnitThemeBook book;
    book.reload(std::vector<AdmFile>{make_theme_file("KNOWN", 1)});

    UnitThemeSet set;
    TL_CHECK(set.add_unit_theme("UNKNOWN", &book));
    TL_CHECK_EQ(set.own_themes().size(), 1u);
    TL_CHECK(set.own_themes()[0] == nullptr);
    TL_CHECK_EQ(set.added_themes().size(), 1u);
    // A second miss is the same null, refused as a duplicate.
    TL_CHECK(!set.add_unit_theme("OTHER", &book));
    TL_CHECK_EQ(set.own_themes().size(), 1u);
    // Any miss now answers yes, since it asks about the null.
    TL_CHECK(set.has_unit_theme("ANYTHING", &book));
    TL_CHECK(set.has_unit_theme(9999LL, &book));
    // The name is compared as given, not upper-cased.
    TL_CHECK(book.get_unit_theme("known") == nullptr);

    std::vector<const UnitTheme*> announced;
    set.update({}, [&announced](const UnitTheme* theme, bool) { announced.push_back(theme); });
    TL_CHECK_EQ(announced.size(), 1u);
    TL_CHECK(announced[0] == nullptr);

    TL_CHECK(set.remove_unit_theme(9999LL, &book));
    TL_CHECK(set.own_themes().empty());

    // Without a table the name overloads do nothing (the singleton-null return).
    UnitThemeSet bare;
    TL_CHECK(!bare.add_unit_theme("KNOWN", nullptr));
    TL_CHECK(bare.own_themes().empty());
}

TL_TEST(theme_book_skips_a_file_without_a_name) {
    UnitThemeBook book;
    book.reload(std::vector<AdmFile>{
        make_theme_file("FIRST", 1),
        make_theme_file("", 2),
        make_theme_file("third", 3),
    });
    TL_CHECK_EQ(book.size(), 2u);
    TL_CHECK_EQ(book.themes()[0].name, "FIRST");
    TL_CHECK_EQ(book.themes()[1].name, "THIRD");
    TL_CHECK(book.get_unit_theme(2LL) == nullptr);
}

TL_TEST(unit_theme_update_announces_removed_then_added_then_effect_themes) {
    UnitThemeBook book;
    book.reload(std::vector<AdmFile>{
        make_theme_file("T_REMOVED", 1),
        make_theme_file("T_ADDED", 2),
        make_theme_file("T_EFFECT", 3),
    });
    const UnitTheme* t_rem = book.get_unit_theme("T_REMOVED");
    const UnitTheme* t_add = book.get_unit_theme("T_ADDED");
    const UnitTheme* t_eff = book.get_unit_theme("T_EFFECT");

    UnitThemeSet set;
    set.add_unit_theme(t_rem);
    set.update({});  // established t_rem on own_

    // Now remove t_rem, add t_add, and supply t_eff from effects:
    set.remove_unit_theme(t_rem);
    set.add_unit_theme(t_add);

    std::vector<std::pair<std::string, bool>> events;
    set.update({t_eff}, [&events](const UnitTheme* theme, bool removed) {
        events.emplace_back(theme->name, removed);
    });

    // Announced in order: removed themes, added themes, effect themes.
    TL_CHECK_EQ(events.size(), 3u);
    TL_CHECK_EQ(events[0].first, "T_REMOVED");
    TL_CHECK_EQ(events[0].second, true);
    TL_CHECK_EQ(events[1].first, "T_ADDED");
    TL_CHECK_EQ(events[1].second, false);
    TL_CHECK_EQ(events[2].first, "T_EFFECT");
    TL_CHECK_EQ(events[2].second, false);

    // Pending lists are empty afterwards:
    TL_CHECK(set.added_themes().empty());
    TL_CHECK(set.removed_themes().empty());
    TL_CHECK_EQ(set.effect_themes().size(), 1u);
    TL_CHECK_EQ(set.effect_themes()[0], t_eff);

    // Next update drops t_eff from effects:
    events.clear();
    set.update({}, [&events](const UnitTheme* theme, bool removed) {
        events.emplace_back(theme->name, removed);
    });
    TL_CHECK_EQ(events.size(), 1u);
    TL_CHECK_EQ(events[0].first, "T_EFFECT");
    TL_CHECK_EQ(events[0].second, true);
    TL_CHECK(set.effect_themes().empty());
}

TL_TEST(unit_theme_init_reads_the_themes_block) {
    UnitThemeBook book;
    book.reload(std::vector<AdmFile>{
        make_theme_file("CHAMPION", 1),
        make_theme_file("TURNED", 2),
    });

    AdmSection root;
    root.name = "CHARACTER";
    AdmSection themes_child;
    themes_child.name = "THEMES";
    // Key names do not matter in unitInitThemes; only type 5 (String) or 8 (Translate):
    themes_child.records.push_back(str("THEME", "champion", AdmType::String));
    themes_child.records.push_back(str("ANYKEY", "turned", AdmType::Translate));
    themes_child.records.push_back(integer("IGNORED", 42));
    root.children.push_back(themes_child);

    UnitThemeSet set;
    set.init_themes(root, &book);

    TL_CHECK_EQ(set.own_themes().size(), 2u);
    TL_CHECK(set.has_unit_theme("CHAMPION", &book));
    TL_CHECK(set.has_unit_theme("TURNED", &book));
}

TL_TEST(effect_with_unittheme_gives_the_unit_its_theme) {
    UnitThemeBook book;
    book.reload(std::vector<AdmFile>{
        make_theme_file("BURNING", 10),
    });
    const UnitTheme* burning = book.get_unit_theme("BURNING");

    EffectManager manager;
    EffectManager::Context ctx;
    ctx.theme_book = &book;
    manager.set_context(ctx);

    Effect e1;
    e1.type = 0;
    e1.activation = EffectActivation::Dynamic;
    e1.unit_theme = "BURNING";

    Effect e2;
    e2.type = 1;
    e2.activation = EffectActivation::Dynamic;
    // e2 has no unit_theme

    manager.add_effect(e1);
    manager.calculate_effect_values();
    manager.add_effect(e2);
    manager.calculate_effect_values();

    // manager.themes() has the theme from e1, not duplicate or null from e2:
    TL_CHECK_EQ(manager.themes().size(), 1u);
    TL_CHECK_EQ(manager.themes()[0], burning);

    // UnitThemeSet update synchronizes effect manager's themes into +0x150:
    UnitThemeSet set;
    set.update(manager.themes());
    TL_CHECK(set.has_unit_theme(burning));

    // Clear effects empties themes:
    manager.clear_effects(true);
    TL_CHECK(manager.themes().empty());

    set.update(manager.themes());
    TL_CHECK(!set.has_unit_theme(burning));
}

TL_TEST(effect_themes_are_listed_in_effect_type_order) {
    // `CEffectManager::calculateEffectValues` walks types first; a type past 0x90
    // gives no theme.
    UnitThemeBook book;
    book.reload(std::vector<AdmFile>{
        make_theme_file("HIGH", 1),
        make_theme_file("LOW", 2),
        make_theme_file("OUTSIDE", 3),
    });

    EffectManager manager;
    EffectManager::Context ctx;
    ctx.theme_book = &book;
    manager.set_context(ctx);

    Effect high;
    high.type = 40;
    high.activation = EffectActivation::Dynamic;
    high.unit_theme = "HIGH";
    Effect low;
    low.type = 3;
    low.activation = EffectActivation::Passive;
    low.unit_theme = "LOW";
    Effect outside;
    outside.type = 0x91;
    outside.activation = EffectActivation::Dynamic;
    outside.unit_theme = "OUTSIDE";
    Effect low_again = low;
    low_again.activation = EffectActivation::Dynamic;

    manager.add_effect(high);
    manager.calculate_effect_values();
    manager.add_effect(outside);
    manager.calculate_effect_values();
    manager.add_effect(low);
    manager.calculate_effect_values();
    manager.add_effect(low_again);
    manager.calculate_effect_values();

    TL_CHECK_EQ(manager.themes().size(), 2u);
    TL_CHECK_EQ(manager.themes()[0], book.get_unit_theme("LOW"));
    TL_CHECK_EQ(manager.themes()[1], book.get_unit_theme("HIGH"));
}

namespace {

using tl::core::ThemeParticleStep;
using tl::core::ThemeParticleTriggers;
using tl::core::ThemeParticleUnit;

UnitTheme theme_with(const std::vector<UnitThemeParticle>& particles,
                     const std::string& name = "T") {
    UnitTheme theme;
    theme.name = name;
    theme.particles = particles;
    return theme;
}

UnitThemeParticle theme_particle(const std::string& file, uint32_t count, const std::string& type,
                                 const std::string& bone = "") {
    UnitThemeParticle particle;
    particle.file = file;
    particle.count = count;
    particle.unit_type = type;
    particle.bone = bone;
    return particle;
}

/// A character with `bones` bones called B0, B1, ...; every particle file is
/// found, and handed out as 0, 1, 2, ...
ThemeParticleUnit character_with_bones(uint16_t bones, int* next_handle,
                                       std::vector<std::string>* files = nullptr) {
    ThemeParticleUnit unit;
    unit.isa = [](const std::string& type) { return type == "CHARACTER" || type == "ANY"; };
    unit.has_skeleton = true;
    unit.bone_count = bones;
    unit.bone_name = [bones](uint16_t i) {
        return i < bones ? "B" + std::to_string(i) : std::string();
    };
    unit.has_bone = [bones](const std::string& name) {
        for (uint16_t i = 0; i < bones; ++i) {
            if (name == "B" + std::to_string(i)) return true;
        }
        return false;
    };
    unit.get_particle = [next_handle, files](const std::string& file) {
        if (files != nullptr) files->push_back(file);
        return (*next_handle)++;
    };
    return unit;
}

}  // namespace

TL_TEST(theme_particles_are_made_per_unit_type_and_count) {
    // `createSpecificUnitThemeParticles`: `UNITTYPE` gates, `COUNT` 0 makes one,
    // the file upper-cased.
    const UnitTheme theme = theme_with({theme_particle("media/a.layout", 3, "CHARACTER"),
                                        theme_particle("media/b.layout", 1, "MONSTER"),
                                        theme_particle("media/c.layout", 0, "ANY")});
    int handle = 0;
    std::vector<std::string> files;
    const ThemeParticleUnit unit = character_with_bones(0, &handle, &files);
    tl::core::RunicRandom rng(1);
    ThemeParticleTriggers triggers;
    const std::vector<ThemeParticleStep> steps =
        triggers.unit_theme_updated(&theme, false, unit, rng);

    TL_CHECK_EQ(triggers.triggers().size(), 4u);
    TL_CHECK_EQ(steps.size(), 4u);
    TL_CHECK_EQ(files[0], std::string("MEDIA/A.LAYOUT"));
    TL_CHECK_EQ(files[3], std::string("MEDIA/C.LAYOUT"));
    for (const ThemeParticleStep& step : steps) TL_CHECK(step.start);
    // No bone authored: the particle rides the unit's node.
    TL_CHECK(!triggers.triggers()[0].bone_found);
    TL_CHECK(triggers.triggers()[0].attaches);
}

TL_TEST(theme_particle_every_bone_takes_one_bone_each_by_index) {
    const UnitTheme theme = theme_with({theme_particle("x", 10, "CHARACTER", "Every Bone")});
    int handle = 0;
    tl::core::RunicRandom rng(1);
    ThemeParticleTriggers triggers;
    triggers.unit_theme_updated(&theme, false, character_with_bones(4, &handle), rng);

    // `COUNT` is replaced by the skeleton's bone count, and no draw is made.
    TL_CHECK_EQ(triggers.triggers().size(), 4u);
    for (size_t i = 0; i < 4; ++i) {
        TL_CHECK(triggers.triggers()[i].bone_found);
        TL_CHECK_EQ(triggers.triggers()[i].bone, "B" + std::to_string(i));
    }
    tl::core::RunicRandom untouched(1);
    TL_CHECK_EQ(rng.between(0.0f, 1.0f), untouched.between(0.0f, 1.0f));
}

TL_TEST(theme_particle_random_bone_draws_each_on_the_volatile_stream) {
    // `configureParticleTrigger`: `randomBetweenVolatile(0, bones - 1)`,
    // truncated, once per trigger that got a particle.
    const UnitTheme theme = theme_with({theme_particle("x", 3, "CHARACTER", "Random")});
    int handle = 0;
    tl::core::RunicRandom rng(12345);
    tl::core::RunicRandom mirror(12345);
    ThemeParticleTriggers triggers;
    triggers.unit_theme_updated(&theme, false, character_with_bones(7, &handle), rng);

    TL_CHECK_EQ(triggers.triggers().size(), 3u);
    for (const tl::core::ThemeParticleTrigger& trigger : triggers.triggers()) {
        const int pick = static_cast<int>(mirror.between(0.0f, 6.0f));
        TL_CHECK(trigger.bone_found);
        TL_CHECK_EQ(trigger.bone, "B" + std::to_string(pick));
    }
}

TL_TEST(theme_particle_on_a_missing_bone_rides_the_unit) {
    const UnitTheme theme = theme_with({theme_particle("x", 1, "CHARACTER", "Bip01 Head")});
    int handle = 0;
    tl::core::RunicRandom rng(1);
    ThemeParticleTriggers triggers;
    triggers.unit_theme_updated(&theme, false, character_with_bones(3, &handle), rng);

    TL_CHECK_EQ(triggers.triggers().size(), 1u);
    TL_CHECK(!triggers.triggers()[0].bone_found);
    TL_CHECK_EQ(triggers.triggers()[0].bone, std::string("Bip01 Head"));
}

TL_TEST(theme_particles_are_made_once_then_stopped_and_restarted) {
    // `unitThemeUpdated`: a theme with a trigger that has a particle
    // is not made again; removing it stops those, adding it again starts them.
    const UnitTheme theme = theme_with({theme_particle("x", 2, "CHARACTER")});
    const UnitTheme other = theme_with({theme_particle("y", 1, "CHARACTER")}, "OTHER");
    int handle = 0;
    const ThemeParticleUnit unit = character_with_bones(0, &handle);
    tl::core::RunicRandom rng(1);
    ThemeParticleTriggers triggers;
    triggers.unit_theme_updated(&theme, false, unit, rng);
    triggers.unit_theme_updated(&other, false, unit, rng);
    TL_CHECK_EQ(triggers.triggers().size(), 3u);

    std::vector<ThemeParticleStep> steps = triggers.unit_theme_updated(&theme, true, unit, rng);
    TL_CHECK_EQ(steps.size(), 2u);
    TL_CHECK_EQ(steps[0].trigger, 0u);
    TL_CHECK(!steps[0].start);
    TL_CHECK(!steps[1].start);

    steps = triggers.unit_theme_updated(&theme, false, unit, rng);
    TL_CHECK_EQ(triggers.triggers().size(), 3u);
    TL_CHECK_EQ(steps.size(), 2u);
    TL_CHECK(steps[0].start);
    TL_CHECK_EQ(handle, 3);
}

TL_TEST(theme_particle_the_preloader_lacks_leaves_an_empty_trigger) {
    // The trigger is appended before `GetParticle`; without a particle it is not
    // configured (no bone draw) and never started, and the next add tries again.
    const UnitTheme theme = theme_with({theme_particle("x", 1, "CHARACTER", "Random")});
    int handle = 0;
    ThemeParticleUnit unit = character_with_bones(5, &handle);
    unit.get_particle = [](const std::string&) { return -1; };
    tl::core::RunicRandom rng(9);
    ThemeParticleTriggers triggers;
    TL_CHECK(triggers.unit_theme_updated(&theme, false, unit, rng).empty());
    TL_CHECK(triggers.unit_theme_updated(&theme, false, unit, rng).empty());
    TL_CHECK_EQ(triggers.triggers().size(), 2u);
    TL_CHECK_EQ(triggers.triggers()[0].particle, -1);
    tl::core::RunicRandom untouched(9);
    TL_CHECK_EQ(rng.between(0.0f, 1.0f), untouched.between(0.0f, 1.0f));

    // A null theme (a name the table lacks) makes nothing.
    TL_CHECK(triggers.unit_theme_updated(nullptr, false, unit, rng).empty());
    TL_CHECK_EQ(triggers.triggers().size(), 2u);
}

namespace {

tl::core::AnimationTag model_key(const std::string& name, const std::string& layout,
                                 const std::string& bone, uint32_t count = 1) {
    tl::core::AnimationTag key;
    key.name = name;
    key.layout = layout;
    key.bone = bone;
    key.particle_count = count;
    return key;
}

}  // namespace

TL_TEST(model_keys_make_triggers_per_count_and_every_bone) {
    // `CCharacter::loadModel`: types 4 and 5 only; `PARTICLECOUNT` triggers, or one
    // per bone for `Every Bone`.
    UnitThemeBook book;
    book.reload({make_theme_file("FIRETHEMESKILL", 7)});
    tl::core::AnimationTag glow =
        model_key("SPAWNPARTICLE_STOP_ON_DEATH", "media\\glow.layout", "B1");
    glow.offset_x = 0.5f;
    glow.offset_y = 1.5f;
    glow.unit_theme = "FIRETHEMESKILL";
    const std::vector<tl::core::AnimationTag> keys = {
        glow,
        model_key("PLAYSOUND", "", ""),
        model_key("SPAWNPARTICLE", "media/torch.layout", "", 2),
        model_key("SPAWNPARTICLE", "media/all.layout", "Every Bone", 9),
    };
    int handle = 0;
    std::vector<std::string> files;
    tl::core::RunicRandom rng(3);
    ThemeParticleTriggers triggers;
    triggers.load_model(keys, true, &book, character_with_bones(3, &handle, &files), rng);

    TL_CHECK_EQ(triggers.triggers().size(), 6u);
    TL_CHECK_EQ(files[0], std::string("MEDIA\\GLOW.LAYOUT"));
    const tl::core::ThemeParticleTrigger& first = triggers.triggers()[0];
    TL_CHECK(first.bind);
    TL_CHECK(first.stop_on_death);
    TL_CHECK(first.bone_found);
    TL_CHECK_EQ(first.offset_y, 1.5f);
    TL_CHECK(first.theme == book.get_unit_theme("FIRETHEMESKILL"));
    // No bone: the unit's own node; not on death.
    TL_CHECK(!triggers.triggers()[1].bone_found);
    TL_CHECK(!triggers.triggers()[1].stop_on_death);
    TL_CHECK(triggers.triggers()[1].theme == nullptr);
    for (size_t i = 0; i < 3; ++i) {
        TL_CHECK_EQ(triggers.triggers()[3 + i].bone, "B" + std::to_string(i));
    }
}

TL_TEST(model_key_count_reads_particlecount_above_zero) {
    // `CKeyframe::CKeyframe`: `PARTICLECOUNT`, 1 unless above 0.
    tl::formats::AdmFile file;
    file.ok = true;
    file.root.name = "ANIMATIONS";
    AdmSection animation;
    animation.name = "ANIMATION";
    animation.records.push_back(str("FILE", "media/models/x/x.skeleton"));
    for (uint32_t count : {3u, 0u, 0xffffffffu}) {
        AdmSection key;
        key.name = "KEY";
        key.records.push_back(str("NAME", "SPAWNPARTICLE"));
        key.records.push_back(integer("PARTICLECOUNT", count));
        animation.children.push_back(key);
    }
    AdmSection plain;
    plain.name = "KEY";
    plain.records.push_back(str("NAME", "SPAWNPARTICLE"));
    animation.children.push_back(plain);
    file.root.children.push_back(animation);
    const tl::core::AnimationManifest manifest = tl::core::AnimationManifest::from(file);
    const std::vector<tl::core::AnimationTag>* tags = manifest.tags_of_file("X.MESH");
    TL_CHECK(tags != nullptr);
    TL_CHECK_EQ(tags->size(), 4u);
    TL_CHECK_EQ((*tags)[0].particle_count, 3u);
    TL_CHECK_EQ((*tags)[1].particle_count, 1u);
    TL_CHECK_EQ((*tags)[2].particle_count, 1u);
    TL_CHECK_EQ((*tags)[3].particle_count, 1u);
    // Exact files only: no fall back to an animation state.
    TL_CHECK(manifest.tags_of_file("x1.skeleton") == nullptr);
}

TL_TEST(bind_particles_start_when_shown_and_stop_when_hidden) {
    // `CCharacter::setVisible(bool, bool)`.
    UnitThemeBook book;
    book.reload({make_theme_file("ICETHEMESKILL", 9)});
    tl::core::AnimationTag themed = model_key("SPAWNPARTICLE_STOP_ON_DEATH", "a", "B0");
    themed.unit_theme = "ICETHEMESKILL";
    const std::vector<tl::core::AnimationTag> keys = {
        model_key("SPAWNPARTICLE_STOP_ON_DEATH", "body", "B0"),
        model_key("SPAWNPARTICLE", "glow", "B0"),
        themed,
    };
    int handle = 0;
    tl::core::RunicRandom rng(1);
    ThemeParticleTriggers triggers;
    triggers.load_model(keys, true, &book, character_with_bones(1, &handle), rng);
    const auto none = [](const UnitTheme*) { return false; };

    std::vector<ThemeParticleStep> steps = triggers.set_visible(true, true, 0, none);
    TL_CHECK_EQ(steps.size(), 2u);
    TL_CHECK(steps[0].start && steps[1].start);
    TL_CHECK_EQ(steps[1].trigger, 1u);

    // The themed one runs only while the unit has its theme.
    steps = triggers.set_visible(true, true, 0, [](const UnitTheme*) { return true; });
    TL_CHECK_EQ(steps.size(), 3u);

    steps = triggers.set_visible(false, true, 0, none);
    TL_CHECK_EQ(steps.size(), 2u);
    TL_CHECK(!steps[0].start && steps[0].at_once && !steps[0].park);

    // Dying (AI state 5 or 6): what stops on death is stopped even when shown.
    steps = triggers.set_visible(true, false, 6, none);
    TL_CHECK_EQ(steps.size(), 2u);
    TL_CHECK(!steps[0].start && !steps[0].at_once);
    TL_CHECK(steps[1].start);

    // `die` stops every trigger that stops on death, the themed one too.
    steps = triggers.die();
    TL_CHECK_EQ(steps.size(), 2u);
    TL_CHECK_EQ(steps[1].trigger, 2u);

    // Keys of an animation that is not `BIND` are not worn.
    ThemeParticleTriggers other;
    other.load_model(keys, false, &book, character_with_bones(1, &handle), rng);
    TL_CHECK(other.set_visible(true, true, 0, none).empty());
}

TL_TEST(following_theme_particles_are_parked_when_hidden) {
    // a trigger that `FOLLOWS` and is not `BIND`'s is put out of sight
    // when the unit is hidden, and left alone when it is shown.
    UnitThemeParticle loose = theme_particle("x", 1, "CHARACTER");
    loose.attaches = false;
    loose.follows = true;
    const UnitTheme theme = theme_with({loose, theme_particle("y", 1, "CHARACTER")});
    int handle = 0;
    tl::core::RunicRandom rng(1);
    ThemeParticleTriggers triggers;
    triggers.unit_theme_updated(&theme, false, character_with_bones(0, &handle), rng);
    const auto all = [](const UnitTheme*) { return true; };
    TL_CHECK(triggers.set_visible(true, true, 0, all).empty());
    std::vector<ThemeParticleStep> steps = triggers.set_visible(false, true, 0, all);
    TL_CHECK_EQ(steps.size(), 2u);
    TL_CHECK(steps[0].park);
    TL_CHECK_EQ(steps[0].trigger, 0u);
    // hidden at once, the other trigger, which attaches and
    // does not loop, is stopped at once...
    TL_CHECK_EQ(steps[1].trigger, 1u);
    TL_CHECK(!steps[1].start && steps[1].at_once && !steps[1].park);
    // ... unless its particle loops; a fade touches neither.
    steps = triggers.set_visible(false, true, 0, all, [](int) { return true; });
    TL_CHECK_EQ(steps.size(), 1u);
    TL_CHECK_EQ(triggers.set_visible(false, false, 0, all).size(), 1u);
    // A theme's particles stop on death.
    TL_CHECK_EQ(triggers.die().size(), 2u);
}

TL_TEST(stop_particles_takes_following_and_bind_triggers) {
    // `CCharacter::stopParticles(bool)`: a
    // trigger that follows (+0x59) or is a `BIND` key (+0x5a), with a particle.
    UnitThemeParticle loose = theme_particle("x", 1, "CHARACTER");
    loose.follows = true;
    const UnitTheme theme = theme_with({loose, theme_particle("y", 1, "CHARACTER")});
    int handle = 0;
    tl::core::RunicRandom rng(1);
    ThemeParticleTriggers triggers;
    triggers.unit_theme_updated(&theme, false, character_with_bones(0, &handle), rng);
    std::vector<ThemeParticleStep> steps = triggers.stop_particles(true);
    TL_CHECK_EQ(steps.size(), 1u);
    TL_CHECK_EQ(steps[0].trigger, 0u);
    TL_CHECK(!steps[0].start && steps[0].at_once && !steps[0].park);
    TL_CHECK(!triggers.stop_particles(false)[0].at_once);

    // The keys of a `BIND` skeleton are all taken; of another, none.
    const std::vector<tl::core::AnimationTag> keys = {model_key("SPAWNPARTICLE", "glow", "B0")};
    ThemeParticleTriggers bound;
    bound.load_model(keys, true, nullptr, character_with_bones(1, &handle), rng);
    TL_CHECK_EQ(bound.stop_particles(true).size(), 1u);
    ThemeParticleTriggers other;
    other.load_model(keys, false, nullptr, character_with_bones(1, &handle), rng);
    TL_CHECK(other.stop_particles(true).empty());
}
