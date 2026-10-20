// The game's colour runs in a display string (`|cAARRGGBB ... |u`): what
// core/text.h's markup_runs() splits and strip_markup() leaves.
#include "core/text.h"
#include "test_support.h"

#include <string>
#include <vector>

TL_TEST(markup_runs_split_a_display_string_by_colour) {
    const std::string text = "bring me a sample of |cFFFFBA00Gleaming Ember|u, I will reward you";
    const std::vector<tl::core::MarkupRun> runs = tl::core::markup_runs(text);
    TL_CHECK_EQ(runs.size(), size_t{3});

    TL_CHECK(!runs[0].coloured);
    TL_CHECK_EQ(runs[0].text, std::string("bring me a sample of "));

    TL_CHECK(runs[1].coloured);
    TL_CHECK_EQ(runs[1].text, std::string("Gleaming Ember"));
    // `|c` writes alpha first: FF BA00 is opaque orange, not a translucent white.
    TL_CHECK_EQ(runs[1].a, 1.0f);
    TL_CHECK_EQ(runs[1].r, 1.0f);
    TL_CHECK(runs[1].g > 0.72f && runs[1].g < 0.74f);
    TL_CHECK_EQ(runs[1].b, 0.0f);

    TL_CHECK(!runs[2].coloured);

    // The runs concatenate to exactly what strip_markup() gives.
    std::string joined;
    for (const tl::core::MarkupRun& run : runs) joined += run.text;
    TL_CHECK_EQ(joined, tl::core::strip_markup(text));
}

TL_TEST(an_unclosed_colour_run_reaches_the_end_of_the_line) {
    const std::vector<tl::core::MarkupRun> runs =
        tl::core::markup_runs("look for the Ember on the |cFFFFBA00first floor");
    TL_CHECK_EQ(runs.size(), size_t{2});
    TL_CHECK(runs[1].coloured);
    TL_CHECK_EQ(runs[1].text, std::string("first floor"));
}

TL_TEST(a_bare_pipe_is_ordinary_text) {
    const std::vector<tl::core::MarkupRun> runs = tl::core::markup_runs("a|b|cZZ");
    TL_CHECK_EQ(runs.size(), size_t{1});
    TL_CHECK_EQ(runs[0].text, std::string("a|b|cZZ"));
    TL_CHECK_EQ(runs[0].text, tl::core::strip_markup("a|b|cZZ"));
}
