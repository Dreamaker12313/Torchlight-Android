// The console's Stats column, `CStatsXenon` (X360): the handler's A, Y and stick steps and
// the refresh's colour rule.
#include "core/stats_page.h"
#include "test_support.h"

TL_TEST(stats_page_step_clamps_and_reports_a_move) {
    // max(0, min(3, selected + step)); NAVIGATE only on a move.
    tl::core::StatsPage page;
    TL_CHECK(!tl::core::stats_page_step(page, -1));
    TL_CHECK_EQ(page.selected, 0);
    TL_CHECK(page.dirty);  // runs either way
    TL_CHECK(tl::core::stats_page_step(page, 1));
    TL_CHECK(tl::core::stats_page_step(page, 1));
    TL_CHECK(tl::core::stats_page_step(page, 1));
    TL_CHECK_EQ(page.selected, 3);
    TL_CHECK(!tl::core::stats_page_step(page, 1));
    TL_CHECK_EQ(page.selected, 3);
}

TL_TEST(stats_page_add_and_remove_points) {
    // The handler's case 5 and with -1.
    tl::core::StatsPage page;
    tl::core::open_stats_page(page, 2);
    TL_CHECK(page.open);
    TL_CHECK_EQ(page.available, 2);
    page.selected = 1;
    TL_CHECK(tl::core::stats_page_add_point(page, 2));
    TL_CHECK(tl::core::stats_page_add_point(page, 2));
    TL_CHECK_EQ(page.pending[1], 2);
    TL_CHECK_EQ(page.available, 0);

    // None left: no point moves, but the page still refreshes.
    page.dirty = false;
    TL_CHECK(!tl::core::stats_page_add_point(page, 2));
    TL_CHECK(page.dirty);

    // Nothing pending on the selection: Y does nothing, not even a refresh.
    page.selected = 0;
    page.dirty = false;
    TL_CHECK(!tl::core::stats_page_remove_point(page, 2));
    TL_CHECK(!page.dirty);

    page.selected = 1;
    TL_CHECK(tl::core::stats_page_remove_point(page, 2));
    TL_CHECK_EQ(page.pending[1], 1);
    TL_CHECK_EQ(page.available, 1);
    TL_CHECK(page.dirty);

    // The character has no unspent points: neither button does anything.
    page.dirty = false;
    TL_CHECK(!tl::core::stats_page_add_point(page, 0));
    TL_CHECK(!tl::core::stats_page_remove_point(page, 0));
    TL_CHECK(!page.dirty);
}

TL_TEST(stats_page_figure_colours) {
    // "FFc0c0ff" above, "FFFF0000" below, "FFFFFFFF" equal.
    TL_CHECK_EQ(tl::core::stats_figure_colour(3), 0xFFC0C0FFu);
    TL_CHECK_EQ(tl::core::stats_figure_colour(-1), 0xFFFF0000u);
    TL_CHECK_EQ(tl::core::stats_figure_colour(0), 0xFFFFFFFFu);
}
