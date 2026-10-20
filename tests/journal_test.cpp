#include "core/journal.h"
#include "core/character.h"
#include "formats/save_file.h"
#include "test_support.h"

#include <vector>

namespace {

TL_TEST(journal_increment_adds_to_the_slot) {
    tl::core::JournalStatistics stats;

    // Initially zero
    for (size_t i = 0; i < tl::core::kJournalStatisticCount; ++i) {
        TL_CHECK_EQ(stats.value(static_cast<tl::core::JournalStatistic>(i)), 0);
    }

    // Increment adds
    stats.increment(tl::core::JournalStatistic::GoldGathered, 500);
    TL_CHECK_EQ(stats.value(tl::core::JournalStatistic::GoldGathered), 500);

    // Negative amount subtracts
    stats.increment(tl::core::JournalStatistic::GoldGathered, -150);
    TL_CHECK_EQ(stats.value(tl::core::JournalStatistic::GoldGathered), 350);

    // Set overwrites
    stats.set(tl::core::JournalStatistic::GoldGathered, 1000);
    TL_CHECK_EQ(stats.value(tl::core::JournalStatistic::GoldGathered), 1000);

    // The other 17 slots are untouched
    for (size_t i = 0; i < tl::core::kJournalStatisticCount; ++i) {
        const auto stat = static_cast<tl::core::JournalStatistic>(i);
        if (stat != tl::core::JournalStatistic::GoldGathered) {
            TL_CHECK_EQ(stats.value(stat), 0);
        }
    }
}

TL_TEST(journal_counts_only_for_a_player) {
    tl::core::JournalStatistics stats;

    // Non-player call does not increment (gate check)
    // CCharacter::incrementJournalStatistic
    tl::core::increment_journal_statistic(stats, /*is_player=*/false,
                                          tl::core::JournalStatistic::MonstersDefeated, 1);
    TL_CHECK_EQ(stats.value(tl::core::JournalStatistic::MonstersDefeated), 0);

    // Player call increments
    tl::core::increment_journal_statistic(stats, /*is_player=*/true,
                                          tl::core::JournalStatistic::MonstersDefeated, 1);
    TL_CHECK_EQ(stats.value(tl::core::JournalStatistic::MonstersDefeated), 1);
}

TL_TEST(journal_enum_matches_the_binary) {
    // Verified against binary call addresses and CJournalMenu::updateLayout
    TL_CHECK_EQ(static_cast<int32_t>(tl::core::JournalStatistic::TimePlayed), 0);
    TL_CHECK_EQ(static_cast<int32_t>(tl::core::JournalStatistic::GoldGathered), 1);
    TL_CHECK_EQ(static_cast<int32_t>(tl::core::JournalStatistic::LevelsExplored), 2);
    TL_CHECK_EQ(static_cast<int32_t>(tl::core::JournalStatistic::StepsTaken), 3);
    TL_CHECK_EQ(static_cast<int32_t>(tl::core::JournalStatistic::QuestsCompleted), 4);
    TL_CHECK_EQ(static_cast<int32_t>(tl::core::JournalStatistic::Deaths), 5);
    TL_CHECK_EQ(static_cast<int32_t>(tl::core::JournalStatistic::MonstersDefeated), 6);
    TL_CHECK_EQ(static_cast<int32_t>(tl::core::JournalStatistic::ChampionsDefeated), 7);
    TL_CHECK_EQ(static_cast<int32_t>(tl::core::JournalStatistic::SkillsCast), 8);
    TL_CHECK_EQ(static_cast<int32_t>(tl::core::JournalStatistic::ChestsOpened), 9);
    TL_CHECK_EQ(static_cast<int32_t>(tl::core::JournalStatistic::TrapsSprung), 10);
    TL_CHECK_EQ(static_cast<int32_t>(tl::core::JournalStatistic::BarrelsBroken), 11);
    TL_CHECK_EQ(static_cast<int32_t>(tl::core::JournalStatistic::PotionsUsed), 12);
    TL_CHECK_EQ(static_cast<int32_t>(tl::core::JournalStatistic::PortalsUsed), 13);
    TL_CHECK_EQ(static_cast<int32_t>(tl::core::JournalStatistic::FishCaught), 14);
    TL_CHECK_EQ(static_cast<int32_t>(tl::core::JournalStatistic::TimesGambled), 15);
    TL_CHECK_EQ(static_cast<int32_t>(tl::core::JournalStatistic::ItemsTransmuted), 16);
    TL_CHECK_EQ(static_cast<int32_t>(tl::core::JournalStatistic::ItemsEnchanted), 17);

    TL_CHECK_EQ(tl::core::kJournalStatisticCount, size_t{18});
}

TL_TEST(journal_save_round_trip) {
    tl::core::JournalStatistics original;
    for (size_t i = 0; i < tl::core::kJournalStatisticCount; ++i) {
        original.set(static_cast<tl::core::JournalStatistic>(i), static_cast<int32_t>(i * 17 + 3));
    }
    // Distinct values, with one negative value
    original.set(tl::core::JournalStatistic::Deaths, -7);

    tl::formats::CharacterFile file;
    tl::core::write_journal(original, file.journal);
    // Slot order in the file: slot i at index i, the negative as its bit pattern.
    TL_CHECK_EQ(file.journal[1], uint32_t{20});
    TL_CHECK_EQ(file.journal[5], uint32_t{0xFFFFFFF9u});
    TL_CHECK_EQ(file.journal[17], uint32_t{17 * 17 + 3});

    const std::vector<uint8_t> bytes = tl::formats::write_character_file(file);
    tl::formats::CharacterFile loaded_file;
    TL_CHECK(tl::formats::read_character_file(bytes.data(), bytes.size(), loaded_file));

    tl::core::JournalStatistics loaded;
    tl::core::read_journal(loaded_file.journal, loaded);
    TL_CHECK_EQ(loaded.value(tl::core::JournalStatistic::Deaths), -7);

    for (size_t i = 0; i < tl::core::kJournalStatisticCount; ++i) {
        const auto stat = static_cast<tl::core::JournalStatistic>(i);
        TL_CHECK_EQ(loaded.value(stat), original.value(stat));
    }
}

TL_TEST(journal_time_played_format) {
    using tl::core::format_time_played;
    TL_CHECK_EQ(format_time_played(0.0f), std::string("0hrs 0mins 0secs"));
    TL_CHECK_EQ(format_time_played(59.0f), std::string("0hrs 0mins 59secs"));
    TL_CHECK_EQ(format_time_played(3661.0f), std::string("1hrs 1mins 1secs"));
    // `ceilf` first: a fraction of a second rounds up, here into
    // the next minute.
    TL_CHECK_EQ(format_time_played(59.25f), std::string("0hrs 1mins 0secs"));
    TL_CHECK_EQ(format_time_played(0.001f), std::string("0hrs 0mins 1secs"));
    // Above 100 hours: 105 h 23 min 45 s = 379425 s.
    TL_CHECK_EQ(format_time_played(379425.0f), std::string("105hrs 23mins 45secs"));
}

TL_TEST(journal_increment_wraps_like_an_int) {
    tl::core::JournalStatistics stats;
    stats.set(tl::core::JournalStatistic::GoldGathered, 0x7fffffff);
    stats.increment(tl::core::JournalStatistic::GoldGathered, 1);
    TL_CHECK_EQ(stats.value(tl::core::JournalStatistic::GoldGathered), INT32_MIN);
}

TL_TEST(journal_page_rows_follow_the_console) {
    tl::core::JournalStatistics stats;
    stats.set(tl::core::JournalStatistic::Deaths, 3);
    stats.set(tl::core::JournalStatistic::MonstersDefeated, 41);
    stats.set(tl::core::JournalStatistic::ItemsEnchanted, -2);
    // A value written to slot 0 is never shown: row 0 is the clock.
    stats.set(tl::core::JournalStatistic::TimePlayed, 99);
    const auto rows = tl::core::journal_page_rows(stats, 784.5f);

    TL_CHECK_EQ(rows[0].label, std::string("|cFFC8C8C8Time Played:|u"));
    TL_CHECK_EQ(rows[0].value, std::string("|cFFC8C8C80hrs 13mins 5secs|u"));
    // slot order, so Deaths is the sixth row.
    TL_CHECK_EQ(rows[4].label, std::string("|cFFC8C8C8Quests Completed:|u"));
    TL_CHECK_EQ(rows[5].label, std::string("|cFFC8C8C8Deaths:|u"));
    TL_CHECK_EQ(rows[5].value, std::string("|cFFC8C8C83|u"));
    TL_CHECK_EQ(rows[6].label, std::string("|cFFC8C8C8Monsters Defeated:|u"));
    TL_CHECK_EQ(rows[6].value, std::string("|cFFC8C8C841|u"));
    TL_CHECK_EQ(rows[8].label, std::string("|cFFC8C8C8Skills/Spells Cast:|u"));
    TL_CHECK_EQ(rows[1].value, std::string("|cFFC8C8C80|u"));
    // Printed as a signed int.
    TL_CHECK_EQ(rows[17].label, std::string("|cFFC8C8C8Items Enchanted:|u"));
    TL_CHECK_EQ(rows[17].value, std::string("|cFFC8C8C8-2|u"));
}

}  // namespace
