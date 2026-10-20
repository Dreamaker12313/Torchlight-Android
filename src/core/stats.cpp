#include "core/stats.h"

#include "core/msvc_sort.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace tl::core {

namespace {

// ASCII upper-casing: the case-blind key.
// PORT: ASCII case folding, the case-blind compare of names in the port's narrow strings.
std::string key_of(std::string_view name) {
    std::string out(name);
    for (char& c : out) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return out;
}

}  // namespace

// CGraph::CGraph
StatGraph::StatGraph() = default;

StatGraph::StatGraph(GraphType type, std::string name, uint32_t line_limit)
    : name_(std::move(name)), type_(type), line_limit_(line_limit) {}

// CGraph::clear: only `DynamicAttributeCurved::removeAllControlPoints`,
// below the line limit; max_x, last_y, slopes, range and tangents survive.
void StatGraph::clear(unsigned int line_idx) {
    if (line_idx >= line_limit_ || line_idx >= lines_.size()) return;
    lines_[line_idx].points.clear();
}

// CGraph::addGraphLine
void StatGraph::add_line(GraphType type) {
    Line line;
    line.type = type;
    lines_.push_back(std::move(line));
    line_limit_ = static_cast<uint32_t>(lines_.size());
}

// CGraph::addValue
void StatGraph::add_point(float x, float y, unsigned int line_idx) {
    if (x > max_x_) {
        max_x_ = x;
    }
    while (lines_.size() <= line_idx) {
        add_line(type_);
    }
    if (line_idx == 0) {
        last_y_0_ = std::max(last_y_0_, y);
    } else if (line_idx == 1) {
        last_y_1_ = std::max(last_y_1_, y);
    }
    dirty_ = true;
    lines_[line_idx].points.push_back(Point{x, y});
}

// `CGraph::processPoints`; per line
// `ParticleUniverse::DynamicAttributeCurved::processControlPoints`,
// tangents as `Ogre::SimpleSpline::recalcTangents`.
void StatGraph::process_points() {
    for (size_t l = 0; l < lines_.size(); ++l) {
        Line& line = lines_[l];
        // The console's `std::sort`: ties keep its order.
        msvc_sort(line.points.begin(), line.points.end(),
                  [](const Point& a, const Point& b) { return a.x < b.x; });
        if (!line.points.empty()) {
            line.range = line.points.back().x - line.points.front().x;
            if (line.type == GraphType::Curved && line.points.size() >= 2) {
                // Closed when first == last: the first tangent reaches back to the
                // last point but one, and the last tangent is the first.
                const size_t n = line.points.size();
                const Point& first = line.points[0];
                const Point& last = line.points[n - 1];
                const bool closed = first.x == last.x && first.y == last.y;
                line.tangents.resize(n);
                for (size_t i = 0; i < n; ++i) {
                    if (i == 0) {
                        const float before = closed ? line.points[n - 2].y : line.points[0].y;
                        line.tangents[i] = (line.points[1].y - before) * 0.5f;
                    } else if (i == n - 1) {
                        line.tangents[i] = closed
                                               ? line.tangents[0]
                                               : (line.points[i].y - line.points[i - 1].y) * 0.5f;
                    } else {
                        line.tangents[i] = (line.points[i + 1].y - line.points[i - 1].y) * 0.5f;
                    }
                }
            }
        }
    }
    dirty_ = false;
}

// CGraph::getControlPoints
size_t StatGraph::size() const {
    size_t count = 0;
    for (const auto& line : lines_) {
        count = std::max(count, line.points.size());
    }
    return count;
}

// PORT: an accessor of a curve.
size_t StatGraph::size(unsigned int line_idx) const {
    if (line_idx < lines_.size()) return lines_[line_idx].points.size();
    return 0;
}

// PORT: an accessor of a curve.
bool StatGraph::empty() const {
    if (lines_.empty()) return true;
    for (const auto& line : lines_) {
        if (!line.points.empty()) return false;
    }
    return true;
}

