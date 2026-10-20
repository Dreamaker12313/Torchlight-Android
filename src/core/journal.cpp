#include "core/journal.h"

#include <cmath>
#include <cstdio>

namespace tl::core {

const char* journal_statistic_name(JournalStatistic stat) {
    switch (stat) {
        case JournalStatistic::TimePlayed: return "TimePlayed";
        case JournalStatistic::GoldGathered: return "GoldGathered";
        case JournalStatistic::LevelsExplored: return "LevelsExplored";
        case JournalStatistic::StepsTaken: return "StepsTaken";
        case JournalStatistic::QuestsCompleted: return "QuestsCompleted";
        case JournalStatistic::Deaths: return "Deaths";
        case JournalStatistic::MonstersDefeated: return "MonstersDefeated";
        case JournalStatistic::ChampionsDefeated: return "ChampionsDefeated";
        case JournalStatistic::SkillsCast: return "SkillsCast";
        case JournalStatistic::ChestsOpened: return "ChestsOpened";
        case JournalStatistic::TrapsSprung: return "TrapsSprung";
        case JournalStatistic::BarrelsBroken: return "BarrelsBroken";
        case JournalStatistic::PotionsUsed: return "PotionsUsed";
        case JournalStatistic::PortalsUsed: return "PortalsUsed";
        case JournalStatistic::FishCaught: return "FishCaught";
        case JournalStatistic::TimesGambled: return "TimesGambled";
        case JournalStatistic::ItemsTransmuted: return "ItemsTransmuted";
        case JournalStatistic::ItemsEnchanted: return "ItemsEnchanted";
    }
    return "Unknown";
}

// CCharacter::incrementJournalStatistic
void increment_journal_statistic(JournalStatistics& store, bool is_player, JournalStatistic stat,
                                 int32_t amount) {
    if (!is_player) return;
    store.increment(stat, amount);
}

// CGameClient::saveCharacter, its journal
// loop
void write_journal(const JournalStatistics& store,
                   std::array<uint32_t, kJournalStatisticCount>& out) {
    for (size_t i = 0; i < kJournalStatisticCount; ++i) {
        out[i] = static_cast<uint32_t>(store.value(static_cast<JournalStatistic>(i)));
    }
}

// CGameClient::loadCharacter, its journal loop
void read_journal(const std::array<uint32_t, kJournalStatisticCount>& in,
                  JournalStatistics& store) {
    for (size_t i = 0; i < kJournalStatisticCount; ++i) {
        store.set(static_cast<JournalStatistic>(i), static_cast<int32_t>(in[i]));
    }
}

// CJournalMenu::updateLayout, row 0
std::string format_time_played(float play_time) {
    const float rounded = std::ceil(play_time);
    // `cvttss2si` into a 64-bit register; out of range it yields the
    // "integer indefinite" value. Only the low 32 bits are used, unsigned.
    const int64_t whole = (rounded >= -9.2233720e18f && rounded < 9.2233720e18f)
                              ? static_cast<int64_t>(rounded)
                              : INT64_MIN;
    const uint32_t total = static_cast<uint32_t>(static_cast<uint64_t>(whole));
    const uint32_t secs = total % 60u;
    const uint32_t minutes_total = total / 60u;
    const uint32_t mins = minutes_total % 60u;
    const uint32_t hrs = minutes_total / 60u;

    char buf[64];
    std::snprintf(buf, sizeof(buf), "%uhrs %umins %usecs", hrs, mins, secs);
    return std::string(buf);
}

}  // namespace tl::core

namespace tl::core {

// CJournalXenon's fill, the jump table
const char* journal_statistic_label(JournalStatistic stat) {
    switch (stat) {
        case JournalStatistic::TimePlayed: return "Time Played";
        case JournalStatistic::GoldGathered: return "Gold Gathered";
        case JournalStatistic::LevelsExplored: return "Levels Explored";
        case JournalStatistic::StepsTaken: return "Steps Taken";
        case JournalStatistic::QuestsCompleted: return "Quests Completed";
        case JournalStatistic::Deaths: return "Deaths";
        case JournalStatistic::MonstersDefeated: return "Monsters Defeated";
        case JournalStatistic::ChampionsDefeated: return "Champions Defeated";
        case JournalStatistic::SkillsCast: return "Skills/Spells Cast";
        case JournalStatistic::ChestsOpened: return "Chests Opened";
        case JournalStatistic::TrapsSprung: return "Traps Sprung";
        case JournalStatistic::BarrelsBroken: return "Barrels Broken";
        case JournalStatistic::PotionsUsed: return "Potions Used";
        case JournalStatistic::PortalsUsed: return "Portals Used";
        case JournalStatistic::FishCaught: return "Fish Caught";
        case JournalStatistic::TimesGambled: return "Times Gambled";
        case JournalStatistic::ItemsTransmuted: return "Items Transmuted";
        case JournalStatistic::ItemsEnchanted: return "Items Enchanted";
    }
    return "";
}

// CJournalXenon's fill
std::array<JournalPageRow, kJournalStatisticCount> journal_page_rows(const JournalStatistics& store,
                                                                     float play_time) {
    constexpr const char* kRunOpen = "|cFFC8C8C8";
    std::array<JournalPageRow, kJournalStatisticCount> rows;
    for (size_t i = 0; i < kJournalStatisticCount; ++i) {
        const auto stat = static_cast<JournalStatistic>(i);
        rows[i].label = std::string(kRunOpen) + journal_statistic_label(stat) + ":|u";
        // row 0 the clock, the others their slot.
        const std::string value =
            i == 0 ? format_time_played(play_time) : std::to_string(store.value(stat));
        rows[i].value = std::string(kRunOpen) + value + "|u";
    }
    return rows;
}

}  // namespace tl::core
