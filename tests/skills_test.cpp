// The player's skill book and the console skill screen's cursor (`CSkillMenuXenon`).
#include "core/skills.h"
#include "test_support.h"

using tl::core::ClassSkill;
using tl::core::KnownSkill;
using tl::core::SkillActivation;
using tl::core::SkillBook;
using tl::core::SkillButton;
using tl::core::SkillCursor;
using tl::core::SkillDef;
using tl::core::SkillProperty;

namespace {

SkillDef make_def(const std::string& name, int levels, int64_t guid) {
    SkillDef def;
    def.name = name;
    def.display_name = name;
    def.guid = guid;
    for (int i = 0; i < levels; ++i) def.levels.push_back(SkillProperty{});
    return def;
}

ClassSkill entry(const std::string& name, int pane, int row, int column, uint32_t required,
                 uint32_t level = 0) {
    ClassSkill e;
    e.name = name;
    e.pane = pane;
    e.row = row;
    e.column = column;
    e.level_required = required;
    e.level = level;
    return e;
}

}  // namespace

/// `CSkill::getLevelRequiredForInvestment`: a level's own `LEVEL_REQUIRED`, else
/// the class entry's plus the points already in.
TL_TEST(skill_level_requirement_climbs_with_investment) {
    SkillDef def = make_def("Bolt", 10, 7);
    KnownSkill skill;
    skill.def = &def;
    skill.entry = entry("Bolt", 0, 0, 0, 5);
    TL_CHECK_EQ(SkillBook::level_required_for_investment(skill), 5u);
    skill.invested = 3;
    TL_CHECK_EQ(SkillBook::level_required_for_investment(skill), 8u);
    def.levels[3].level_required = 20;  // the fourth level authors its own
    TL_CHECK_EQ(SkillBook::level_required_for_investment(skill), 20u);
}

/// a point is spent only when the level is
/// reached, a point is held and the rank is not full.
TL_TEST(learning_a_skill_spends_a_point_and_respects_the_gates) {
    SkillDef def = make_def("Bolt", 2, 7);
    SkillBook book;
    book.set_class({entry("Bolt", 0, 0, 0, 5)}, [&](const std::string&) { return &def; });
    KnownSkill& skill = book.skills()[0];
    int points = 0;
    TL_CHECK(!book.learn(skill, 10, points));  // no points
    points = 3;
    TL_CHECK(!book.learn(skill, 4, points));  // level 5 needed
    TL_CHECK(book.learn(skill, 5, points));
    TL_CHECK_EQ(points, 2);
    TL_CHECK_EQ(skill.invested, 1u);
    TL_CHECK_EQ(skill.effective, 1u);
    TL_CHECK(!book.learn(skill, 5, points));  // the next level wants 6
    TL_CHECK(book.learn(skill, 6, points));
    TL_CHECK(!book.learn(skill, 60, points));  // two levels authored: the rank is full
    TL_CHECK_EQ(points, 1);
}

/// The class's `LEVEL` is where a skill starts (the Alchemist's Ember Bolt at 1).
TL_TEST(class_starting_level_is_invested) {
    SkillDef def = make_def("Bolt", 10, 7);
    SkillBook book;
    book.set_class({entry("Bolt", 0, 0, 0, 1, 1)}, [&](const std::string&) { return &def; });
    TL_CHECK_EQ(book.skills()[0].invested, 1u);
    TL_CHECK_EQ(book.skills()[0].effective, 1u);
    TL_CHECK_EQ(book.skills()[0].max_rank(), 10u);
}

/// `SKILL_REQUIRED` blocks learning until the named skill holds a level.
TL_TEST(skill_required_blocks_learning) {
    SkillDef a = make_def("A", 5, 1);
    SkillDef b = make_def("B", 5, 2);
    SkillBook book;
    ClassSkill needs = entry("B", 0, 1, 0, 1);
    needs.skill_required = "a";
    book.set_class({entry("A", 0, 0, 0, 1), needs},
                   [&](const std::string& n) { return n == "A" ? &a : &b; });
    int points = 2;
    TL_CHECK(!book.can_learn(book.skills()[1], 10, points));
    TL_CHECK(book.learn(book.skills()[0], 10, points));
    TL_CHECK(book.can_learn(book.skills()[1], 10, points));
}

