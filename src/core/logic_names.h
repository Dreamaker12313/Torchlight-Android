// The layout descriptors' name lookups: a property a layout stores as its name
// (`LOOP STYLE`, `LOGIC`, `STATE`...) turned into the index its setter keeps.
//
// Each walks a table of wide strings in order and compares the name exactly
// (case and spaces count); a name that is none of them is 0, except the
// difficulty's. The tables are the console's, in its order.
#pragma once

#include <cstdint>
#include <string>

namespace tl::core {

/// `CUnitTriggerDescriptor::GetLoopTypeIDByString`:
/// `gTRIGGER_LOOP_TYPE_NAMES`.
int32_t loop_type_id_by_string(const std::string& name);

/// `CCounterDescriptor::GetCounterTypeIDByString`:
/// `gCOUNTER_TYPE_NAMES`, the six `LOGIC`s
/// `updateCounter`, `add` and `subtract` switch on.
int32_t counter_type_id_by_string(const std::string& name);

/// `CRandomChoiceDescriptor::GetChoiceTypeIDByString`:
/// `gRANDOMCHOICE_TYPE_NAMES`, `WEIGHT` then
/// `RANDOM`.
int32_t choice_type_id_by_string(const std::string& name);

/// `CTimelineDescriptor::GetTimelineModIDByString`:
/// `None`, `Set`, `Multiply`.
int32_t timeline_mod_id_by_string(const std::string& name);

/// `CGameStateControllerDescriptor::getGameStateIDByString`:
/// `gGAMESTATE_NAMES`, the `STATE` a Game
/// State Controller authors.
int32_t game_state_id_by_string(const std::string& name);

/// `CRandomGroupDescriptor::GetRandomGroupTypeIDByString`:
/// `gRANDOMGROUP_NAMES`, `ALL`, `Weight`,
/// `Random Chance`.
uint32_t random_group_type_from_string(const std::string& name);

/// `CRandomGroupDescriptor::getDifficultyIDByString`:
/// `EASY`, `NORMAL`, `HARD` and `VERY HARD` are
/// 0..3, anything else (the empty name too) 0xe.
uint32_t difficulty_id_by_string(const std::string& name);

}  // namespace tl::core
