// The console's pet commands dialog, `CMessagePetCommandsXenon` (Start on the inventory tab,
// `CInventoryMenuXenon`'s handler). The PC does the same jobs in
// `CGameUI::onClick`.
//
// No OGRE and no file I/O.
#pragma once

#include "core/pet.h"

#include <cstdint>
#include <string>

namespace tl::core {

/// What the gate reads off the player's level: its template and two of the stratum's flags.
struct PetDepartureLevel {
    /// A plain layout no dungeon owns has no template, and the gate says no.
    bool has_template = false;
    /// The stratum's `ALLOW_PET_RETURN` (`CDungeon::getStrataTemplate`).
    bool allow_pet_return = false;
    /// The stratum's `IS_TOWN`.
    bool is_town = false;
};

/// whether the pet may be sent to town now. It decides both the Y row's look
/// and whether Y sends.
bool pet_can_depart(const PetDepartureLevel& level, PetState state);

/// whether X on a tray may move its item into the
/// first pet's bag, and whether the tray's legend offers "Swap".
bool pet_takes_items(bool tray_owner_is_player, bool have_pet, PetState pet_state);

/// The stance as the original numbers it: 0 Passive, 1 Aggressive, 2 Defensive
/// (`CGameUI::onClick`).
uint32_t pet_stance_number(PetStance stance);
/// The other way. A number past 2 reads as Aggressive, which is what the
/// step below makes of one.
PetStance pet_stance_from_number(uint32_t number);

/// `vfunc_2` codes 0x13 (left, `direction` < 0) and 0x14 (right), on the
/// original's numbers. Anything past 2 steps to 1.
uint32_t step_pet_stance_number(uint32_t stance, int direction);
PetStance step_pet_stance(PetStance stance, int direction);

/// What `vfunc_2` does with a code (the console menus' button enum: A 5, B 6,
/// X 7, Y 8, Start 0xd, Back 0xe, the left stick 0x11 up, 0x12 down, 0x13 left,
/// 0x14 right).
enum class PetCommand {
    Ignore,       ///< 5, 7, 0x11, 0x12, 0x17, 0x18: nothing, the dialog stays
    StanceLeft,   ///< 0x13: the stance steps left, the target is dropped, a refresh
    StanceRight,  ///< 0x14: likewise to the right
    SendToTown,   ///< 8: if `pet_can_depart`, the voice and the trip; then closes
    Close,        ///< anything else: `vfunc_3(0)`
};
PetCommand pet_command_for_code(int code);

/// What `vfunc_5` puts in the dialog's five windows:
/// `sendtotown`, `PetAggressive`, `PetPassive`, `PetDefensive`,
/// `stancetext`.
struct PetCommandsView {
    /// The three stance buttons' images: `pet_<stance>` for the current one,
    /// `pet_<stance>_inactive` for the other two.
    std::string aggressive_image;
    std::string passive_image;
    std::string defensive_image;
    /// `stancetext`: the translation of "Aggressive Stance", "Passive Stance"
    /// or "Defensive Stance".
    std::string stance_text;
    /// `sendtotown` shown (`Window::setVisible`): the Y row is not there at all
    /// when the pet may not go.
    bool send_visible = false;
};
PetCommandsView pet_commands_view(PetStance stance, bool can_depart);

}  // namespace tl::core