/// The map handler: the function slot by button and
/// set, the slot beside it cleared, and only learned active skills.
TL_TEST(mapping_writes_the_function_slot_for_the_set) {
    TL_CHECK_EQ(tl::core::skill_button_slot(SkillButton::Y, 0), 3);
    TL_CHECK_EQ(tl::core::skill_button_slot(SkillButton::B, 0), 4);
    TL_CHECK_EQ(tl::core::skill_button_slot(SkillButton::LT, 0), 5);
    TL_CHECK_EQ(tl::core::skill_button_slot(SkillButton::RT, 0), 6);
    TL_CHECK_EQ(tl::core::skill_button_slot(SkillButton::Y, 1), 2);
    TL_CHECK_EQ(tl::core::skill_button_slot(SkillButton::B, 1), 7);
    TL_CHECK_EQ(tl::core::skill_button_slot(SkillButton::LT, 1), 8);
    TL_CHECK_EQ(tl::core::skill_button_slot(SkillButton::RT, 1), 9);

    SkillDef active = make_def("Bolt", 3, 77);
    SkillDef passive = make_def("Aura", 3, 88);
    passive.activation = SkillActivation::Passive;
    SkillBook book;
    book.set_class({entry("Bolt", 0, 0, 0, 1), entry("Aura", 0, 0, 1, 1, 1)},
                   [&](const std::string& n) { return n == "Bolt" ? &active : &passive; });
    TL_CHECK(!book.map(book.skills()[0], SkillButton::RT, 0));  // not learned
    TL_CHECK(!book.map(book.skills()[1], SkillButton::RT, 0));  // passive
    int points = 1;
    TL_CHECK(book.learn(book.skills()[0], 1, points));
    book.set_slots({}, {0, 0, 0, 0, 0, 0, 55, 0, 0, 0});
    TL_CHECK(book.map(book.skills()[0], SkillButton::RT, 0));
    TL_CHECK_EQ(book.slot(6), 77);
    TL_CHECK_EQ(book.second_slots()[6], -1);
}

/// `CPlayer::unitInit`: the first skill with a level, not
/// passive, enabled and not hidden goes on slot 6 (RT), clearing its item half.
TL_TEST(unit_init_puts_the_first_usable_skill_on_rt) {
    SkillDef bolt = make_def("Bolt", 3, 77);
    SkillDef aura = make_def("Aura", 3, 88);
    aura.activation = SkillActivation::Passive;
    SkillDef hide = make_def("Hide", 3, 66);
    hide.hidden = true;
    SkillDef lance = make_def("Lance", 3, 99);
    const auto lookup = [&](const std::string& n) -> const SkillDef* {
        if (n == "Bolt") return &bolt;
        if (n == "Aura") return &aura;
        if (n == "Hide") return &hide;
        if (n == "Lance") return &lance;
        return nullptr;
    };

    // The first has its starting level: it is taken.
    SkillBook book;
    book.set_class({entry("Bolt", 0, 0, 0, 1, 1), entry("Lance", 0, 1, 0, 1, 1)}, lookup);
    book.set_slots({}, {0, 0, 0, 0, 0, 0, 55, 0, 0, 0});
    const KnownSkill* active = book.bind_default_skill_slot();
    TL_CHECK(active != nullptr && active->guid() == 77);
    TL_CHECK_EQ(book.slot(6), 77);
    TL_CHECK_EQ(book.second_slots()[6], -1);
    TL_CHECK_EQ(tl::core::skill_button_slot(SkillButton::RT, 0), SkillBook::kDefaultSkillSlot);

    // `cycleSkill(1)` passes over no level, passive and hidden, and a skill no
    // file names is not in the manager's list at all.
    SkillBook cycled;
    cycled.set_class(
        {entry("Bolt", 0, 0, 0, 1, 0), entry("Missing", 0, 0, 1, 1, 1),
         entry("Aura", 0, 1, 0, 1, 1), entry("Hide", 0, 1, 1, 1, 1), entry("Lance", 0, 2, 0, 1, 1)},
        lookup);
    active = cycled.bind_default_skill_slot();
    TL_CHECK(active != nullptr && active->guid() == 99);
    TL_CHECK_EQ(cycled.slot(6), 99);

    // Back round to the first with nothing usable: the first is taken anyway.
    SkillBook none;
    none.set_class({entry("Bolt", 0, 0, 0, 1, 0), entry("Aura", 0, 1, 0, 1, 1)}, lookup);
    active = none.bind_default_skill_slot();
    TL_CHECK(active != nullptr && active->guid() == 77);
    TL_CHECK_EQ(none.slot(6), 77);

    // No skills: the slot is -1 and its item half is left alone.
    SkillBook empty;
    empty.set_slots({}, {0, 0, 0, 0, 0, 0, 55, 0, 0, 0});
    TL_CHECK(empty.bind_default_skill_slot() == nullptr);
    TL_CHECK_EQ(empty.slot(6), -1);
    TL_CHECK_EQ(empty.second_slots()[6], 55);
}

