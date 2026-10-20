// The stash and the shared stash. See `stash.h`.
#include "core/stash.h"

namespace tl::core {

Inventory make_player_stash() {
    // `CInventory(player, 0x32)`, and nothing added.
    return Inventory(kPlayerStashCapacity, /*bare=*/true);
}

SharedStash::SharedStash() : inventory_(kSharedStashCapacity, /*bare=*/true) {
    // `CSharedStash::CSharedStash`: `+0x28` 0 and an empty
    // `CInventory(nullptr, 0x2a)`.
}

bool SharedStash::set_player(bool hardcore) {
    // `CSharedStash::setPlayer`.
    if (hardcore_ == hardcore) return false;
    hardcore_ = hardcore;
    inventory_ = Inventory(kSharedStashCapacity, /*bare=*/true);
    return true;
}

bool SharedStash::begin_load() {
    // `CSharedStash::loadSharedStash`:
    // returns at once when the inventory holds anything; otherwise a new one,
    // then the file.
    if (!inventory_.empty()) return false;
    inventory_ = Inventory(kSharedStashCapacity, /*bare=*/true);
    return true;
}

// PORT: the shared stash's file, the hardcore one apart.
const char* SharedStash::file_name() const {
    return hardcore_ ? "sharedstashh.bin" : "sharedstash.bin";
}

}  // namespace tl::core
