// The console's pet commands dialog (CMessagePetCommandsXenon): the send gate, the stance
// steps, what each pad code does, and what the rows show.
#include "core/pet_commands.h"
#include "test_support.h"

namespace {

using tl::core::PetCommand;
using tl::core::PetDepartureLevel;
using tl::core::PetStance;
using tl::core::PetState;

PetDepartureLevel dungeon_floor() {
    PetDepartureLevel level;
    level.has_template = true;
    level.allow_pet_return = true;
    level.is_town = false;
    return level;
}

}  // namespace

TL_TEST(pet_commands_gate_follows_the_template_and_the_pets_state) {
    PetDepartureLevel level = dungeon_floor();
    TL_CHECK(tl::core::pet_can_depart(level, PetState::Following));
    TL_CHECK(tl::core::pet_can_depart(level, PetState::Attacking));
    TL_CHECK(tl::core::pet_can_depart(level, PetState::Fleeing));
    // AI states 0x29 and 0x2a: already going, or gone.
    TL_CHECK(!tl::core::pet_can_depart(level, PetState::Departing));
    TL_CHECK(!tl::core::pet_can_depart(level, PetState::InTown));

    level.is_town = true;
    TL_CHECK(!tl::core::pet_can_depart(level, PetState::Following));
    level.is_town = false;
    level.allow_pet_return = false;
    TL_CHECK(!tl::core::pet_can_depart(level, PetState::Following));
    // No template: a layout no dungeon owns. The flags do not matter.
    level = dungeon_floor();
    level.has_template = false;
    TL_CHECK(!tl::core::pet_can_depart(level, PetState::Following));
}

TL_TEST(pet_commands_number_stances_as_the_original) {
    TL_CHECK_EQ(tl::core::pet_stance_number(PetStance::Passive), 0u);
    TL_CHECK_EQ(tl::core::pet_stance_number(PetStance::Aggressive), 1u);
    TL_CHECK_EQ(tl::core::pet_stance_number(PetStance::Defensive), 2u);
    TL_CHECK(tl::core::pet_stance_from_number(0) == PetStance::Passive);
    TL_CHECK(tl::core::pet_stance_from_number(1) == PetStance::Aggressive);
    TL_CHECK(tl::core::pet_stance_from_number(2) == PetStance::Defensive);
}

TL_TEST(pet_commands_step_the_stance_both_ways) {
    // Left: 0 -> 2, 1 -> 0, anything else -> 1.
    TL_CHECK_EQ(tl::core::step_pet_stance_number(0, -1), 2u);
    TL_CHECK_EQ(tl::core::step_pet_stance_number(1, -1), 0u);
    TL_CHECK_EQ(tl::core::step_pet_stance_number(2, -1), 1u);
    TL_CHECK_EQ(tl::core::step_pet_stance_number(7, -1), 1u);
    // Right: 0 -> 1, 1 -> 2, 2 -> 0, anything else -> 1.
    TL_CHECK_EQ(tl::core::step_pet_stance_number(0, 1), 1u);
    TL_CHECK_EQ(tl::core::step_pet_stance_number(1, 1), 2u);
    TL_CHECK_EQ(tl::core::step_pet_stance_number(2, 1), 0u);
    TL_CHECK_EQ(tl::core::step_pet_stance_number(7, 1), 1u);

    // On the port's enum: right is Passive -> Aggressive -> Defensive -> Passive,
    // left the reverse, and three steps either way come home.
    TL_CHECK(tl::core::step_pet_stance(PetStance::Passive, 1) == PetStance::Aggressive);
    TL_CHECK(tl::core::step_pet_stance(PetStance::Aggressive, 1) == PetStance::Defensive);
    TL_CHECK(tl::core::step_pet_stance(PetStance::Defensive, 1) == PetStance::Passive);
    TL_CHECK(tl::core::step_pet_stance(PetStance::Defensive, -1) == PetStance::Aggressive);
    TL_CHECK(tl::core::step_pet_stance(PetStance::Aggressive, -1) == PetStance::Passive);
    TL_CHECK(tl::core::step_pet_stance(PetStance::Passive, -1) == PetStance::Defensive);
}

TL_TEST(pet_commands_map_each_pad_code) {
    // A, X, the stick's up and down, and 0x17/0x18 do nothing.
    for (int code : {5, 7, 0x11, 0x12, 0x17, 0x18}) {
        TL_CHECK(tl::core::pet_command_for_code(code) == PetCommand::Ignore);
    }
    TL_CHECK(tl::core::pet_command_for_code(8) == PetCommand::SendToTown);
    TL_CHECK(tl::core::pet_command_for_code(0x13) == PetCommand::StanceLeft);
    TL_CHECK(tl::core::pet_command_for_code(0x14) == PetCommand::StanceRight);
    // B, Start, Back, the shoulders and triggers close it.
    for (int code : {6, 9, 10, 11, 12, 0xd, 0xe}) {
        TL_CHECK(tl::core::pet_command_for_code(code) == PetCommand::Close);
    }
}

TL_TEST(pet_commands_light_the_current_stance) {
    tl::core::PetCommandsView view = tl::core::pet_commands_view(PetStance::Passive, true);
    TL_CHECK_EQ(view.passive_image, std::string("pet_passive"));
    TL_CHECK_EQ(view.aggressive_image, std::string("pet_aggressive_inactive"));
    TL_CHECK_EQ(view.defensive_image, std::string("pet_defensive_inactive"));
    TL_CHECK_EQ(view.stance_text, std::string("Passive Stance"));
    TL_CHECK(view.send_visible);

    view = tl::core::pet_commands_view(PetStance::Aggressive, false);
    TL_CHECK_EQ(view.passive_image, std::string("pet_passive_inactive"));
    TL_CHECK_EQ(view.aggressive_image, std::string("pet_aggressive"));
    TL_CHECK_EQ(view.defensive_image, std::string("pet_defensive_inactive"));
    TL_CHECK_EQ(view.stance_text, std::string("Aggressive Stance"));
    TL_CHECK(!view.send_visible);

    view = tl::core::pet_commands_view(PetStance::Defensive, true);
    TL_CHECK_EQ(view.defensive_image, std::string("pet_defensive"));
    TL_CHECK_EQ(view.stance_text, std::string("Defensive Stance"));
}

TL_TEST(pet_commands_pet_takes_items_from_a_tray) {
    // Any tray the player does not own passes, pet or none.
    TL_CHECK(tl::core::pet_takes_items(false, false, PetState::InTown));
    // The player's bag: a pet that is here.
    TL_CHECK(tl::core::pet_takes_items(true, true, PetState::Following));
    TL_CHECK(!tl::core::pet_takes_items(true, true, PetState::Departing));
    TL_CHECK(!tl::core::pet_takes_items(true, true, PetState::InTown));
    TL_CHECK(!tl::core::pet_takes_items(true, false, PetState::Following));
}
