// The skills tab's two-column grid for a class file laid out on the PC's three columns.
#include "core/skill_grid.h"
#include "test_support.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

using tl::core::ClassSkill;

namespace {

ClassSkill at(const std::string& name, int pane, int row, int column) {
    ClassSkill s;
    s.name = name;
    s.pane = pane;
    s.row = row;
    s.column = column;
    return s;
}

const ClassSkill& named(const std::vector<ClassSkill>& skills, const std::string& name) {
    for (const ClassSkill& s : skills) {
        if (s.name == name) return s;
    }
    static const ClassSkill kNone;
    return kNone;
}

}  // namespace

TL_TEST(skill_grid_leaves_two_columns_alone) {
    std::vector<ClassSkill> skills = {at("a", 0, 0, 0), at("b", 0, 0, 1), at("c", 1, 5, 1)};
    bool repacked = true;
    TL_CHECK_EQ(tl::core::pack_two_column_skill_grid(skills, &repacked), 0u);
    TL_CHECK(!repacked);
    TL_CHECK_EQ(named(skills, "a").column, 0);
    TL_CHECK_EQ(named(skills, "b").column, 1);
    TL_CHECK_EQ(named(skills, "c").row, 5);
    TL_CHECK_EQ(named(skills, "c").column, 1);
}

TL_TEST(skill_grid_packs_rows_left) {
    // Columns 1 and 2 become 0 and 1; a lone skill in column 1 or 2 goes to 0.
    std::vector<ClassSkill> skills = {at("b", 0, 0, 2), at("a", 0, 0, 1), at("c", 0, 1, 2)};
    bool repacked = false;
    TL_CHECK_EQ(tl::core::pack_two_column_skill_grid(skills, &repacked), 0u);
    TL_CHECK(repacked);
    TL_CHECK_EQ(named(skills, "a").column, 0);
    TL_CHECK_EQ(named(skills, "b").column, 1);
    TL_CHECK_EQ(named(skills, "c").row, 1);
    TL_CHECK_EQ(named(skills, "c").column, 0);
}

TL_TEST(skill_grid_carries_a_full_rows_leftmost) {
    // Three in row 2: the leftmost waits for row 3 and goes first there.
    std::vector<ClassSkill> skills = {at("left", 0, 2, 0), at("mid", 0, 2, 1), at("right", 0, 2, 2),
                                      at("below", 0, 3, 1)};
    TL_CHECK_EQ(tl::core::pack_two_column_skill_grid(skills), 0u);
    TL_CHECK_EQ(named(skills, "mid").row, 2);
    TL_CHECK_EQ(named(skills, "mid").column, 0);
    TL_CHECK_EQ(named(skills, "right").row, 2);
    TL_CHECK_EQ(named(skills, "right").column, 1);
    TL_CHECK_EQ(named(skills, "left").row, 3);
    TL_CHECK_EQ(named(skills, "left").column, 0);
    TL_CHECK_EQ(named(skills, "below").row, 3);
    TL_CHECK_EQ(named(skills, "below").column, 1);
}

TL_TEST(skill_grid_carry_past_the_last_row) {
    // Three in the last row: the leftmost takes the lowest free cell above.
    std::vector<ClassSkill> skills = {at("x", 2, 5, 0), at("y", 2, 5, 1), at("z", 2, 5, 2),
                                      at("w", 2, 4, 2)};
    TL_CHECK_EQ(tl::core::pack_two_column_skill_grid(skills), 0u);
    TL_CHECK_EQ(named(skills, "y").row, 5);
    TL_CHECK_EQ(named(skills, "z").row, 5);
    TL_CHECK_EQ(named(skills, "w").row, 4);
    TL_CHECK_EQ(named(skills, "w").column, 0);
    TL_CHECK_EQ(named(skills, "x").row, 4);
    TL_CHECK_EQ(named(skills, "x").column, 1);
}

TL_TEST(skill_grid_full_pane_leaves_the_rest_off) {
    // Eighteen skills on a pane of twelve cells: six find none.
    std::vector<ClassSkill> skills;
    for (int row = 0; row < 6; ++row) {
        for (int column = 0; column < 3; ++column) {
            skills.push_back(
                at(std::to_string(row) + "/" + std::to_string(column), 1, row, column));
        }
    }
    TL_CHECK_EQ(tl::core::pack_two_column_skill_grid(skills), 6u);
    std::set<std::pair<int, int>> cells;
    int off = 0;
    for (const ClassSkill& s : skills) {
        if (s.row < 0) {
            TL_CHECK_EQ(s.column, -1);
            ++off;
            continue;
        }
        TL_CHECK(s.column == 0 || s.column == 1);
        TL_CHECK(cells.insert({s.row, s.column}).second);
    }
    TL_CHECK_EQ(off, 6);
    TL_CHECK_EQ(cells.size(), 12u);
}

TL_TEST(skill_grid_every_skill_its_own_cell) {
    // A PC-shaped pane: up to three a row, eleven skills - all placed, none shared.
    std::vector<ClassSkill> skills = {
        at("a", 0, 0, 1), at("b", 0, 0, 2), at("c", 0, 1, 0), at("d", 0, 1, 2),
        at("e", 0, 2, 0), at("f", 0, 2, 1), at("g", 0, 2, 2), at("h", 0, 3, 1),
        at("i", 0, 4, 0), at("j", 0, 4, 2), at("k", 0, 5, 1), at("off pane", 1, -1, -1)};
    TL_CHECK_EQ(tl::core::pack_two_column_skill_grid(skills), 0u);
    std::set<std::pair<int, int>> cells;
    for (const ClassSkill& s : skills) {
        if (s.pane != 0) continue;
        TL_CHECK(s.row >= 0 && s.row < 6);
        TL_CHECK(cells.insert({s.row, s.column}).second);
    }
    TL_CHECK_EQ(cells.size(), 11u);
    // A skill with no row stays off the tab.
    TL_CHECK_EQ(named(skills, "off pane").row, -1);
}