// PORT: an accessor of a curve.
const std::vector<StatGraph::Point>& StatGraph::points(unsigned int line_idx) const {
    static const std::vector<Point> empty_points;
    if (line_idx < lines_.size()) return lines_[line_idx].points;
    return empty_points;
}

// CGraph::loadGraph, CGraph::loadGraphLine
StatGraph StatGraph::from(const tl::formats::AdmSection& section) {
    StatGraph graph;
    graph.name_ = section.string_or("NAME", "");

    std::string type_str = section.string_or("TYPE", "LINE");
    for (char& c : type_str) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    const bool curved = section.bool_or("CURVED", false);
    graph.type_ = curved ? GraphType::Curved : GraphType::Linear;
    graph.infer_past_end_ = section.bool_or("INFER_PASSED_END", false);

    // Any TYPE but LINE: two lines, MINY and MAXY.
    const bool two_lines = (type_str != "LINE");
    if (two_lines) {
        graph.add_line(graph.type_);
        graph.add_line(graph.type_);
    } else {
        graph.add_line(graph.type_);
    }

    // The original walks every child without checking its name; a `[LINE]`
    // holds only `[POINT]`s, so filtering on the name is the same walk.
    size_t point_count = 0;
    for (const auto& child : section.children) {
        if (child.name == "POINT") ++point_count;
    }

    float prev_y0 = 0.0f;
    float prev_y1 = 0.0f;
    size_t cur_idx = 0;

    for (const auto& point : section.children) {
        if (point.name != "POINT") continue;
        const float x = point.float_or("X", 0.0f);

        if (!two_lines) {
            const float y = point.float_or("Y", 0.0f);
            graph.add_point(x, y, 0);
            if (cur_idx == point_count - 1) {
                // Quirk: the slope is the last step in Y, not divided by the step in X;
                // previous Ys start at 0.
                graph.slope_0_ = y - prev_y0;
                graph.slope_1_ = y - prev_y1;
            }
            prev_y0 = y;
            prev_y1 = y;
        } else {
            const float min_y = point.float_or("MINY", 0.0f);
            const float max_y = point.float_or("MAXY", 0.0f);
            graph.add_point(x, min_y, 0);
            graph.add_point(x, max_y, 1);
            if (cur_idx == point_count - 1) {
                graph.slope_0_ = min_y - prev_y0;
                graph.slope_1_ = max_y - prev_y1;
            }
            prev_y0 = min_y;
            prev_y1 = max_y;
        }
        ++cur_idx;
    }

    graph.process_points();
    return graph;
}

namespace {

/// `Ogre::SimpleSpline`'s Hermite basis (its constructor's `mCoeffs`).
constexpr float kHermite[4][4] = {{2.0f, -2.0f, 1.0f, 1.0f},
                                  {-3.0f, 3.0f, -2.0f, -1.0f},
                                  {0.0f, 0.0f, 1.0f, 0.0f},
                                  {1.0f, 0.0f, 0.0f, 0.0f}};

/// Column `j` of the row vector `v` times the matrix `m`, fused in the
/// console's order (`Vector4 * Matrix4`).
float row_times_column(const float (&v)[4], const float (&m)[4][4], int j) {
    if (j == 0)
        return std::fma(v[0], m[0][0],
                        std::fma(m[3][0], v[3], std::fma(m[1][0], v[1], m[2][0] * v[2])));
    return std::fma(m[3][j], v[3],
                    std::fma(m[2][j], v[2], std::fma(m[0][j], v[0], m[1][j] * v[1])));
}

}  // namespace

