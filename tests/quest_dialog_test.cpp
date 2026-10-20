// CQuestDialog::parseDialogTag's DIALOG collection; CQuestDialog::getDialog's pick.
#include "core/dialog.h"
#include "core/quest_book.h"
#include "core/random.h"
#include "formats/adm.h"
#include "test_support.h"

#include <string>
#include <vector>

TL_TEST(dialogue_collection_reads_all_dialog_entries_and_replaces_newlines) {
    tl::formats::AdmSection section;
    section.name = "PASSIVE";

    tl::formats::AdmRecord r1;
    r1.key = "DIALOG";
    r1.type = tl::formats::AdmType::String;
    r1.text = "First line\\nwith newline";
    section.records.push_back(r1);

    tl::formats::AdmRecord r2;
    r2.key = "SOUND";
    r2.type = tl::formats::AdmType::String;
    r2.text = "SOME_SOUND";
    section.records.push_back(r2);

    tl::formats::AdmRecord r3;
    r3.key = "DIALOG";
    r3.type = tl::formats::AdmType::String;
    r3.text = "Second line";
    section.records.push_back(r3);

    tl::formats::AdmRecord r4;
    r4.key = "DIALOG";
    r4.type = tl::formats::AdmType::Translate;
    r4.text = "Third line";
    section.records.push_back(r4);

    tl::formats::AdmRecord r5;
    r5.key = "UNITNAME";
    r5.type = tl::formats::AdmType::String;
    r5.text = "TOWNFOLK";
    section.records.push_back(r5);

    tl::core::DialogLine line = tl::core::parse_dialog_line(section);
    TL_CHECK_EQ(line.texts.size(), size_t{3});
    TL_CHECK_EQ(line.texts[0], std::string("First line\nwith newline"));
    TL_CHECK_EQ(line.texts[1], std::string("Second line"));
    TL_CHECK_EQ(line.texts[2], std::string("Third line"));
    TL_CHECK_EQ(line.sound, std::string("SOME_SOUND"));
    TL_CHECK_EQ(line.unit, std::string("TOWNFOLK"));
}

TL_TEST(one_string_takes_no_draw_with_or_without_quest) {
    tl::core::DialogLine line;
    line.texts = {"Only one line"};

    // Without quest
    tl::core::RunicRandom rng1(1234);
    const uint64_t state_before1 = rng1.state();
    const std::string picked1 = tl::core::get_dialog(line, nullptr, rng1);
    TL_CHECK_EQ(picked1, std::string("Only one line"));
    TL_CHECK_EQ(rng1.state(), state_before1);

    // With quest (even with seed != -1)
    tl::core::QuestDefinition def;
    def.name = "SoloQuest";
    tl::core::QuestEntry entry;
    entry.definition = &def;
    entry.seed = 99999;

    tl::core::RunicRandom rng2(1234);
    const uint64_t state_before2 = rng2.state();
    const std::string picked2 = tl::core::get_dialog(line, &entry, rng2);
    TL_CHECK_EQ(picked2, std::string("Only one line"));
    TL_CHECK_EQ(rng2.state(), state_before2);
}

TL_TEST(no_quest_takes_no_draw_and_returns_first_string) {
    tl::core::DialogLine line;
    line.texts = {"First option", "Second option", "Third option"};

    tl::core::RunicRandom rng(5678);
    const uint64_t state_before = rng.state();
    const std::string picked = tl::core::get_dialog(line, nullptr, rng);
    TL_CHECK_EQ(picked, std::string("First option"));
    TL_CHECK_EQ(rng.state(), state_before);
}

TL_TEST(seed_minus_one_draws_once_without_reseeding) {
    tl::core::DialogLine line;
    line.texts = {"Option 0", "Option 1", "Option 2"};

    tl::core::QuestDefinition def;
    def.name = "TownsfolkTalk";
    tl::core::QuestEntry entry;
    entry.definition = &def;
    entry.seed = -1;

    tl::core::RunicRandom rng1(4321);
    tl::core::RunicRandom rng2(4321);

    const int expected_idx = rng2.integer_between(1, 3) - 1;
    const std::string picked = tl::core::get_dialog(line, &entry, rng1);

    TL_CHECK_EQ(picked, line.texts[expected_idx]);
    TL_CHECK_EQ(rng1.state(), rng2.state());
}

TL_TEST(seeded_quest_reseeds_before_draw_producing_deterministic_choice) {
    tl::core::DialogLine line;
    line.texts = {"Option 0", "Option 1", "Option 2"};

    tl::core::QuestDefinition def;
    def.name = "GivenQuest";
    tl::core::QuestEntry entry;
    entry.definition = &def;
    entry.seed = 12345;

    tl::core::RunicRandom ref(1);
    ref.set_seed(12345);
    const int ref_idx = ref.integer_between(1, 3) - 1;
    const uint64_t expected_final_state = ref.state();

    // From starting seed 1111
    tl::core::RunicRandom rng1(1111);
    const std::string picked1 = tl::core::get_dialog(line, &entry, rng1);
    TL_CHECK_EQ(picked1, line.texts[ref_idx]);
    TL_CHECK_EQ(rng1.state(), expected_final_state);

    // From starting seed 9999
    tl::core::RunicRandom rng2(9999);
    const std::string picked2 = tl::core::get_dialog(line, &entry, rng2);
    TL_CHECK_EQ(picked2, line.texts[ref_idx]);
    TL_CHECK_EQ(rng2.state(), expected_final_state);
}

TL_TEST(picked_string_substitutes_quest_tags) {
    tl::core::DialogLine line;
    line.texts = {"Go to floor [DEPTH] for the quest."};

    tl::core::QuestDefinition def;
    def.name = "DepthQuest";
    tl::core::QuestEntry entry;
    entry.definition = &def;
    // `[DEPTH]` is the quest's floor (`CQuest +0x20`) plus 1, not the level
    // its rewards are sampled at (`+0x20c`).
    entry.level = 3;
    entry.depth = 6;
    entry.seed = 12345;

    tl::core::RunicRandom rng(777);
    const std::string picked = tl::core::get_dialog(line, &entry, rng);
    TL_CHECK_EQ(picked, std::string("Go to floor 7 for the quest."));
}

TL_TEST(default_constructed_quest_entry_seed_is_minus_one) {
    tl::core::QuestEntry entry;
    TL_CHECK_EQ(entry.seed, -1);
}

TL_TEST(empty_dialog_texts_returns_empty_string_divergence) {
    tl::core::DialogLine line;

    tl::core::QuestDefinition def;
    def.name = "EmptyQuest";
    tl::core::QuestEntry entry;
    entry.definition = &def;
    entry.seed = 12345;

    tl::core::RunicRandom rng(888);
    const uint64_t state_before = rng.state();
    const std::string picked = tl::core::get_dialog(line, &entry, rng);
    TL_CHECK(picked.empty());
    TL_CHECK_EQ(rng.state(), state_before);
}
