// The text-crawl cinematics (`core/cinematics.h`): the lookup, the crawl's
// length and position, the music's share, and `performWarp`'s rule for which
// arrival plays one.
#include "core/cinematics.h"
#include "formats/adm.h"
#include "test_support.h"

#include <string>

namespace {

using tl::core::Cinematic;
using tl::core::CinematicCrawl;
using tl::core::Cinematics;

tl::formats::AdmSection group(const std::string& name, const std::string& text) {
    tl::formats::AdmSection section;
    section.name = "CINEMATIC";
    const auto add = [&section](const char* key, const std::string& value) {
        tl::formats::AdmRecord record;
        record.key = key;
        record.type = tl::formats::AdmType::String;
        record.text = value;
        section.records.push_back(record);
    };
    add("NAME", name);
    add("TEXT", text);
    add("IMAGE", "media/ui/vignettebackdrop.imageset");
    add("SOUND", "Vignette1");
    return section;
}

}  // namespace

// `CCinematics::reload`: every file is listed,
// and a name is indexed to the last file of it.
TL_TEST(cinematics_lookup_is_exact_and_last_file_wins) {
    Cinematics book;
    book.add(tl::core::load_cinematic(group("VIGNETTE1", "first")));
    book.add(tl::core::load_cinematic(group("VIGNETTE1", "second")));
    book.add(tl::core::load_cinematic(group("DestroyerIntro", "third")));
    TL_CHECK_EQ(book.size(), 3u);
    const Cinematic* found = book.get("VIGNETTE1");
    TL_CHECK(found != nullptr);
    TL_CHECK_EQ(found->text, std::string("second"));
    TL_CHECK_EQ(found->sound, std::string("Vignette1"));
    TL_CHECK(book.get("vignette1") == nullptr);
    TL_CHECK(book.get("DestroyerIntro") != nullptr);
}

TL_TEST(cinematics_text_turns_backslash_n_into_newlines) {
    TL_CHECK_EQ(tl::core::cinematic_display_text("a\\n\\nb"), std::string("a\n\nb"));
    TL_CHECK_EQ(tl::core::cinematic_display_text("plain"), std::string("plain"));
}

TL_TEST(cinematics_crawl_lasts_ten_seconds_at_least) {
    TL_CHECK_EQ(tl::core::cinematic_crawl_seconds(std::string(600, 'x')), 10.0f);
    TL_CHECK_EQ(tl::core::cinematic_crawl_seconds(std::string(1000, 'x')), 1000.0f * 0.015f);
}

TL_TEST(cinematics_crawl_rises_from_below_the_frame) {
    CinematicCrawl crawl;
    crawl.duration = 10.0f;
    // Nothing crawled yet: the window's height plus 50 below its place.
    TL_CHECK_EQ(crawl.step(0.0f, 200.0f), 250.0f);
    TL_CHECK_EQ(crawl.step(5.0f, 200.0f), 125.0f);
    TL_CHECK(!crawl.finished());
    // The step that passes the end overshoots once, then the text rests.
    TL_CHECK(crawl.step(6.0f, 200.0f) < 0.0f);
    TL_CHECK(crawl.finished());
    TL_CHECK_EQ(crawl.step(1.0f, 200.0f), 0.0f);
}

TL_TEST(cinematics_held_button_runs_tenfold_and_a_press_finishes) {
    TL_CHECK_EQ(tl::core::cinematic_crawl_delta(0.5f, false, false, 12.0f), 0.5f);
    TL_CHECK_EQ(tl::core::cinematic_crawl_delta(0.5f, true, false, 12.0f), 5.0f);
    TL_CHECK_EQ(tl::core::cinematic_crawl_delta(0.5f, false, true, 12.0f), 12.0f);
    TL_CHECK_EQ(tl::core::cinematic_crawl_delta(0.5f, true, true, 12.0f), 5.0f);
}

TL_TEST(cinematics_music_sinks_to_a_quarter_and_returns) {
    TL_CHECK_EQ(tl::core::cinematic_music_level(1.0f, 0.5f, true), 0.5f);
    TL_CHECK_EQ(tl::core::cinematic_music_level(0.5f, 0.5f, true), 0.25f);
    TL_CHECK_EQ(tl::core::cinematic_music_level(0.25f, 0.5f, true), 0.25f);
    TL_CHECK_EQ(tl::core::cinematic_music_level(0.25f, 0.5f, false), 0.75f);
    TL_CHECK_EQ(tl::core::cinematic_music_level(0.75f, 0.5f, false), 1.0f);
    TL_CHECK_EQ(tl::core::cinematic_music_level(1.0f, 0.5f, false), 1.0f);
}

TL_TEST(cinematics_warp_plays_on_the_deepest_fresh_arrival_only) {
    // Town to the first floor: the tracker was just made at this depth.
    TL_CHECK(tl::core::warp_plays_cinematic(0, 0, "VIGNETTE1", false));
    // Back up from floor 2: the dungeon has been seen deeper.
    TL_CHECK(!tl::core::warp_plays_cinematic(1, 0, "VIGNETTE1", false));
    // The same floor again, put back from its remembered state.
    TL_CHECK(!tl::core::warp_plays_cinematic(0, 0, "VIGNETTE1", true));
    // A floor whose stratum names none.
    TL_CHECK(!tl::core::warp_plays_cinematic(0, 1, "", false));
    // A dungeon with no tracker at all reads -1.
    TL_CHECK(tl::core::warp_plays_cinematic(-1, 4, "VIGNETTE2", false));
}
