// The console's automap: the level's picture, drawn from its passability grid,
// and the reveal that uncovers it. The console has no room-piece tiles
// (`CLevel::addRoomPieceToAutomap` is the PC's): `CAutomap::create`
// lays `CLevel::createAutomapTexture` on a grid mesh,
// a vertex every 4 units, and `CAutomap::setPlayerPosition`
// fades vertices in. A town is uncovered whole.
#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace tl::core {

class PassabilityMap;

/// The level's picture, as `CLevel::createAutomapTexture`
/// fills its `Ogre::Image`: `PF_R8G8B8`, one pixel per `step` cells, rows along
/// z and columns along x.
struct AutomapTexture {
    uint32_t width = 0;   ///< a power of two, at least 2 (halved past 512)
    uint32_t height = 0;  ///< a power of two, at least 2 (halved past 512)
    int step = 2;         ///< cells per pixel: 2, or 4 when halved
    /// `width * height * 3` bytes, row by row: 0 for a blocked cell, 128 for a
    /// walkable one whose eight neighbours `step` away are walkable, 200 for a
    /// walkable one on an edge.
    std::vector<uint8_t> rgb;
    /// The grid's far corner less its near corner (`CLevel +0x184 - +0x17c`,
    /// `+0x188 - +0x180`).
    float extent_x = 0.0f;
    float extent_z = 0.0f;
    /// The grid's near corner (`CLevel +0x17c`, `+0x180`).
    float origin_x = 0.0f;
    float origin_z = 0.0f;
    /// How much of the texture the grid covers: `(cells >> 1) / size`, taken
    /// before any halving.
    float used_u = 0.0f;
    float used_v = 0.0f;

    bool empty() const { return rgb.empty(); }
};

/// The grid `createAutomapTexture` reads: its size in cells and its corners
/// (`CLevel +0x30`, `+0x34`, `+0x17c`..`+0x188`).
struct AutomapGridShape {
    uint32_t cells_wide = 0;
    uint32_t cells_high = 0;
    float top_left_x = 0.0f;
    float top_left_z = 0.0f;
    float bottom_right_x = 0.0f;
    float bottom_right_z = 0.0f;
};

/// `CLevel::createAutomapTexture` over any grid; `passable`
/// is `CLevel::mapPassable(int, int)`. Empty for a grid with
/// no cells.
AutomapTexture create_automap_texture(const AutomapGridShape& shape,
                                      const std::function<bool(int, int)>& passable);

/// The same over the level's own passability grid; empty when it has not been
/// baked (the console's guard on the level's collision list).
AutomapTexture create_automap_texture(const PassabilityMap& map);

/// The reveal: `CAutomap`'s two arrays, what has been uncovered and how far each
/// vertex has faded in, one per vertex of the map's mesh.
class AutomapReveal {
public:
    /// `CAutomap::create`: the corners it keeps from the
    /// texture, and the exposed flag.
    void create(const AutomapTexture& texture, bool exposed);

    /// `CAutomap::finalize`: the grid sized
    /// `(int)fma(max - min, 0.25, 1)` a side, both arrays zeroed and the map
    /// dirty; an exposed level is revealed whole.
    void finalize();

    /// `CAutomap::setPlayerPosition`, its
    /// reveal: a 6 x 6 square of vertices around the player's, 1.0 inside and
    /// 0.6 on its rim; a vertex only brightens.
    void reveal_around(float x, float z);

    /// `CAutomap::getRevealed`: the index is
    /// tested against the whole grid and the coordinates only against 0, so an
    /// `x` past a row's end reads the next row's start.
    float revealed(int x, int z) const;

    /// What `CAutomap::update` did this frame.
    struct Step {
        bool stepped = false;  ///< the map was dirty: the fades ran, and it is clean
        bool repaint = false;  ///< an opacity moved: the mesh's colours are rewritten
    };

    /// `CAutomap::update`, its first part, on a visible map:
    /// nothing unless dirty; then clean, and (not exposed) every vertex's
    /// opacity moves `2 * delta` toward its reveal, staying dirty until all
    /// have arrived.
    Step fade(float delta);

    /// The dirty flag set again: by an icon still fading, or a restored reveal.
    void mark_dirty() { dirty_ = true; }

    /// PORT: a still frame (a frame dump, `Automap::update(.., 0)`) shows every
    /// vertex at its reveal at once, rather than fading over frames that never
    /// come. True when an opacity moved.
    bool settle();

    /// The vertex a world position falls in, `(int)((p - min) * 0.25)`.
    int cell_x(float x) const { return static_cast<int>((x - min_x_) * 0.25f); }
    int cell_z(float z) const { return static_cast<int>((z - min_z_) * 0.25f); }

    /// `CLevelState::restoreAutomap`: only a grid of the same size is
    /// put back, and the map is dirty.
    bool restore(int32_t tiles_x, int32_t tiles_y, const std::vector<float>& cells);

    void clear();

    // PORT: accessors of `CAutomap`'s fields (`getFOWWidth`, `getFOWHeight`, `getRevealedMap`).
    int tiles_x() const { return tiles_x_; }
    // PORT: an accessor of the tiles down.
    int tiles_y() const { return tiles_y_; }
    // PORT: an accessor of the exposed flag.
    bool exposed() const { return exposed_; }
    // PORT: an accessor of the dirty flag.
    bool dirty() const { return dirty_; }
    // PORT: an accessor of the reveal.
    const std::vector<float>& revealed_cells() const { return revealed_; }
    // PORT: an accessor of the opacities.
    const std::vector<float>& opacity() const { return opacity_; }

private:
    float max_x_ = 0.0f;           ///< `+0x60`
    float max_z_ = 0.0f;           ///< `+0x64`
    float min_x_ = 0.0f;           ///< `+0x68`
    float min_z_ = 0.0f;           ///< `+0x6c`
    int tiles_x_ = 0;              ///< `+0x70`
    int tiles_y_ = 0;              ///< `+0x74`
    std::vector<float> revealed_;  ///< `+0x80`
    std::vector<float> opacity_;   ///< `+0x84`
    bool dirty_ = false;           ///< `+0x88`
    bool exposed_ = false;         ///< `+0x8a`
};

}  // namespace tl::core
