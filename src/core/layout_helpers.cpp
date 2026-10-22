// Small helpers of a level's loading (core/layout_helpers.h).
#include "core/layout_helpers.h"

#include "core/log.h"
#include "core/platform.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace tl::core {

/// Where the character stands when a level is entered.
///
/// Two spellings, and only knowing both finds the town's.
///
/// The main-menu layouts carry a `Property Node` *named* `Player Start`, and
/// that is what the menu scene has always been read by. The playable town
/// carries one *typed* `Player Start` - a node called `Property Node0` whose
/// TYPE string is `Player Start`, at (-31.98, 0.79, 17.15) - and searching by
/// name misses it completely. `Property Node` is the editor's general-purpose
/// marker and its TYPE is the thing that says what it marks: across the shipped
/// layouts the types run `No Spawn Region` (597), `Quest Boss` (163), `Editor
/// Player Start` (156), `Quest Item` (153), `Player Start` (24), `Town Portal`,
/// `Waypoint Start` and a dozen more.
///
/// So this looks at the name first, then at the type, then at the two other
/// authored places a player is known to arrive - a town portal and a waypoint,
/// which is where the game puts a character coming back to town. Which rule
/// matched is logged: a character standing somewhere odd is otherwise
/// indistinguishable from a placement bug.
// INVENTED: the marker the player stands on: the node named `Player Start`,
// else the first marker of these types, in this order; the console's choice of
// an arrival point is not transcribed for this path.
const tl::formats::CmpNode* find_spawn(const tl::formats::CmpScene& layout) {
    auto same = [](const std::string& a, const char* b) {
        const size_t n = std::strlen(b);
        if (a.size() != n) return false;
        for (size_t i = 0; i < n; ++i) {
            if (std::tolower(static_cast<unsigned char>(a[i])) !=
                std::tolower(static_cast<unsigned char>(b[i]))) {
                return false;
            }
        }
        return true;
    };

    if (const tl::formats::CmpNode* start = layout.find("Player Start")) return start;

    // By marker type, in the order of how directly each names a place a player
    // stands. `Editor Player Start` is the editor's own preview spawn and is the
    // most common of the three, so it is a good fallback for a level that has
    // no runtime one.
    for (const char* type :
         {"Player Start", "Editor Player Start", "Town Portal", "Waypoint Start"}) {
        for (const tl::formats::CmpNode& node : layout.nodes) {
            if (!node.has_position()) continue;
            for (const std::string& text : node.strings) {
                if (!same(text, type)) continue;
                TL_LOG_INFO("no node named Player Start; using the '%s' marker '%s'", type,
                            node.name.c_str());
                return &node;
            }
        }
    }
    return nullptr;
}

/// Links an animation skeleton, named by full archive path, onto one entity.
///
/// Bones are matched by **handle**, not by name: Animation::apply(Skeleton*)
/// binds each track to skel->getBone(track.first) (OgreAnimation.cpp:336). An
/// earlier comment here said the opposite, which would have made this safe for
/// any subset of bones; it is not.
///
/// It is safe for Torchlight because the handles agree. Idle.SKELETON carries 39
/// bones to the body's 43, and the four it lacks are the tag_zfish* fishing-pole
/// tags at the end of the list, which no animation tracks. Every worn set shares
/// the body's 43 bones under the same handles. A rig that did not agree would
/// deform into nonsense rather than fail, so this is worth re-checking
/// before adding a class or a creature.
/// Whether an authored `ANIMATION` names the mesh's own bind pose.
///
/// Case-insensitively, because the archive spells it `bind` on 205 nodes and
/// `Bind` on 102 of them. An empty string means the same thing - a `Monster`
/// whose source omits `ANIMATION` compiles to the empty string on all 54 of the
/// nodes that omit it - and there is no `bind.SKELETON` anywhere in the archive.
bool names_the_bind_pose(const std::string& animation) {
    if (animation.empty()) return true;
    if (animation.size() != 4) return false;
    std::string lowered;
    for (char c : animation) {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return lowered == "bind";
}

/// The grid as a gdb dump of the original is written: a
/// header, then one line per x column, '.' for an open cell.
void write_passability_dump(const tl::core::PassabilityMap& grid, const std::string& path) {
    FILE* out = std::fopen(path.c_str(), "w");
    if (out == nullptr) {
        TL_LOG_WARN("could not open passability dump '%s'", path.c_str());
        return;
    }
    std::fprintf(out, "# passability origin_x=%.6f origin_z=%.6f width=%d height=%d passable=%u\n",
                 static_cast<double>(grid.origin_x()), static_cast<double>(grid.origin_z()),
                 grid.width(), grid.height(), grid.passable_cells());
    std::string column(static_cast<size_t>(std::max(grid.height(), 0)), '#');
    for (int x = 0; x < grid.width(); ++x) {
        for (int z = 0; z < grid.height(); ++z) {
            column[static_cast<size_t>(z)] = grid.map_passable(x, z) ? '.' : '#';
        }
        std::fprintf(out, "%s\n", column.c_str());
    }
    std::fclose(out);
    TL_LOG_INFO("  passability dumped to %s", path.c_str());
}

/// The passability lines `CLevel::calculatePassability` and `CLevel::populate`
/// print, plus the grid's size and how long the bake took.
void log_passability(const tl::core::PassabilityMap& grid, double seconds) {
    for (const tl::core::PassabilityMap::Pass& pass : grid.passes()) {
        TL_LOG_INFO("  Level Pathnode Ray Casts :%u (%u cell(s) open)", pass.ray_casts,
                    pass.passable);
    }
    TL_LOG_INFO("  passability: %d x %d cell(s) of %.1f, Pathable Area:%g m^2, in %.2f s",
                grid.width(), grid.height(), static_cast<double>(tl::core::PassabilityMap::kCell),
                static_cast<double>(grid.pathable_area()), seconds);
}

/// Log how much of the process's memory budget is gone, if the platform knows.
///
/// Silent on the desktop, which has no meaningful ceiling. On the console this
/// is the number M4 and M5 both listed as outstanding, and the reason a real
/// level can now be the console's default instead of a small stand-in scene:
/// the question stops being "will it fit?" and becomes a figure in the log.
void log_memory(const char* when) {
    unsigned long long used = 0, total = 0;
    if (!tl::core::memory_usage(used, total)) return;

    const double mb = 1024.0 * 1024.0;
    TL_LOG_INFO("memory %s: %.1f MB used of %.1f MB (%.1f%%), %.1f MB free", when,
                static_cast<double>(used) / mb, static_cast<double>(total) / mb,
                total ? 100.0 * static_cast<double>(used) / static_cast<double>(total) : 0.0,
                static_cast<double>(total - used) / mb);
}

}  // namespace tl::core
