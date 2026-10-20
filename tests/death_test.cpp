// The death screen's figures (`CDieMenu::setOpen`/`onClick`), the gates they are
// read off, and the negative awards that take them.
#include "core/death.h"
#include "core/character.h"
#include "core/inventory.h"
#include "core/stats.h"
#include "test_support.h"

namespace {

tl::formats::AdmSection line(const std::string& name,
                             const std::vector<std::pair<float, float>>& points) {
    tl::formats::AdmSection section;
    section.name = "LINE";
    tl::formats::AdmRecord named;
    named.key = "NAME";
    named.type = tl::formats::AdmType::String;
    named.text = name;
    section.records.push_back(named);
    for (const auto& point : points) {
        tl::formats::AdmSection node;
        node.name = "POINT";
        tl::formats::AdmRecord x;
        x.key = "X";
        x.type = tl::formats::AdmType::Float;
        x.number = point.first;
        tl::formats::AdmRecord y;
        y.key = "Y";
        y.type = tl::formats::AdmType::Float;
        y.number = point.second;
        node.records.push_back(x);
        node.records.push_back(y);
        section.children.push_back(node);
    }
    return section;
}

// `media/graphs/stats/EXPERIENCEGATE` and `FAMEGATE`, first three points.
tl::core::StatBook gates() {
    tl::core::StatBook book;
    book.add(tl::core::kExperienceGateGraph,
             tl::core::StatGraph::from(
                 line("EXPERIENCEGATE", {{1.0f, 4000.0f}, {2.0f, 9412.0f}, {3.0f, 17404.0f}})));
    book.add(tl::core::kFameGateGraph,
             tl::core::StatGraph::from(
                 line("FAMEGATE", {{1.0f, 1600.0f}, {2.0f, 3760.0f}, {3.0f, 6960.0f}})));
    return book;
}

}  // namespace

TL_TEST(death_gates_are_zero_at_level_zero_and_clamp_to_the_points) {
    const tl::core::StatBook book = gates();
    TL_CHECK_EQ(tl::core::experience_gate(book, 0), 0);
    TL_CHECK_EQ(tl::core::experience_gate(book, 1), 4000);
    TL_CHECK_EQ(tl::core::experience_gate(book, 3), 17404);
    // Past the third control point the third is read.
    TL_CHECK_EQ(tl::core::experience_gate(book, 9), 17404);
    TL_CHECK_EQ(tl::core::fame_gate(book, 2), 3760);
}

TL_TEST(death_screen_figures_are_a_tenth_of_each_span) {
    const tl::core::StatBook book = gates();
    // Level 1, fame level 1: (4000 - 0) / 10 and (1600 - 0) / 10; 175 gold -> 17.
    tl::core::DeathPenalty f = tl::core::death_screen_figures(book, 1, 1, 175);
    TL_CHECK_EQ(f.experience, 400);
    TL_CHECK_EQ(f.fame, 160);
    TL_CHECK_EQ(f.gold, 17);
    // Level 3: (17404 - 9412) / 10 = 799 (integer division).
    f = tl::core::death_screen_figures(book, 3, 2, 9);
    TL_CHECK_EQ(f.experience, 799);
    TL_CHECK_EQ(f.fame, 216);
    TL_CHECK_EQ(f.gold, 0);
}

TL_TEST(death_penalty_takes_only_the_chosen_line) {
    const tl::core::StatBook book = gates();
    using tl::core::ResurrectChoice;
    tl::core::DeathPenalty p = tl::core::death_penalty(ResurrectChoice::Here, book, 2, 1, 500);
    TL_CHECK_EQ(p.experience, 541);
    TL_CHECK_EQ(p.fame, 160);
    TL_CHECK_EQ(p.gold, 0);
    p = tl::core::death_penalty(ResurrectChoice::Entrance, book, 2, 1, 500);
    TL_CHECK_EQ(p.experience, 0);
    TL_CHECK_EQ(p.fame, 0);
    TL_CHECK_EQ(p.gold, 50);
    p = tl::core::death_penalty(ResurrectChoice::Town, book, 2, 1, 500);
    TL_CHECK_EQ(p.experience + p.fame + p.gold, 0);
}

TL_TEST(death_screen_text_fills_the_tokens) {
    tl::core::DeathPenalty f;
    f.experience = 400;
    f.fame = 160;
    f.gold = 17;
    TL_CHECK_EQ(tl::core::resurrect_here_text(f),
                std::string("Resurrect here, but lose 400 Experience and 160 Fame."));
    TL_CHECK_EQ(tl::core::resurrect_entrance_text(f),
                std::string("Resurrect at the entrance to this floor, but lose 17 Gold."));
    TL_CHECK_EQ(tl::core::death_screen_title(true), std::string("Death"));
}

TL_TEST(negative_awards_floor_at_zero) {
    tl::core::Inventory bag;
    bag.set_gold(30);
    bag.give_gold(-50);
    TL_CHECK_EQ(bag.gold(), 0);
    bag.set_gold(2147483600);
    bag.give_gold(100);
    TL_CHECK_EQ(bag.gold(), 2147483647);

    const tl::core::StatBook book = gates();
    tl::core::CharacterProgression sheet;
    sheet.award_experience(300, book);
    sheet.award_experience(-400, book);
    TL_CHECK_EQ(sheet.experience(), uint64_t{0});
    sheet.award_fame(200, book);
    sheet.award_fame(-50, book);
    TL_CHECK_EQ(sheet.fame(), uint64_t{150});
}
