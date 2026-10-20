#include "core/automap_grid.h"

#include "core/passability.h"

#include <algorithm>
#include <cmath>

namespace tl::core {

// CLevel::createAutomapTexture
AutomapTexture create_automap_texture(const AutomapGridShape& shape,
                                      const std::function<bool(int, int)>& passable) {
    AutomapTexture texture;
    const uint32_t wide = shape.cells_wide;
    const uint32_t high = shape.cells_high;
    if (wide == 0) return texture;

    texture.origin_z = shape.top_left_z;
    texture.extent_x = shape.bottom_right_x - shape.top_left_x;
    texture.extent_z = shape.bottom_right_z - shape.top_left_z;
    texture.origin_x = shape.top_left_x;

    // One pixel per two cells, the texture the next power of two up from that,
    // never under 2.
    const uint32_t half_wide = wide >> 1;
    const uint32_t half_high = high >> 1;
    uint32_t tex_w = 2;
    while (tex_w < half_wide) tex_w <<= 1;
    uint32_t tex_h = 2;
    while (tex_h < half_high) tex_h <<= 1;
    texture.used_u =
        static_cast<float>(static_cast<double>(half_wide) / static_cast<double>(tex_w));
    texture.used_v =
        static_cast<float>(static_cast<double>(half_high) / static_cast<double>(tex_h));
    // Past 512 either way, half the size, one pixel per four cells. The used
    // fractions above are not taken again.
    int step = 2;
    if (tex_w > 512 || tex_h > 512) {
        tex_w >>= 1;
        tex_h >>= 1;
        step = 4;
    }
    texture.width = tex_w;
    texture.height = tex_h;
    texture.step = step;

    constexpr uint32_t kBytes = 3;  // PF_R8G8B8
    const uint32_t row = kBytes * tex_w;
    const uint32_t total = row * tex_h;
    texture.rgb.assign(total, 0);

    // The writes are bounded the console's way, by the byte offset rather than
    // by the column: a pixel is written at an offset up to and including the
    // row's end, so the sample one past a row's last pixel lands on the next
    // row's first (which that row then overwrites), and the first offset at or
    // past the whole image ends the picture.
    bool full = false;
    uint32_t row_start = 0;
    uint32_t row_end = row;
    for (uint32_t y = 0; y < high; y += static_cast<uint32_t>(step)) {
        uint32_t at = row_start;
        for (uint32_t x = 0; x < wide; x += static_cast<uint32_t>(step)) {
            if (at > row_end) break;
            if (at >= total) {
                full = true;
                break;
            }
            const int cx = static_cast<int>(x);
            const int cz = static_cast<int>(y);
            uint8_t grey = 0;
            if (passable(cx, cz)) {
                // A walkable cell is an edge unless all eight cells `step` away
                // are walkable too.
                const bool interior =
                    passable(cx - step, cz) && passable(cx + step, cz) && passable(cx, cz - step) &&
                    passable(cx, cz + step) && passable(cx - step, cz - step) &&
                    passable(cx + step, cz - step) && passable(cx - step, cz + step) &&
                    passable(cx + step, cz + step);
                grey = interior ? 0x80 : 200;
            }
            texture.rgb[at] = grey;
            texture.rgb[at + 1] = grey;
            texture.rgb[at + 2] = grey;
            at += kBytes;
            if (at > row_end) break;
            if (at >= total) {
                full = true;
                break;
            }
        }
        if (full) break;
        row_start += row;
        row_end += row;
    }
    return texture;
}

AutomapTexture create_automap_texture(const PassabilityMap& map) {
    if (!map.ready()) return {};
    AutomapGridShape shape;
    shape.cells_wide = static_cast<uint32_t>(map.width());
    shape.cells_high = static_cast<uint32_t>(map.height());
    shape.top_left_x = map.origin_x();
    shape.top_left_z = map.origin_z();
    shape.bottom_right_x = map.far_x();
    shape.bottom_right_z = map.far_z();
    return create_automap_texture(shape, [&map](int x, int z) { return map.map_passable(x, z); });
}

// CAutomap::create, the corners and the exposed flag.
void AutomapReveal::create(const AutomapTexture& texture, bool exposed) {
    exposed_ = exposed;
    min_x_ = texture.origin_x;
    min_z_ = texture.origin_z;
    max_x_ = texture.extent_x + texture.origin_x;
    max_z_ = texture.extent_z + texture.origin_z;
}

// CAutomap::finalize
void AutomapReveal::finalize() {
    tiles_x_ = static_cast<int>(std::fma(max_x_ - min_x_, 0.25f, 1.0f));
    tiles_y_ = static_cast<int>(std::fma(max_z_ - min_z_, 0.25f, 1.0f));
    const size_t cells = tiles_x_ > 0 && tiles_y_ > 0
                             ? static_cast<size_t>(tiles_x_) * static_cast<size_t>(tiles_y_)
                             : 0;
    revealed_.assign(cells, 0.0f);
    opacity_.assign(cells, 0.0f);
    dirty_ = true;
    if (exposed_) std::fill(revealed_.begin(), revealed_.end(), 1.0f);
}

// CAutomap::setPlayerPosition, the reveal.
void AutomapReveal::reveal_around(float x, float z) {
    if (revealed_.empty()) return;
    // `fmsubs`: (p - min) * 0.25 - 3, one rounding.
    const float fx = std::fma(x - min_x_, 0.25f, -3.0f);
    const float fz = std::fma(z - min_z_, 0.25f, -3.0f);
    const int cells = tiles_x_ * tiles_y_;
    for (int cx = static_cast<int>(fx); static_cast<float>(cx) < fx + 6.0f; ++cx) {
        for (int cz = static_cast<int>(fz); static_cast<float>(cz) < fz + 6.0f; ++cz) {
            const float px = static_cast<float>(cx);
            const float pz = static_cast<float>(cz);
            const bool inside = px > fx && pz > fz && px < fx + 5.0f && pz < fz + 5.0f;
            const float value = inside ? 1.0f : 0.6f;
            const int index = tiles_x_ * cz + cx;
            if (index >= cells || cx < 0 || cz < 0) continue;
            float& cell = revealed_[static_cast<size_t>(index)];
            if (cell < value) {
                cell = value;
                dirty_ = true;
            }
        }
    }
}

// CAutomap::getRevealed
float AutomapReveal::revealed(int x, int z) const {
    const int index = tiles_x_ * z + x;
    if (index >= tiles_x_ * tiles_y_ || x < 0 || z < 0) return 0.0f;
    return revealed_[static_cast<size_t>(index)];
}

// CAutomap::update, the vertices' fade.
AutomapReveal::Step AutomapReveal::fade(float delta) {
    Step step;
    if (!dirty_) return step;
    step.stepped = true;
    dirty_ = false;
    if (exposed_) return step;
    for (int z = 0; z < tiles_y_; ++z) {
        for (int x = 0; x < tiles_x_; ++x) {
            const size_t index =
                static_cast<size_t>(tiles_x_) * static_cast<size_t>(z) + static_cast<size_t>(x);
            float& opacity = opacity_[index];
            // A vertex at 100 is left alone; nothing in the console sets one.
            if (opacity == 100.0f) continue;
            const float target = revealed(x, z);
            if (opacity == target) continue;
            step.repaint = true;
            opacity = std::fma(delta, 2.0f, opacity);
            if (!(opacity < target)) opacity = target;
            if (opacity != target) dirty_ = true;
        }
    }
    return step;
}

// PORT: a still frame's reveal, at once.
bool AutomapReveal::settle() {
    if (exposed_ || opacity_ == revealed_) return false;
    opacity_ = revealed_;
    dirty_ = false;
    return true;
}

// CLevelState::restoreAutomap
bool AutomapReveal::restore(int32_t tiles_x, int32_t tiles_y, const std::vector<float>& cells) {
    if (tiles_x != tiles_x_ || tiles_y != tiles_y_ || cells.size() != revealed_.size())
        return false;
    revealed_ = cells;
    dirty_ = true;
    return true;
}

// PORT: the reveal emptied for the next level.
void AutomapReveal::clear() { *this = AutomapReveal(); }

}  // namespace tl::core