/// `CCharacter::canLearnSpell`, `learnSpell`: the first empty of four
/// slots (two for a pet), refused at the level already held.
TL_TEST(a_spell_is_learned_into_the_first_empty_slot) {
    SkillDef bolt = make_def("Bolt", 3, 77);
    SkillDef portal = make_def("Town Portal", 1, 500);
    SkillBook book;
    book.set_class({entry("Bolt", 0, 0, 0, 1, 1)}, [&](const std::string& n) -> const SkillDef* {
        return n == "Bolt" ? &bolt : nullptr;
    });

    TL_CHECK(book.can_learn_spell("Town Portal", 1, false));
    const KnownSkill* learned = book.learn_spell("Town Portal", 1, &portal);
    TL_CHECK(learned != nullptr && learned->spell && learned->invested == 1);
    TL_CHECK_EQ(book.spells[0], std::string("Town Portal"));
    TL_CHECK_EQ(book.spell_levels[0], 1);
    // The same name at the same level: refused, whatever its case.
    TL_CHECK(!book.can_learn_spell("TOWN PORTAL", 1, false));
    // and at another level the console's slot test refuses it
    // (a slot holding the name, compared exactly).
    TL_CHECK(!book.can_learn_spell("Town Portal", 2, false));
    // The class's levels are the class's alone.
    TL_CHECK_EQ(book.levels().size(), 1u);

    // A pet has two slots; anyone else four, and nothing when they are full.
    book.spells[1] = "B";
    TL_CHECK(!book.can_learn_spell("Heal", 1, true));
    TL_CHECK(book.can_learn_spell("Heal", 1, false));
    book.spells[2] = "C";
    book.spells[3] = "D";
    TL_CHECK(!book.can_learn_spell("Heal", 1, false));
    TL_CHECK(book.learn_spell("Heal", 1, &portal) == nullptr);

    // No file of that name: the slot is taken, no skill is added.
    SkillBook missing;
    TL_CHECK(missing.learn_spell("Nothing", 1, nullptr) == nullptr);
    TL_CHECK_EQ(missing.spells[0], std::string("Nothing"));
    TL_CHECK(missing.find_by_name("Nothing") == nullptr);

    // A save's spells are learned again from empty.
    book.clear_spells();
    TL_CHECK(book.spells[0].empty());
    TL_CHECK(book.find_by_name("Town Portal") == nullptr);
    TL_CHECK(book.find_by_name("Bolt") != nullptr);
}

/// The learn test without the slot check, `getKnownSpell`,
/// `unLearnSpell`: unlearning moves later slots up.
TL_TEST(a_full_book_passes_the_tray_test_and_a_spell_can_be_unlearned) {
    // Two levels: learned at 2 it stays 2 (`_setLevelOfSkillFromSkillManager`
    // holds a level to the skill's `[LEVELn]` count).
    SkillDef heal = make_def("Heal", 2, 600);
    SkillDef bolt = make_def("Bolt", 1, 601);
    SkillDef ward = make_def("Ward", 1, 602);
    SkillBook pet;
    TL_CHECK(pet.learn_spell("Heal", 2, &heal) != nullptr);
    TL_CHECK(pet.learn_spell("Bolt", 1, &bolt) != nullptr);
    TL_CHECK(!pet.can_learn_spell("Ward", 1, true));
    TL_CHECK(pet.can_learn_spell("Ward", 1, true, /*need_empty_slot=*/false));
    TL_CHECK(!pet.can_learn_spell("Heal", 1, true, false));
    // The slot's skill at the slot's level.
    TL_CHECK(pet.known_spell(0) != nullptr && pet.known_spell(0)->entry.name == "Heal");
    TL_CHECK(pet.known_spell(2) == nullptr);
    pet.unlearn_spell(0);
    TL_CHECK_EQ(pet.spells[0], std::string("Bolt"));
    TL_CHECK_EQ(pet.spell_levels[0], 1);
    TL_CHECK(pet.spells[1].empty());
    TL_CHECK_EQ(pet.spell_levels[3], 0);
    TL_CHECK(pet.find_by_name("Heal") == nullptr);
    TL_CHECK(pet.find_by_name("Bolt") != nullptr);
    TL_CHECK(pet.can_learn_spell("Ward", 1, true));
    TL_CHECK(pet.learn_spell("Ward", 1, &ward) != nullptr);
    TL_CHECK_EQ(pet.spells[1], std::string("Ward"));
    // An empty slot, or one past the fourth: nothing.
    pet.unlearn_spell(3);
    pet.unlearn_spell(9);
    TL_CHECK_EQ(pet.spells[1], std::string("Ward"));
}

