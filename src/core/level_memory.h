// Where the player has been: the levels the original remembers and the dungeon records it
// keeps, from `CPlayer`. `CPlayer::storeLevelSavedState`,
// `CGameClient::loadLevel`, `CPlayer::updateStoredLevels`.
// Only the list and its rules: filling a state from a live
// level and applying one belong to whoever owns the level. No OGRE and no file I/O.
#pragma once

#include "formats/save_file.h"

#include <cstdint>
#include <string>
#include <vector>

namespace tl::core {

/// `CGameGlobals +0x2c0 .. +0x2c8`, read from `media/GLOBALS.DAT` by
/// `CGameGlobals::reload`; the defaults are
/// the constructor's.
struct LevelMemoryRules {
    int32_t max_level_memory = 6;        ///< `MAX_LEVEL_MEMORY`
    int32_t level_respawn_time = 1800;   ///< `LEVEL_RESPAWN_TIME`, seconds of play
    int32_t merchant_respawn_time = 60;  ///< `MERCHANT_RESPAWN_TIME`
};

/// The fields of `CPlayer` that remember where the player has been.
struct TravelRecord {
    std::vector<tl::formats::LevelSaveState> levels;        ///< +0x798
    std::vector<tl::formats::DungeonTrackerSave> trackers;  ///< +0xa40
    float play_time = 0.0f;                 ///< +0x388, advanced by `CCharacter::update`
    int32_t deepest_depth = 0;              ///< +0x7d8
    int32_t deepest_depth_max = 0;          ///< +0x7e4
    std::string last_dungeon;               ///< +2000
    int32_t last_dungeon_depth = 0;         ///< +0x7cc
    bool portal_open = false;               ///< +0x7b0
    tl::formats::SaveVec3 portal_position;  ///< +0x7c0
    std::string portal_dungeon;             ///< +0x7b8
    int32_t portal_depth = 0;               ///< +0x7b4
};

/// What `updateStoredLevels` asks of one of the player's quests (`CPlayer+0x868`):
/// whether it has journal details, and the dungeon (+0x30) and depth (+0x20) it
/// is bound to.
struct QuestLevelClaim {
    bool has_details = false;
    std::string dungeon;
    int32_t depth = 0;
};

/// `STRINGS::StringUpper` on ASCII, which is what every dungeon name is.
std::string upper_ascii(const std::string& text);

/// `CPlayer::hasLevelSavedState(std::wstring, int)`.
///
bool has_level_saved_state(const TravelRecord& travel, const std::string& dungeon, int32_t depth);

/// `CPlayer::getLevelSavedState(std::wstring, int)`; null when none.
tl::formats::LevelSaveState* get_level_saved_state(TravelRecord& travel, const std::string& dungeon,
                                                   int32_t depth);

/// `CPlayer::removeLevelSavedState(unsigned int)`:
/// the last state moves into the slot. An
/// index past the end removes nothing but still drops the last entry, as the
/// original does.
void remove_level_saved_state(TravelRecord& travel, uint32_t index);

/// `CPlayer::hasDungeonHistory(std::wstring)`:
/// a remembered level of that (upper-cased)
/// name.
bool has_dungeon_history(const TravelRecord& travel, const std::string& dungeon);

/// `CPlayer::getMaxDepth(std::wstring)`: the
/// tracker's deepest depth, or -1.
int32_t get_max_depth(const TravelRecord& travel, const std::string& dungeon);

/// `CPlayer::getDungeonRank(std::wstring)`:
/// the tracker's recorded player level, or 0 when none matches.
int32_t get_dungeon_rank(const TravelRecord& travel, const std::string& dungeon);

/// `CPlayer::clearDungeonHistory(std::wstring)`.
///
void clear_dungeon_history(TravelRecord& travel, const std::string& dungeon);

/// `CPlayer::updateDungeonTracking(std::wstring, int)`.
/// `salt` is the dungeon's `CDungeon+0x58`,
/// or null when `CDungeonManager::getDungeonByName` finds no such dungeon.
void update_dungeon_tracking(TravelRecord& travel, const std::string& dungeon, int32_t depth,
                             int32_t player_level, const int32_t* salt);

/// `CPlayer::storeLevelSavedState`, steps 1
/// to 4: the tracking and depth bookkeeping, the old state of this level
/// dropped (its time kept), and a new empty state appended and returned for the
/// caller to fill. The caller then calls `update_stored_levels`.
tl::formats::LevelSaveState& begin_store_level_saved_state(TravelRecord& travel,
                                                           const std::string& dungeon,
                                                           int32_t depth, bool is_town,
                                                           bool dont_store, int32_t player_level,
                                                           const int32_t* salt);

/// `CPlayer::updateStoredLevels(CLevel&)`.
/// `dungeon` and `depth` are the level just stored.
void update_stored_levels(TravelRecord& travel, const std::string& dungeon, int32_t depth,
                          const std::vector<QuestLevelClaim>& quests,
                          const LevelMemoryRules& rules);

}  // namespace tl::core
