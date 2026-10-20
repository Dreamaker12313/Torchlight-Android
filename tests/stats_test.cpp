#include "core/stats.h"
#include "formats/adm.h"
#include "test_support.h"

#include <cmath>
#include <string>
#include <vector>

namespace {

tl::formats::AdmSection make_line_section(const std::string& name,
                                          const std::vector<std::pair<float, float>>& points,
                                          bool curved = false, bool infer_past_end = false) {
    tl::formats::AdmSection section;
    section.name = "LINE";

    tl::formats::AdmRecord name_rec;
    name_rec.key = "NAME";
    name_rec.type = tl::formats::AdmType::String;
    name_rec.text = name;
    section.records.push_back(name_rec);

    tl::formats::AdmRecord type_rec;
    type_rec.key = "TYPE";
    type_rec.type = tl::formats::AdmType::String;
    type_rec.text = "LINE";
    section.records.push_back(type_rec);

    tl::formats::AdmRecord curved_rec;
    curved_rec.key = "CURVED";
    curved_rec.type = tl::formats::AdmType::Bool;
    curved_rec.integer = curved ? 1 : 0;
    section.records.push_back(curved_rec);

    tl::formats::AdmRecord infer_rec;
    infer_rec.key = "INFER_PASSED_END";
    infer_rec.type = tl::formats::AdmType::Bool;
    infer_rec.integer = infer_past_end ? 1 : 0;
    section.records.push_back(infer_rec);

    for (const auto& pt : points) {
        tl::formats::AdmSection pt_sec;
        pt_sec.name = "POINT";

        tl::formats::AdmRecord x_rec;
        x_rec.key = "X";
        x_rec.type = tl::formats::AdmType::Float;
        x_rec.number = pt.first;
        pt_sec.records.push_back(x_rec);

        tl::formats::AdmRecord y_rec;
        y_rec.key = "Y";
        y_rec.type = tl::formats::AdmType::Float;
        y_rec.number = pt.second;
        pt_sec.records.push_back(y_rec);

        section.children.push_back(pt_sec);
    }
    return section;
}

tl::formats::AdmSection make_range_section(
    const std::string& name, const std::vector<std::tuple<float, float, float>>& points,
    bool infer_past_end = false) {
    tl::formats::AdmSection section;
    section.name = "LINE";

    tl::formats::AdmRecord name_rec;
    name_rec.key = "NAME";
    name_rec.type = tl::formats::AdmType::String;
    name_rec.text = name;
    section.records.push_back(name_rec);

    tl::formats::AdmRecord type_rec;
    type_rec.key = "TYPE";
    type_rec.type = tl::formats::AdmType::String;
    type_rec.text = "RANGE";
    section.records.push_back(type_rec);

    tl::formats::AdmRecord infer_rec;
    infer_rec.key = "INFER_PASSED_END";
    infer_rec.type = tl::formats::AdmType::Bool;
    infer_rec.integer = infer_past_end ? 1 : 0;
    section.records.push_back(infer_rec);

    for (const auto& pt : points) {
        tl::formats::AdmSection pt_sec;
        pt_sec.name = "POINT";

        tl::formats::AdmRecord x_rec;
        x_rec.key = "X";
        x_rec.type = tl::formats::AdmType::Float;
        x_rec.number = std::get<0>(pt);
        pt_sec.records.push_back(x_rec);

        tl::formats::AdmRecord miny_rec;
        miny_rec.key = "MINY";
        miny_rec.type = tl::formats::AdmType::Float;
        miny_rec.number = std::get<1>(pt);
        pt_sec.records.push_back(miny_rec);

        tl::formats::AdmRecord maxy_rec;
        maxy_rec.key = "MAXY";
        maxy_rec.type = tl::formats::AdmType::Float;
        maxy_rec.number = std::get<2>(pt);
        pt_sec.records.push_back(maxy_rec);

        section.children.push_back(pt_sec);
    }
    return section;
}

bool approx_eq(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

}  // namespace

TL_TEST(stats_linear_exact_points_and_interpolation) {
    const auto sec =
        make_line_section("TEST_LINEAR", {{1.0f, 10.0f}, {2.0f, 20.0f}, {4.0f, 40.0f}});
    const tl::core::StatGraph graph = tl::core::StatGraph::from(sec);

    TL_CHECK(!graph.empty());
    TL_CHECK_EQ(graph.size(), 3u);
    TL_CHECK(graph.type() == tl::core::GraphType::Linear);

    // Exact points
    TL_CHECK(approx_eq(graph.at(1.0f), 10.0f));
    TL_CHECK(approx_eq(graph.at(2.0f), 20.0f));
    TL_CHECK(approx_eq(graph.at(4.0f), 40.0f));

    // Linear interpolation
    TL_CHECK(approx_eq(graph.at(1.5f), 15.0f));
    TL_CHECK(approx_eq(graph.at(3.0f), 30.0f));
}

TL_TEST(stats_below_first_point_clamps) {
    const auto sec = make_line_section("CLAMP_LOW", {{2.0f, 50.0f}, {5.0f, 100.0f}});
    const tl::core::StatGraph graph = tl::core::StatGraph::from(sec);

    TL_CHECK(approx_eq(graph.at(2.0f), 50.0f));
    TL_CHECK(approx_eq(graph.at(1.0f), 50.0f));
    TL_CHECK(approx_eq(graph.at(0.0f), 50.0f));
    TL_CHECK(approx_eq(graph.at(-10.0f), 50.0f));
}

TL_TEST(stats_past_last_point_without_infer_clamps) {
    const auto sec = make_line_section("CLAMP_HIGH", {{1.0f, 10.0f}, {3.0f, 30.0f}}, false, false);
    const tl::core::StatGraph graph = tl::core::StatGraph::from(sec);

    TL_CHECK(!graph.infer_past_end());
    TL_CHECK(approx_eq(graph.at(3.0f), 30.0f));
    TL_CHECK(approx_eq(graph.at(4.0f), 30.0f));
    TL_CHECK(approx_eq(graph.at(100.0f), 30.0f));
}

TL_TEST(stats_past_last_point_with_infer_extrapolates) {
    // Points (1, 10), (2, 20), (3, 35) with delta X = 1.0
    // slope = 35 - 20 = 15.
    const auto sec = make_line_section("EXTRAPOLATE", {{1.0f, 10.0f}, {2.0f, 20.0f}, {3.0f, 35.0f}},
                                       false, true);
    const tl::core::StatGraph graph = tl::core::StatGraph::from(sec);

    TL_CHECK(graph.infer_past_end());
    TL_CHECK(approx_eq(graph.slope(), 15.0f));
    TL_CHECK(approx_eq(graph.at(3.0f), 35.0f));
    // At x = 4: 35 + (4 - 3) * 15 = 50
    TL_CHECK(approx_eq(graph.at(4.0f), 50.0f));
    // At x = 5: 35 + (5 - 3) * 15 = 65
    TL_CHECK(approx_eq(graph.at(5.0f), 65.0f));
}

TL_TEST(stats_infer_slope_does_not_divide_by_dx) {
    // slope = last_y - prev_y, regardless of delta X (10 here, not 5).
    const auto sec = make_line_section("SPAN_DIFF", {{1.0f, 10.0f}, {3.0f, 20.0f}}, false, true);
    const tl::core::StatGraph graph = tl::core::StatGraph::from(sec);

    TL_CHECK(approx_eq(graph.slope(), 10.0f));
    // At x = 4: last_y(20) + (4 - 3) * 10 = 30
    TL_CHECK(approx_eq(graph.at(4.0f), 30.0f));
}

TL_TEST(stats_boundary_zero_points) {
    tl::core::StatGraph graph;
    TL_CHECK(graph.empty());
    TL_CHECK_EQ(graph.size(), 0u);
    TL_CHECK(approx_eq(graph.at(0.0f), 0.0f));
    TL_CHECK(approx_eq(graph.at(5.0f), 0.0f));
}

TL_TEST(stats_boundary_one_point) {
    // One-point curve: Stat_Points_Per_Level has 1 point at Y=5.
    // A single point defines a constant curve across all levels.
    const auto sec_no_infer = make_line_section("ONE_NO_INFER", {{1.0f, 5.0f}}, false, false);
    const tl::core::StatGraph g1 = tl::core::StatGraph::from(sec_no_infer);
    TL_CHECK_EQ(g1.size(), 1u);
    TL_CHECK(approx_eq(g1.at(0.5f), 5.0f));
    TL_CHECK(approx_eq(g1.at(1.0f), 5.0f));
    TL_CHECK(approx_eq(g1.at(10.0f), 5.0f));

    // With INFER_PASSED_END both previous Ys start at 0, so the slope is the
    // point's own Y. Kept.
    const auto sec_infer = make_line_section("ONE_INFER", {{1.0f, 5.0f}}, false, true);
    const tl::core::StatGraph g2 = tl::core::StatGraph::from(sec_infer);
    TL_CHECK_EQ(g2.size(), 1u);
    TL_CHECK(approx_eq(g2.slope(), 5.0f));
    TL_CHECK(approx_eq(g2.at(0.5f), 5.0f));
    TL_CHECK(approx_eq(g2.at(1.0f), 5.0f));
    TL_CHECK(approx_eq(g2.at(2.0f), 10.0f));
    TL_CHECK(approx_eq(g2.at(10.0f), 50.0f));
}

TL_TEST(stats_clear_empties_points_and_keeps_the_rest) {
    // `CGraph::clear` only removes the control points; the extrapolation state
    // survives.
    const auto sec = make_line_section("CLEARED", {{1.0f, 10.0f}, {2.0f, 20.0f}}, false, true);
    tl::core::StatGraph graph = tl::core::StatGraph::from(sec);
    graph.clear(0);
    TL_CHECK(graph.empty());
    TL_CHECK(approx_eq(graph.max_x(), 2.0f));
    TL_CHECK(approx_eq(graph.slope(), 10.0f));
    TL_CHECK(approx_eq(graph.at(1.5f), 0.0f));
    TL_CHECK(approx_eq(graph.at(3.0f), 30.0f));
    // Past the line limit nothing happens.
    tl::core::StatGraph other = tl::core::StatGraph::from(sec);
    other.clear(1);
    TL_CHECK_EQ(other.size(), 2u);
}

TL_TEST(stats_add_point_does_not_derive_a_slope) {
    // `addValue` tracks max X and the largest Y but never a slope;
    // only `loadGraphLine` sets one.
    tl::core::StatGraph graph(tl::core::GraphType::Linear);
    graph.set_infer_past_end(true);
    graph.add_point(1.0f, 10.0f);
    graph.add_point(2.0f, 20.0f);
    graph.process_points();
    TL_CHECK(approx_eq(graph.slope(), 0.0f));
    TL_CHECK(approx_eq(graph.at(1.5f), 15.0f));
    TL_CHECK(approx_eq(graph.at(5.0f), 20.0f));
}

TL_TEST(stats_duplicate_x_last_wins) {
    // Two points at x = 1: (1, 10) and (1, 20). Next point (2, 30).
    const auto sec = make_line_section("DUP_X", {{1.0f, 10.0f}, {1.0f, 20.0f}, {2.0f, 30.0f}});
    const tl::core::StatGraph graph = tl::core::StatGraph::from(sec);

    TL_CHECK(approx_eq(graph.at(1.0f), 20.0f));
    TL_CHECK(approx_eq(graph.at(1.5f), 25.0f));
    TL_CHECK(approx_eq(graph.at(2.0f), 30.0f));
}

TL_TEST(stats_points_out_of_order_are_sorted) {
    // Points added out of order: (3, 30), (1, 10), (2, 20).
    const auto sec = make_line_section("UNSORTED", {{3.0f, 30.0f}, {1.0f, 10.0f}, {2.0f, 20.0f}});
    const tl::core::StatGraph graph = tl::core::StatGraph::from(sec);

    TL_CHECK(approx_eq(graph.at(1.0f), 10.0f));
    TL_CHECK(approx_eq(graph.at(1.5f), 15.0f));
    TL_CHECK(approx_eq(graph.at(2.0f), 20.0f));
    TL_CHECK(approx_eq(graph.at(2.5f), 25.0f));
    TL_CHECK(approx_eq(graph.at(3.0f), 30.0f));
}

TL_TEST(stats_curved_spline_evaluation) {
    // Catmull-Rom spline on 4 points: (0, 0), (1, 1), (2, 4), (3, 9)
    const auto sec =
        make_line_section("SPLINE", {{0.0f, 0.0f}, {1.0f, 1.0f}, {2.0f, 4.0f}, {3.0f, 9.0f}}, true);
    const tl::core::StatGraph graph = tl::core::StatGraph::from(sec);

    TL_CHECK(graph.type() == tl::core::GraphType::Curved);
    // Endpoints and control points match
    TL_CHECK(approx_eq(graph.at(0.0f), 0.0f));
    TL_CHECK(approx_eq(graph.at(1.0f), 1.0f));
    TL_CHECK(approx_eq(graph.at(2.0f), 4.0f));
    TL_CHECK(approx_eq(graph.at(3.0f), 9.0f));

    // Intermediate point is smooth and non-linear
    const float mid = graph.at(1.5f);
    TL_CHECK(mid > 1.0f && mid < 4.0f);
}

TL_TEST(stats_two_lines_miny_maxy) {
    // Multi-line range graph: line 0 is MINY, line 1 is MAXY
    const auto sec =
        make_range_section("RANGE_GRAPH", {{1.0f, 10.0f, 20.0f}, {2.0f, 30.0f, 50.0f}}, true);
    const tl::core::StatGraph graph = tl::core::StatGraph::from(sec);

    TL_CHECK_EQ(graph.num_lines(), 2u);
    // Both lines are kept (MINY, MAXY), each with its own slope past the end.
    TL_CHECK_EQ(graph.points(1).size(), 2u);
    TL_CHECK(approx_eq(graph.slope(0), 20.0f));
    TL_CHECK(approx_eq(graph.slope(1), 30.0f));
    // `CGraph::getValue` on the console reads line 0 (MINY) only.
    TL_CHECK(approx_eq(graph.at(1.0f), 10.0f));
    TL_CHECK(approx_eq(graph.at(2.0f), 30.0f));
    // Extrapolated past end: slope 30 - 10 = 20, so at x = 3: 30 + 20 = 50.
    TL_CHECK(approx_eq(graph.at(3.0f), 50.0f));
}

TL_TEST(stats_stat_book_case_insensitivity_and_lookup) {
    tl::core::StatBook book;
    const auto sec = make_line_section("Health_Player_Alchemist", {{1.0f, 200.0f}, {2.0f, 240.0f}});
    book.add("Health_Player_Alchemist", tl::core::StatGraph::from(sec));
    book.add_by_filename("MEDIA/GRAPHS/STATS/HEALTH_PLAYER_ALCHEMIST.DAT.ADM",
                         tl::core::StatGraph::from(sec));

    TL_CHECK_EQ(book.size(), 1u);

    // Lookups in upper, lower, mixed case
    const tl::core::StatGraph* g_upper = book.find("HEALTH_PLAYER_ALCHEMIST");
    const tl::core::StatGraph* g_lower = book.find("health_player_alchemist");
    const tl::core::StatGraph* g_mixed = book.find("Health_Player_Alchemist");

    TL_CHECK(g_upper != nullptr);
    TL_CHECK(g_lower != nullptr);
    TL_CHECK(g_mixed != nullptr);

    // Look up by filename
    const tl::core::StatGraph* g_file =
        book.find_by_filename("media/graphs/stats/health_player_alchemist.dat.adm");
    TL_CHECK(g_file != nullptr);

    // `getGraph` reads mGraphsByName only; a path is not a name.
    TL_CHECK(book.find("media/graphs/stats/health_player_alchemist.dat.adm") == nullptr);

    // Value lookup with fallback
    TL_CHECK(approx_eq(book.value("HEALTH_PLAYER_ALCHEMIST", 1.0f, 0.0f), 200.0f));
    TL_CHECK(approx_eq(book.value("NONEXISTENT", 1.0f, 999.0f), 999.0f));
}
