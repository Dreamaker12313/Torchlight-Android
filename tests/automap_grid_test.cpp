// `src/core/automap_grid.cpp` - the console's automap picture
// (`CLevel::createAutomapTexture`) and its reveal
// (`CAutomap::finalize` / `setPlayerPosition` / `update`).
#include "core/automap_grid.h"

#include "test_support.h"

#include <cstdint>
#include <vector>

namespace {

/// A grid of `wide` x `high` cells, walkable where `open` says.
struct Grid {
    uint32_t wide = 0;
    uint32_t high = 0;
    std::vector<bool> cells;  // x-major, as the level keeps it

    Grid(uint32_t w, uint32_t h, bool all_open) : wide(w), high(h), cells(w * h, all_open) {}
    void set(int x, int z, bool open) { cells[static_cast<size_t>(x) * high + z] = open; }
    bool passable(int x, int z) const {
        if (x < 0 || z < 0 || x >= static_cast<int>(wide) || z >= static_cast<int>(high))
            return false;
        return cells[static_cast<size_t>(x) * high + z];
    }
    tl::core::AutomapTexture texture(float left = 0.0f, float top = 0.0f) const {
        tl::core::AutomapGridShape shape;
        shape.cells_wide = wide;
        shape.cells_high = high;
        shape.top_left_x = left;
        shape.top_left_z = top;
        shape.bottom_right_x = left + static_cast<float>(wide) * 0.4f;
        shape.bottom_right_z = top + static_cast<float>(high) * 0.4f;
        return tl::core::create_automap_texture(shape,
                                                [this](int x, int z) { return passable(x, z); });
    }
};

uint8_t pixel(const tl::core::AutomapTexture& t, uint32_t col, uint32_t row) {
    return t.rgb[(row * t.width + col) * 3];
}

}  // namespace

TL_TEST(automap_texture_sizes_to_a_power_of_two_of_half_the_cells) {
    const Grid grid(40, 20, true);
    const tl::core::AutomapTexture t = grid.texture();
    TL_CHECK_EQ(t.width, 32u);   // 20 -> 32
    TL_CHECK_EQ(t.height, 16u);  // 10 -> 16
    TL_CHECK_EQ(t.step, 2);
    TL_CHECK_EQ(t.rgb.size(), size_t{32 * 16 * 3});
    TL_CHECK(t.used_u == 20.0f / 32.0f);
    TL_CHECK(t.used_v == 10.0f / 16.0f);
}

TL_TEST(automap_texture_halves_past_512) {
    const Grid grid(1100, 40, true);
    const tl::core::AutomapTexture t = grid.texture();
    TL_CHECK_EQ(t.width, 512u);  // 550 -> 1024, halved
    TL_CHECK_EQ(t.height, 16u);  // 20 -> 32, halved too
    TL_CHECK_EQ(t.step, 4);
    TL_CHECK(t.used_u == 550.0f / 1024.0f);  // taken before the halving
}

TL_TEST(automap_texture_greys_blocked_edge_and_interior) {
    Grid grid(20, 20, true);
    for (int x = 0; x < 20; ++x) grid.set(x, 0, false);  // a wall along z = 0
    const tl::core::AutomapTexture t = grid.texture();
    TL_CHECK_EQ(pixel(t, 3, 0), uint8_t{0});     // cell (6, 0): blocked
    TL_CHECK_EQ(pixel(t, 3, 1), uint8_t{200});   // cell (6, 2): its z - 2 is the wall
    TL_CHECK_EQ(pixel(t, 3, 2), uint8_t{0x80});  // cell (6, 4): all eight open
    TL_CHECK_EQ(pixel(t, 0, 3), uint8_t{200});   // cell (0, 6): x - 2 is off the grid
    TL_CHECK_EQ(pixel(t, 9, 3), uint8_t{200});   // cell (18, 6): x + 2 is off the grid
}

TL_TEST(automap_texture_keeps_the_offset_bound_quirk) {
    // 9 cells wide samples x = 0..8, five pixels into a row of four: the fifth
    // is written at the row's end, which is the next row's first pixel.
    Grid grid(9, 8, false);
    grid.set(8, 4, true);  // the fifth sample of the third row
    const tl::core::AutomapTexture t = grid.texture();
    TL_CHECK_EQ(t.width, 4u);
    // The third row's spill lands on the fourth row's first pixel, which the
    // fourth row (cell (0, 6), blocked) then writes over with 0.
    TL_CHECK_EQ(pixel(t, 0, 3), uint8_t{0});

    // With no fourth row sampled, the spill stays: 9 x 6 has rows z = 0, 2, 4
    // in a texture of four rows.
    Grid short_grid(9, 6, false);
    short_grid.set(8, 4, true);
    const tl::core::AutomapTexture s = short_grid.texture();
    TL_CHECK_EQ(s.height, 4u);
    TL_CHECK_EQ(pixel(s, 0, 3), uint8_t{200});
}

