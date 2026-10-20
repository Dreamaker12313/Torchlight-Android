#include "core/route.h"
#include "core/fused.h"

#include <cmath>

namespace tl::core {

namespace {

// A length as `CPath`'s functions take one: the first
// coordinate squared, the other two fused onto it, the axes in each one's own order.
float fused_length(float first, float second, float third) {
    return std::sqrt(std::fma(third, third, std::fma(second, second, first * first)));
}

/// One coordinate of `GetSplinePositionAtDistance`'s Catmull-Rom in the console's order,
/// before the origin is added.
float catmull_rom(float a, float b, float c, float d, float t, float t2, float t3) {
    const float five_b = b * 5.0f;                         // fmuls
    const float cubic = std::fma(b, 3.0f, -a);             // fmsubs   3b - a
    const float square = std::fma(a, 2.0f, -five_b);       // fmsubs   2a - 5b
    const float cubic2 = fnmsub(c, 3.0f, cubic);           // fnmsubs  (3b - a) - 3c
    const float linear = c - a;                            // fsubs
    const float square2 = std::fma(c, 4.0f, square);       // fmadds   2a - 5b + 4c
    const float cubic3 = cubic2 + d;                       // fadds
    const float square3 = square2 - d;                     // fsubs
    const float sum = std::fma(square3, t2, cubic3 * t3);  // fmuls, fmadds
    const float sum2 = std::fma(linear, t, sum);           // fmadds
    return std::fma(b, 2.0f, sum2) * 0.5f;                 // fmadds, fmuls
}

}  // namespace

// `CPath::Clear`.
void Route::clear() {
    points_.clear();
    running_.clear();
    length_ = 0.0f;
}

// `CPath::AddPoint`.
void Route::add_point(const Vec3& point) {
    points_.push_back(point);
    if (points_.size() < 2) {
        running_.push_back(0.0f);
        length_ = 0.0f;
        return;
    }
    const Vec3& to = points_[points_.size() - 1];
    const Vec3& from = points_[points_.size() - 2];
    // z, then y, then x.
    const float step = fused_length(to.z - from.z, to.y - from.y, to.x - from.x);
    length_ = length_ + step;
    running_.push_back(running_.back() + step);
}

// `CPath::GetPoint`.
Vec3 Route::point(size_t index) const {
    if (index < points_.size()) {
        const Vec3& p = points_[index];
        return {p.x + kOrigin.x, p.y + kOrigin.y, p.z + kOrigin.z};
    }
    return kOrigin;
}

// `CPath::FindNearestPoint`, open-path branch. The
// conditions follow the console's branches so a NaN (from a zero-length segment) goes the
// same way: `blt`/`bgt` only on ordered comparisons, `bge`/`ble` also on unordered.
bool Route::find_nearest_point(const Vec3& at_world, Vec3& nearest, float& distance) const {
    const size_t count = points_.size();
    if (count < 2) return false;
    float min_distance = 0.0f;
    if (length_ < 0.0f) min_distance = length_;
    const Vec3 at{at_world.x - kOrigin.x, at_world.y - kOrigin.y, at_world.z - kOrigin.z};
    const size_t segments = count - 1;

    nearest = points_[0];
    distance = 0.0f;
    float best = 99999.0f;
    for (size_t i = 0; i < segments; ++i) {
        if (best == 99999.0f && !(running_[i] < min_distance)) {
            const Vec3& p = points_[i];
            // y, then z, then x.
            const float d = fused_length(at.y - p.y, at.z - p.z, at.x - p.x);
            if (d < best) {
                distance = running_[i];
                nearest = p;
                best = d;
            }
        }
        const Vec3& a = points_[i];
        const size_t j = i + 1 < count ? i + 1 : i + 1 - count;
        const Vec3& b = points_[j];
        const float sx = b.x - a.x;
        const float sy = b.y - a.y;
        const float sz = b.z - a.z;
        // y, then z, then x, for both.
        const float span = std::sqrt(std::fma(sx, sx, std::fma(sz, sz, sy * sy)));
        const float dot = std::fma(sx, at.x - a.x, std::fma(sz, at.z - a.z, sy * (at.y - a.y)));
        float t = dot / (span * span);
        if (t < 0.0f || 1.0f < t) {
            float along = j == 0 ? length_ : running_[j];
            if (along < min_distance) {
                t = std::fma((min_distance - along) + 1.0f, 1.0f / span, t);
                if (1.0f < t) t = 1.0f;
                along = std::fma(t, span, running_[i]);
            }
            if (along < min_distance) continue;
            // y, then z, then x.
            const float d = fused_length(at.y - b.y, at.z - b.z, at.x - b.x);
            if (!(d < best)) continue;
            distance = along;
            nearest = b;
            best = d;
            continue;
        }
        float along = std::fma(t, span, running_[i]);
        if (along < min_distance) {
            t = std::fma((min_distance - along) + 1.0f, 1.0f / span, t);
            if (1.0f < t) t = 1.0f;
            along = std::fma(t, span, running_[i]);
        }
        if (along < min_distance) continue;
        const Vec3 on{a.x + sx * t, a.y + sy * t, a.z + sz * t};
        // z, then x, then y.
        const float d = fused_length(at.z - on.z, at.x - on.x, at.y - on.y);
        if (d < best) {
            distance = along;
            nearest = on;
            best = d;
        }
    }
    if (best == 99999.0f) return false;
    nearest = {nearest.x + kOrigin.x, nearest.y + kOrigin.y, nearest.z + kOrigin.z};
    return true;
}

// `CPath::GetPositionAtDistance`, open path.
Vec3 Route::position_at_distance(float distance) const {
    if (distance < 0.0f) {
        do {
            distance = length_ + distance;
        } while (distance < 0.0f);
    }
    while (length_ < distance) distance = distance - length_;
    // Note: the console has no such test. On a path with no points it returns
    // what is on its stack; no caller asks one.
    if (points_.empty()) return kOrigin;
    if (distance == length_) return point(points_.size() - 1);
    if (distance == 0.0f) return point(0);
    const size_t count = points_.size();
    for (size_t i = 0; i < count; ++i) {
        const size_t next = i + 1 < count ? i + 1 : i + 1 - count;
        if (distance < running_[next] || next == 0) {
            const Vec3& to = points_[next];
            const Vec3& from = points_[i];
            float dx = to.x - from.x;
            float dy = to.y - from.y;
            float dz = to.z - from.z;
            // y, then z, then x, and the threshold
            // is a double (`lfd`, 1e-8), not a float.
            const float span = fused_length(dy, dz, dx);
            if (1e-08 < static_cast<double>(span)) {
                const float inv = 1.0f / span;
                dx = dx * inv;
                dy = dy * inv;
                dz = dz * inv;
            }
            const float along = distance - running_[i];
            return {kOrigin.x + (from.x + dx * along), kOrigin.y + (from.y + dy * along),
                    kOrigin.z + (from.z + dz * along)};
        }
    }
    return kOrigin;
}

// `CPath::GetSplinePositionAtDistance`, open
// path.
Vec3 Route::spline_position_at_distance(float distance) const {
    if (length_ == 0.0f) return kOrigin;
    // The wrap is by repeated steps, as the original's: an infinite distance never leaves.
    for (; distance < 0.0f; distance = length_ + distance) {}
    for (; length_ < distance; distance = distance - length_) {}
    const int count = static_cast<int>(points_.size());
    // The last point whose running length is short of `distance`, from the
    // end; the first when none.
    int segment = 0;
    for (int i = count - 1; i >= 0; --i) {
        if (running_[static_cast<size_t>(i)] < distance) {
            segment = i;
            break;
        }
    }
    if (distance == length_) return point(static_cast<size_t>(count - 1));
    if (distance == 0.0f) return point(0);

    int before = segment - 1;
    int after = segment + 1;
    int beyond = segment + 2;
    if (before < 0) before = 0;
    if (after >= count) after = count - 1;
    if (beyond >= count) beyond = count - 1;
    const float base = running_[static_cast<size_t>(segment)];
    float span = running_[static_cast<size_t>(after)] - base;
    if (span < 0.0f) span = length_ + span;
    float t = distance - base;
    if (distance < base) t = length_ + t;
    t = t / span;
    const float t2 = t * t;
    const float t3 = t2 * t;
    const Vec3& p0 = points_[static_cast<size_t>(before)];
    const Vec3& p1 = points_[static_cast<size_t>(segment)];
    const Vec3& p2 = points_[static_cast<size_t>(after)];
    const Vec3& p3 = points_[static_cast<size_t>(beyond)];
    return {catmull_rom(p0.x, p1.x, p2.x, p3.x, t, t2, t3) + kOrigin.x,
            catmull_rom(p0.y, p1.y, p2.y, p3.y, t, t2, t3) + kOrigin.y,
            catmull_rom(p0.z, p1.z, p2.z, p3.z, t, t2, t3) + kOrigin.z};
}

}  // namespace tl::core