// `CGraph::getValue`: the console reads line 0 only.
float StatGraph::at(float x) const {
    if (infer_past_end_ && x > max_x_) return std::fma(slope_0_, x - max_x_, last_y_0_);
    if (lines_.empty()) return 0.0f;
    const Line& line = lines_[0];

    // `ParticleUniverse::DynamicAttributeCurved::getValue`.
    //
    const std::vector<Point>& points = line.points;
    if (points.empty()) return 0.0f;
    const size_t n = points.size();
    if (line.type == GraphType::Curved) {
        // Past the range, a zero range or a NaN: the end of the spline.
        const float range = line.range;
        const float f = x / range;
        float t = 1.0f;
        if (range != 0.0f && f < 1.0f) t = f;
        // `Ogre::SimpleSpline::interpolate(Real)`: the
        // segment truncated to 64 bits and its low word kept.
        const float segment =
            static_cast<float>(static_cast<uint64_t>(static_cast<uint32_t>(n - 1))) * t;
        // DIVERGENCE: at or below -1 the console's index wraps and reads before the
        // first point; the port answers that point.
        if (!(segment > -1.0f)) return points[0].y;
        const uint32_t index = static_cast<uint32_t>(static_cast<int64_t>(segment));
        const float s = segment - static_cast<float>(index);

        // `Ogre::SimpleSpline::interpolate(unsigned, Real)`.
        if (index + 1 == n || s == 0.0f) return points[index].y;
        if (s == 1.0f) return points[index + 1].y;
        const float s2 = s * s;
        const float powers[4] = {s2 * s, s2, s, 1.0f};
        const float c[4] = {
            row_times_column(powers, kHermite, 0), row_times_column(powers, kHermite, 1),
            row_times_column(powers, kHermite, 2), row_times_column(powers, kHermite, 3)};
        const float tan_a = index < line.tangents.size() ? line.tangents[index] : 0.0f;
        const float tan_b = index + 1 < line.tangents.size() ? line.tangents[index + 1] : 0.0f;
        // Then by the rows point, next point, tangent, next tangent: Y only.
        return std::fma(
            tan_b, c[3],
            std::fma(tan_a, c[2], std::fma(points[index].y, c[0], points[index + 1].y * c[1])));
    }

    // Linear: from the point before the first one past X, to the next; unfused.
    size_t i = 0;
    while (i < n && !(x < points[i].x)) ++i;
    if (i != 0) --i;
    if (i + 1 == n || !(points[i].x < x)) return points[i].y;
    const Point& a = points[i];
    const Point& b = points[i + 1];
    return ((b.y - a.y) * (x - a.x)) / (b.x - a.x) + a.y;
}

// CGraphManager::loadGraph
void StatBook::add(const std::string& name, StatGraph graph) {
    if (name.empty()) return;
    by_name_[key_of(name)] = std::move(graph);
}

// PORT: a curve kept by its file name too.
void StatBook::add_by_filename(const std::string& filename, StatGraph graph) {
    if (filename.empty()) return;
    by_filename_[key_of(filename)] = std::move(graph);
}

// CGraphManager::getGraph
const StatGraph* StatBook::find(const std::string& name) const {
    const auto it = by_name_.find(key_of(name));
    return it == by_name_.end() ? nullptr : &it->second;
}

// CGraphManager::getGraphByFileName
const StatGraph* StatBook::find_by_filename(const std::string& filename) const {
    const auto it = by_filename_.find(key_of(filename));
    return it == by_filename_.end() ? nullptr : &it->second;
}

// `CGraph::getValue(x, 0)` on a curve by name, or the fallback. PORT: the lookup.
float StatBook::value(const std::string& name, float x, float fallback) const {
    const StatGraph* graph = find(name);
    if (graph == nullptr || graph->empty()) return fallback;
    return graph->at(x);
}

// CGraphManager::clear
void StatBook::clear() {
    by_name_.clear();
    by_filename_.clear();
}

// `CGraphManager::reload()` re-reads the curves
// from disk; the original's singleton is not kept.
// PORT: here the cache is only emptied.
void StatBook::reload() { clear(); }

}  // namespace tl::core