TL_TEST(automap_reveal_sizes_from_the_texture_corners) {
    const Grid grid(100, 50, true);  // 40 x 20 units
    tl::core::AutomapReveal reveal;
    reveal.create(grid.texture(10.0f, -5.0f), false);
    reveal.finalize();
    TL_CHECK_EQ(reveal.tiles_x(), 11);  // 40 / 4 + 1
    TL_CHECK_EQ(reveal.tiles_y(), 6);   // 20 / 4 + 1
    TL_CHECK(reveal.dirty());
    TL_CHECK(reveal.revealed(0, 0) == 0.0f);
}

TL_TEST(automap_reveal_exposed_level_is_whole) {
    const Grid grid(100, 50, true);
    tl::core::AutomapReveal reveal;
    reveal.create(grid.texture(), true);
    reveal.finalize();
    TL_CHECK(reveal.revealed(5, 3) == 1.0f);
    // An exposed map never fades: its mesh is white from the start.
    const tl::core::AutomapReveal::Step step = reveal.fade(0.5f);
    TL_CHECK(step.stepped);
    TL_CHECK(!step.repaint);
    TL_CHECK(!reveal.dirty());
}

TL_TEST(automap_reveal_square_around_the_player) {
    const Grid grid(200, 200, true);  // 80 x 80 units, 21 x 21 vertices
    tl::core::AutomapReveal reveal;
    reveal.create(grid.texture(), false);
    reveal.finalize();
    // At (40, 40): fx = 40 * 0.25 - 3 = 7, so vertices 7..12 a side, the rim
    // (7 and 12) at 0.6 and 8..11 inside at 1.
    reveal.reveal_around(40.0f, 40.0f);
    TL_CHECK(reveal.revealed(6, 9) == 0.0f);
    TL_CHECK(reveal.revealed(7, 9) == 0.6f);
    TL_CHECK(reveal.revealed(8, 9) == 1.0f);
    TL_CHECK(reveal.revealed(11, 11) == 1.0f);
    TL_CHECK(reveal.revealed(12, 9) == 0.6f);
    TL_CHECK(reveal.revealed(13, 9) == 0.0f);
    TL_CHECK(reveal.revealed(9, 7) == 0.6f);
    // A vertex only brightens: moving on leaves the old square at 1.
    reveal.reveal_around(41.0f, 40.0f);
    TL_CHECK(reveal.revealed(8, 9) == 1.0f);
}

TL_TEST(automap_reveal_fades_twice_delta_and_stays_dirty_until_there) {
    const Grid grid(200, 200, true);
    tl::core::AutomapReveal reveal;
    reveal.create(grid.texture(), false);
    reveal.finalize();
    reveal.reveal_around(40.0f, 40.0f);
    tl::core::AutomapReveal::Step step = reveal.fade(0.1f);
    TL_CHECK(step.stepped && step.repaint);
    TL_CHECK(reveal.dirty());
    const auto row = static_cast<size_t>(reveal.tiles_x());
    const size_t inner = 9 * row + 9;
    const size_t rim = 9 * row + 7;
    TL_CHECK(reveal.opacity()[inner] == 0.2f);
    for (int i = 0; i < 10; ++i) reveal.fade(0.1f);
    TL_CHECK(reveal.opacity()[inner] == 1.0f);
    TL_CHECK(reveal.opacity()[rim] == 0.6f);
    TL_CHECK(!reveal.dirty());
    step = reveal.fade(0.1f);
    TL_CHECK(!step.stepped);
}

TL_TEST(automap_reveal_restore_needs_the_same_size) {
    const Grid grid(200, 200, true);
    tl::core::AutomapReveal reveal;
    reveal.create(grid.texture(), false);
    reveal.finalize();
    reveal.fade(0.0f);
    std::vector<float> cells(static_cast<size_t>(reveal.tiles_x() * reveal.tiles_y()), 1.0f);
    TL_CHECK(!reveal.restore(reveal.tiles_x() + 1, reveal.tiles_y(), cells));
    TL_CHECK(!reveal.dirty());
    TL_CHECK(reveal.restore(reveal.tiles_x(), reveal.tiles_y(), cells));
    TL_CHECK(reveal.dirty());
    TL_CHECK(reveal.revealed(3, 3) == 1.0f);
}
