#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace tl::core {

/// `EJournalStatistic`: the 18 slots of `CPlayer +0x7dc`, each printed by
/// `CJournalMenu::updateLayout` under
/// the label of the same name. Each member names its `incrementJournalStatistic` callers.
enum class JournalStatistic : int32_t {
    TimePlayed = 0,    ///< `g_TimePlayed`: no writer; row 0 prints the `+0x388` clock instead
    GoldGathered = 1,  ///< `g_GoldGathered`: `CCharacter::giveGold`
    /// `g_LevelsExplored`: written directly, `storeLevelSavedState`
    LevelsExplored = 2,
    StepsTaken = 3,         ///< `g_StepsTaken`: `CCharacter::updateAnimation`
    QuestsCompleted = 4,    ///< `g_QuestsCompleted`: `CQuest::giveRewardForQuest`
    Deaths = 5,             ///< `g_Deaths`: `CCharacter::die`
    MonstersDefeated = 6,   ///< `g_MonstersDefeated`: `CCharacter::awardExperience`
    ChampionsDefeated = 7,  ///< `g_ChampionsDefeated`: `CCharacter::awardExperience`
    SkillsCast = 8,         ///< `g_SkillsCast`: `CCharacter::castSkill`
    ChestsOpened = 9,       ///< `g_ChestsOpened`: `CTriggerUnit::trigger`
    /// `g_TrapsSprung`: `CBreakable::interact`, `CTriggerUnit::trigger`
    TrapsSprung = 10,
    BarrelsBroken = 11,    ///< `g_BarrelsBroken`: `CCharacter::performAttack`
    PotionsUsed = 12,      ///< `g_PotionsUsed`: `CEquipment::useOnTarget`
    PortalsUsed = 13,      ///< `g_PortalsUsed`: `CCharacter::isEffectValidForUnit`
    FishCaught = 14,       ///< `g_FishCaught`: `CPlayer::fishingAICatch`
    TimesGambled = 15,     ///< `g_TimesGambled`: `CGameUI::menuItemClick` and four more
    ItemsTransmuted = 16,  ///< `g_ItemsTransmuted`: `CCombineMenu::performInteraction`
    ItemsEnchanted = 17,   ///< `g_ItemsEnchanted`: `CEnchantMenu::performInteraction`
};

constexpr size_t kJournalStatisticCount = 18;

/// The enumerator's name, for the log.
const char* journal_statistic_name(JournalStatistic stat);

/// `CPlayer +0x7dc`, 18 `int` slots. `CPlayer::CPlayer`
/// zeroes them one by one.
class JournalStatistics {
public:
    // PORT: an accessor of the journal statistics.
    int32_t value(JournalStatistic stat) const {
        const size_t idx = static_cast<size_t>(stat);
        return idx < kJournalStatisticCount ? stats_[idx] : 0;
    }

    /// CPlayer::setJournalStatistic
    void set(JournalStatistic stat, int32_t val) {
        const size_t idx = static_cast<size_t>(stat);
        if (idx < kJournalStatisticCount) stats_[idx] = val;
    }

    /// CPlayer::incrementJournalStatistic.
    void increment(JournalStatistic stat, int32_t amount = 1) {
        const size_t idx = static_cast<size_t>(stat);
        if (idx < kJournalStatisticCount) {
            // `int` addition wrapping like the original's `add`.
            stats_[idx] = static_cast<int32_t>(static_cast<uint32_t>(stats_[idx]) +
                                               static_cast<uint32_t>(amount));
        }
    }

private:
    std::array<int32_t, kJournalStatisticCount> stats_{};
};

/// CCharacter::incrementJournalStatistic:
/// only a player (`ISA 0x1c`) that is a `CPlayer` forwards to the store.
void increment_journal_statistic(JournalStatistics& store, bool is_player, JournalStatistic stat,
                                 int32_t amount = 1);

/// CGameClient::saveCharacter: the 18 slots
/// in index order, each `int` written as its 4 bytes.
void write_journal(const JournalStatistics& store,
                   std::array<uint32_t, kJournalStatisticCount>& out);

/// CGameClient::loadCharacter: every slot read back in index order
/// through `CPlayer::setJournalStatistic`.
void read_journal(const std::array<uint32_t, kJournalStatisticCount>& in, JournalStatistics& store);

/// Row 0 of CJournalMenu::updateLayout: the `float` play
/// clock (`TravelRecord::play_time`) ceiled, truncated and split as unsigned into
/// `"1hrs 4mins 9secs"`.
///
std::string format_time_played(float play_time);

/// The label `CJournalXenon`'s fill
/// gives a row, before the translation: slot `i`'s, so the page lists the slots in the enum's
/// order.
const char* journal_statistic_label(JournalStatistic stat);

/// One row of the console's journal page: two `ceguiWidget/StaticText`
/// windows over the same rectangle, the label left-aligned and the value
/// right-aligned. Both texts carry the original's colour run.
struct JournalPageRow {
    std::string label;
    std::string value;
};

/// The row windows' size and pitch in layout pixels, before the UI scale,
/// inside the layout's `journalwindow`.
inline constexpr float kJournalRowWidth = 270.0f;
inline constexpr float kJournalRowHeight = 23.0f;
inline constexpr float kJournalRowPitch = 24.0f;

/// CJournalXenon's fill, the texts: 18 rows, one per slot, each wrapped in
/// the original's colour run.
/// The console has no Difficulty and Ancestors rows, which the PC's
/// `CJournalMenu::updateLayout` adds.
std::array<JournalPageRow, kJournalStatisticCount> journal_page_rows(const JournalStatistics& store,
                                                                     float play_time);

}  // namespace tl::core
