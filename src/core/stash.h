// The stash and the shared stash. The console's stash is a second inventory of the
// player's (`CPlayer::unitInit`; page), saved in its own
// save state at slot + 1000 - not the PC's town chest
// (`CStashMenu::setOwner`). `CSharedStash` is one
// inventory across characters. No file I/O: the caller reads and writes the file.
#pragma once

#include "core/inventory.h"

#include <cstddef>
#include <cstdint>

namespace tl::core {

/// `CInventory(player, 0x32)`, a 50-slot backpack after the
/// 19 fixed locations and no other pane.
inline constexpr size_t kPlayerStashCapacity = 50;
/// `CSharedStash::CSharedStash` and `loadSharedStash`:
/// `CInventory(nullptr, 0x2a)`.
inline constexpr size_t kSharedStashCapacity = 42;

/// writes a stash item's slot as `slot + 1000`;
/// takes back every saved item whose slot is above 999, into
/// `slot - 1000` of the stash.
inline constexpr int32_t kStashSaveSlotBase = 1000;
inline bool is_stash_save_slot(int32_t slot) { return slot > kStashSaveSlotBase - 1; }

/// The console's stash, empty:.
Inventory make_player_stash();

/// `CSharedStash`.
class SharedStash {
public:
    SharedStash();

    // PORT: an accessor of the port's own bookkeeping.
    Inventory& inventory() { return inventory_; }
    const Inventory& inventory() const { return inventory_; }
    /// `+0x28`: the stash holds the hardcore file's items.
    bool hardcore() const { return hardcore_; }

    /// `CSharedStash::setPlayer`: a player of the other
    /// kind (hardcore or not) empties the stash; true then, and the caller loads.
    bool set_player(bool hardcore);

    /// `CSharedStash::loadSharedStash`'s gate:
    /// a stash holding anything is not read again;
    /// otherwise true, and the caller reads `file_name()`.
    bool begin_load();

    /// `+0x18` (`sharedstash.bin`, .rodata) or, for a hardcore
    /// stash, `+0x20` (`sharedstashh.bin`), under the save path.
    const char* file_name() const;

private:
    Inventory inventory_;
    bool hardcore_ = false;
};

}  // namespace tl::core
