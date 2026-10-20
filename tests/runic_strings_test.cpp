// Values verified against the console build, pinned here so a change to the port's
// transcriptions cannot slip past the desktop tests unseen.
#include "core/runic_strings.h"
#include "core/logic_names.h"
#include "core/msvc_sort.h"
#include "core/stats.h"
#include "test_support.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace {

float float_of(uint32_t bits) {
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}

}  // namespace

TL_TEST(runic_strings_value_as_string_matches_the_console) {
    TL_CHECK_EQ(tl::core::value_as_string(int32_t{-2147483647 - 1}), std::string("-2147483648"));
    TL_CHECK_EQ(tl::core::value_as_string(uint32_t{4294967295u}), std::string("4294967295"));
    TL_CHECK_EQ(tl::core::value_as_string(int64_t{-9223372036854775807LL - 1}),
                std::string("-9223372036854775808"));
    // The console's C runtime: three exponent digits, ties away from zero, its own NaN names.
    TL_CHECK_EQ(tl::core::value_as_string(1000000.0f), std::string("1e+006"));
    TL_CHECK_EQ(tl::core::value_as_string(9.99999975e-06f), std::string("1e-005"));
    TL_CHECK_EQ(tl::core::value_as_string(1.4e-45f), std::string("1.4013e-045"));
    TL_CHECK_EQ(tl::core::value_as_string(581.3125f), std::string("581.313"));
    TL_CHECK_EQ(tl::core::value_as_string(135030.5f), std::string("135031"));
    TL_CHECK_EQ(tl::core::value_as_string(-886696.5f), std::string("-886697"));
    TL_CHECK_EQ(tl::core::value_as_string(7767945.0f), std::string("7.76795e+006"));
    TL_CHECK_EQ(tl::core::value_as_string(0.5f), std::string("0.5"));
    TL_CHECK_EQ(tl::core::value_as_string(-0.0f), std::string("-0"));
    TL_CHECK_EQ(tl::core::value_as_string(std::numeric_limits<float>::infinity()),
                std::string("1.#INF"));
    TL_CHECK_EQ(tl::core::value_as_string(-std::numeric_limits<float>::infinity()),
                std::string("-1.#INF"));
    TL_CHECK_EQ(tl::core::value_as_string(float_of(0x7fc00000u)), std::string("1.#QNAN"));
    TL_CHECK_EQ(tl::core::value_as_string(float_of(0xffc00000u)), std::string("-1.#IND"));
    TL_CHECK_EQ(tl::core::value_as_string(float_of(0xffc00001u)), std::string("-1.#QNAN"));
}

TL_TEST(runic_strings_replace_searches_from_the_start_each_time) {
    // The console searches again from the start after each replacement.
    TL_CHECK_EQ(tl::core::replace_wstring("aabb", "ab", ""), std::string(""));
    TL_CHECK_EQ(tl::core::replace_wstring("x [VALUE] y [VALUE]", "[VALUE]", "5"),
                std::string("x 5 y 5"));
}

TL_TEST(logic_names_match_the_console) {
    TL_CHECK_EQ(tl::core::loop_type_id_by_string("Cycle"), 1);
    TL_CHECK_EQ(tl::core::loop_type_id_by_string("cycle"), 0);
    TL_CHECK_EQ(tl::core::game_state_id_by_string("INTERACTIVE UI(BARS AT TOP AND BOTTOM)"), 3);
    TL_CHECK_EQ(tl::core::difficulty_id_by_string(""), 0xeu);
    TL_CHECK_EQ(tl::core::difficulty_id_by_string("VERY HARD"), 3u);
    TL_CHECK_EQ(tl::core::random_group_type_from_string("Random Chance"), 2u);
}

TL_TEST(msvc_sort_keeps_the_consoles_order_of_equal_keys) {
    // Up to 32 elements it is an insertion sort: equal keys keep their order.
    std::vector<std::pair<int, int>> v{{2, 0}, {1, 1}, {2, 2}, {1, 3}};
    tl::core::msvc_sort(v.begin(), v.end(),
                        [](const auto& a, const auto& b) { return a.first < b.first; });
    TL_CHECK(v[0].second == 1 && v[1].second == 3 && v[2].second == 0 && v[3].second == 2);
}

TL_TEST(stat_graph_slope_past_the_end_is_fused) {
    // CGraph::getValue fuses the slope (fmadds): fma(3, x - 1, 1e8) for x = 1 + 1/3.
    tl::core::StatGraph graph(tl::core::GraphType::Linear);
    graph.add_point(1.0f, 1e8f);
    graph.process_points();
    graph.set_infer_past_end(true);
    graph.set_slope(0, 3.0f);
    const float x = 1.0f + 1.0f / 3.0f;
    TL_CHECK(graph.at(x) == std::fma(3.0f, x - 1.0f, 1e8f));
}