/// `CPlayer::resetSkills`: the points in enabled, visible type-0
/// skills come back, less the class's starting levels, which are set again.
TL_TEST(respec_refunds_the_points_and_restores_the_starting_levels) {
    SkillDef bolt = make_def("Bolt", 10, 77);
    SkillDef aura = make_def("Aura", 10, 88);
    SkillDef hide = make_def("Hide", 10, 66);
    hide.hidden = true;
    SkillDef portal = make_def("Town Portal", 1, 500);
    portal.type = tl::core::SkillType::Defensive;
    SkillBook book;
    book.set_class(
        {entry("Bolt", 0, 0, 0, 1, 1), entry("Aura", 0, 1, 0, 1, 0), entry("Hide", 0, 2, 0, 1, 2)},
        [&](const std::string& n) -> const SkillDef* {
            if (n == "Bolt") return &bolt;
            if (n == "Aura") return &aura;
            if (n == "Hide") return &hide;
            return nullptr;
        });
    book.find_by_name("Bolt")->invested = 5;
    book.find_by_name("Aura")->invested = 3;
    book.find_by_name("Hide")->invested = 4;
    book.learn_spell("Town Portal", 1, &portal);

    // 5 + 3 counted (the hidden one and the spell are not), less the starting
    // levels 1 + 0 + 2.
    TL_CHECK_EQ(book.reset_skills(), 5);
    TL_CHECK_EQ(book.find_by_name("Bolt")->invested, 1u);
    TL_CHECK_EQ(book.find_by_name("Aura")->invested, 0u);
    // A hidden skill is not zeroed, but its `LEVEL` is set again by name.
    TL_CHECK_EQ(book.find_by_name("Hide")->invested, 2u);
    TL_CHECK_EQ(book.find_by_name("Town Portal")->invested, 1u);
    // Nothing held beyond the start: nothing back, never less than 0.
    TL_CHECK_EQ(book.reset_skills(), 0);
}

/// right and left run across the three panes and wrap.
TL_TEST(skill_cursor_crosses_panes_and_wraps) {
    SkillCursor c;
    tl::core::skill_cursor_right(c, nullptr);
    TL_CHECK(c.pane == 0 && c.column == 1);
    tl::core::skill_cursor_right(c, nullptr);
    TL_CHECK(c.pane == 1 && c.column == 0);
    c.pane = 2;
    c.column = 1;
    tl::core::skill_cursor_right(c, nullptr);
    TL_CHECK(c.pane == 0 && c.column == 0);
    tl::core::skill_cursor_left(c, nullptr);
    TL_CHECK(c.pane == 2 && c.column == 1);
    TL_CHECK_EQ(c.index(), (2 * 6 + 0) * 2 + 1);
}

/// past the top or bottom row the cursor enters the
/// spell row under its column, and leaves it back into the grid.
TL_TEST(skill_cursor_enters_and_leaves_the_spell_row) {
    SkillCursor c;
    c.pane = 1;
    c.column = 1;
    tl::core::skill_cursor_up(c, nullptr);
    TL_CHECK(c.in_spells);
    TL_CHECK_EQ(c.spell, 2);
    TL_CHECK_EQ(c.index(), 38);
    tl::core::skill_cursor_up(c, nullptr);
    TL_CHECK(!c.in_spells);
    TL_CHECK(c.pane == 1 && c.column == 1 && c.row == 5);
    tl::core::skill_cursor_down(c, nullptr);
    TL_CHECK(c.in_spells);
    tl::core::skill_cursor_down(c, nullptr);
    TL_CHECK(!c.in_spells);
    TL_CHECK_EQ(c.row, 0);
    // The spell row wraps on its own.
    c.in_spells = true;
    c.spell = 3;
    tl::core::skill_cursor_right(c, nullptr);
    TL_CHECK_EQ(c.spell, 0);
    tl::core::skill_cursor_left(c, nullptr);
    TL_CHECK_EQ(c.spell, 3);
}

/// A step that lands on an empty cell takes another.
TL_TEST(skill_cursor_skips_empty_cells) {
    auto shown = [](const SkillCursor& c) { return c.column == 0 && c.row % 2 == 0; };
    SkillCursor c;
    tl::core::skill_cursor_down(c, shown);
    TL_CHECK_EQ(c.row, 2);
    tl::core::skill_cursor_right(c, shown);
    TL_CHECK(c.pane == 1 && c.column == 0);
    // Nothing shown anywhere: the step still ends.
    SkillCursor d;
    tl::core::skill_cursor_right(d, [](const SkillCursor&) { return false; });
}
