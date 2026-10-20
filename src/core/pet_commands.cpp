#include "core/pet_commands.h"

namespace tl::core {

// (`CMessagePetCommandsXenon`'s gate).
bool pet_can_depart(const PetDepartureLevel& level, PetState state) {
    if (!level.has_template) return false;
    // Neither going to town (`returnToTownAI`) nor away.
    if (state == PetState::Departing || state == PetState::InTown) return false;
    // `IS_TOWN` first, then `ALLOW_PET_RETURN` is the answer.
    if (level.is_town) return false;
    return level.allow_pet_return;
}

//
bool pet_takes_items(bool tray_owner_is_player, bool have_pet, PetState pet_state) {
    if (!tray_owner_is_player) return true;
    if (!have_pet) return false;
    return pet_state != PetState::Departing && pet_state != PetState::InTown;
}

// PORT: the port's stance as the console's number (0 passive, 1 aggressive, 2 defensive).
uint32_t pet_stance_number(PetStance stance) {
    switch (stance) {
        case PetStance::Passive: return 0;
        case PetStance::Aggressive: return 1;
        case PetStance::Defensive: return 2;
    }
    return 1;
}

// PORT: the console's stance number as the port's stance.
PetStance pet_stance_from_number(uint32_t number) {
    switch (number) {
        case 0: return PetStance::Passive;
        case 2: return PetStance::Defensive;
        default: return PetStance::Aggressive;
    }
}

// `CMessagePetCommandsXenon::vfunc_2`, cases 0x13 and 0x14.
uint32_t step_pet_stance_number(uint32_t stance, int direction) {
    if (direction < 0) {
        if (stance == 0) return 2;
        if (stance == 1) return 0;
        return 1;
    }
    if (stance == 0) return 1;
    if (stance == 1) return 2;
    if (stance < 3) return 0;
    return 1;
}

// PORT: the stance stepped through its number (`step_pet_stance_number`, which holds the rule).
PetStance step_pet_stance(PetStance stance, int direction) {
    return pet_stance_from_number(step_pet_stance_number(pet_stance_number(stance), direction));
}

// `CMessagePetCommandsXenon::vfunc_2`: the switch on the code.
// Case 8 has no `break`: whether or not the pet went, it falls into the
// default and the dialog closes.
PetCommand pet_command_for_code(int code) {
    switch (code) {
        case 5:
        case 7:
        case 0x11:
        case 0x12:
        case 0x17:
        case 0x18: return PetCommand::Ignore;
        case 8: return PetCommand::SendToTown;
        case 0x13: return PetCommand::StanceLeft;
        case 0x14: return PetCommand::StanceRight;
        default: return PetCommand::Close;
    }
}

// `CMessagePetCommandsXenon::vfunc_5`.
//
PetCommandsView pet_commands_view(PetStance stance, bool can_depart) {
    PetCommandsView view;
    view.aggressive_image = "pet_aggressive_inactive";
    view.defensive_image = "pet_defensive_inactive";
    view.passive_image = "pet_passive_inactive";
    switch (pet_stance_number(stance)) {
        case 0:
            view.passive_image = "pet_passive";
            view.stance_text = "Passive Stance";
            break;
        case 1:
            view.aggressive_image = "pet_aggressive";
            view.stance_text = "Aggressive Stance";
            break;
        case 2:
            view.defensive_image = "pet_defensive";
            view.stance_text = "Defensive Stance";
            break;
        default: break;
    }
    view.send_visible = can_depart;
    return view;
}

}  // namespace tl::core
