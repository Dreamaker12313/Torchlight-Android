#include "core/warp.h"

namespace tl::core {

WarpRequest warp_levels(const std::string& dungeon, int32_t delta, int32_t absolute,
                        const TravelRecord& travel) {
    // `CGameClient::warpLevels`.
    WarpRequest request;
    if (upper_ascii(dungeon) == "LASTDUNGEON" && !travel.last_dungeon.empty()) {
        request.dungeon = travel.last_dungeon;
        request.delta = 0;
        request.absolute = travel.last_dungeon_depth;
        return request;
    }
    request.dungeon = dungeon;
    request.delta = delta;
    request.absolute = absolute;
    return request;
}

WarpDestination warp_destination(const WarpRequest& request, const WarpOrigin& from,
                                 const TravelRecord& travel) {
    // `CGameClient::performWarp`.
    WarpDestination out;
    std::string name = upper_ascii(request.dungeon);
    int32_t delta = request.delta;
    int32_t absolute = request.absolute;
    const bool has_dungeon = !from.dungeon.empty();
    const std::string current = upper_ascii(from.dungeon);
    // the current dungeon named is no name at all.
    if (has_dungeon && name == current) name.clear();

    const int32_t here = from.depth;
    if (here < 1 && delta < 0) {
        // up out of the dungeon. `PARENT_DUNGEON`, at depth 0 ...
        if (has_dungeon && !from.parent.empty()) {
            name = from.parent;
            delta = 0;
            absolute = 0;
        } else if (has_dungeon && !travel.last_dungeon.empty()) {
            // or the last-dungeon record, upper-cased, at its depth.
            name = upper_ascii(travel.last_dungeon);
            delta = 0;
            absolute = travel.last_dungeon_depth;
        } else {
            // nothing to go up to; no warp.
            return out;
        }
    }
    //
    int32_t depth = absolute;
    if (depth == 0 && name.empty()) depth = delta + here;
    // an empty name is the dungeon being left.
    if (name.empty()) name = current;
    out.go = true;
    out.dungeon = upper_ascii(name);
    out.depth = depth;
    return out;
}

}  // namespace tl::core
