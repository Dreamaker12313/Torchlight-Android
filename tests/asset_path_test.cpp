// Case-insensitive lookup: Alchemist.MESH asks for "alchemist.skeleton", which ships as
// "Alchemist.SKELETON"; a case-sensitive lookup loads the mesh unanimated.
#include "core/asset_path.h"
#include "test_support.h"

#include <filesystem>
#include <fstream>

using tl::core::AssetIndex;

namespace {

/// A throwaway tree with the exact casing mismatch found in the real archive.
std::filesystem::path make_fixture() {
    const auto root = std::filesystem::temp_directory_path() / "tl_asset_index_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "models" / "Alchemist");

    for (const char* name : {"Alchemist.MESH", "Alchemist.SKELETON", "Attack1.SKELETON"}) {
        std::ofstream(root / "models" / "Alchemist" / name) << "x";
    }
    return root;
}

}  // namespace

TL_TEST(asset_index_normalises_case_and_separators) {
    // Runic's data files use Windows separators, e.g.
    //   <STRING>LAYOUT:media\particles\playerskills\gauntletenchant.layout
    TL_CHECK_EQ(AssetIndex::normalise("Models\\Alchemist\\A.MESH"),
                std::string("models/alchemist/a.mesh"));
}

TL_TEST(asset_index_resolves_regardless_of_case) {
    const auto root = make_fixture();
    AssetIndex idx;
    TL_CHECK(idx.build(root.string()));
    TL_CHECK_EQ(idx.size(), size_t{3});

    // The real-world failure: lowercase reference, uppercase file on disk.
    TL_CHECK_EQ(idx.resolve("alchemist.skeleton"),
                std::string("models/Alchemist/Alchemist.SKELETON"));

    TL_CHECK_EQ(idx.resolve("MODELS/ALCHEMIST/ALCHEMIST.MESH"),
                std::string("models/Alchemist/Alchemist.MESH"));

    // Backslash separators must resolve too.
    TL_CHECK_EQ(idx.resolve("models\\Alchemist\\Attack1.SKELETON"),
                std::string("models/Alchemist/Attack1.SKELETON"));

    std::filesystem::remove_all(root);
}

TL_TEST(asset_index_reports_missing_assets) {
    const auto root = make_fixture();
    AssetIndex idx;
    TL_CHECK(idx.build(root.string()));

    TL_CHECK(idx.resolve("nosuchfile.mesh").empty());
    TL_CHECK(!idx.contains("nosuchfile.mesh"));

    std::filesystem::remove_all(root);
}

TL_TEST(asset_index_rejects_a_missing_root) {
    AssetIndex idx;
    TL_CHECK(!idx.build("this/path/does/not/exist"));
}

TL_TEST(asset_index_prefers_a_sibling_when_told_where_to_look) {
    // Two directories with the same filename, which is the norm in the real
    // archive: nearly every unit has its own idle.skeleton.
    const auto root = std::filesystem::temp_directory_path() / "tl_asset_near_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "models" / "Alchemist");
    std::filesystem::create_directories(root / "models" / "Vanquisher");
    std::ofstream(root / "models" / "Alchemist" / "Idle.SKELETON") << "a";
    std::ofstream(root / "models" / "Vanquisher" / "idle.skeleton") << "v";
    std::ofstream(root / "models" / "Alchemist" / "Alchemist.MESH") << "m";

    AssetIndex idx;
    TL_CHECK(idx.build(root.string()));
    TL_CHECK_EQ(idx.ambiguous_basenames().size(), size_t{1});

    // Asked from the Alchemist's mesh, the Alchemist's skeleton must win.
    TL_CHECK_EQ(idx.resolve_near("idle.skeleton", "models/Alchemist/Alchemist.MESH"),
                std::string("models/Alchemist/Idle.SKELETON"));

    std::filesystem::remove_all(root);
}

TL_TEST(resolve_relative_collapses_dot_dot_against_the_referring_directory) {
    // The town's river, as media/levelSets/town1/water.material writes it.
    TL_CHECK_EQ(AssetIndex::resolve_relative("media/levelSets/town1/",
                                             "../../sharedtextures/watertown/WATER_Base.dds"),
                std::string("media/sharedtextures/watertown/WATER_Base.dds"));

    // Case is preserved: the result is handed to a case-insensitive lookup that
    // reports the archive's own spelling back, and lowercasing here would throw
    // away information for no gain.
    TL_CHECK_EQ(AssetIndex::resolve_relative("A/B/", "../C.DDS"), std::string("A/C.DDS"));
}

TL_TEST(resolve_relative_handles_both_separators_and_dot) {
    TL_CHECK_EQ(AssetIndex::resolve_relative("media\\levelSets\\town1", "..\\..\\shared/x.dds"),
                std::string("media/shared/x.dds"));
    TL_CHECK_EQ(AssetIndex::resolve_relative("a/b", "./c/d.png"), std::string("a/b/c/d.png"));
}

TL_TEST(resolve_relative_says_nothing_to_do_for_a_bare_basename) {
    // Empty means "this is not a path", so the caller falls through to its
    // ordinary basename lookup rather than treating it as a failed resolve.
    TL_CHECK(AssetIndex::resolve_relative("media/models/Alchemist/", "body.png").empty());
}

TL_TEST(resolve_relative_does_not_climb_above_the_root) {
    // Nowhere left to pop. The answer is the remainder rather than a path with
    // stray "..' segments in it, which no lookup could answer.
    TL_CHECK_EQ(AssetIndex::resolve_relative("a", "../../b.dds"), std::string("b.dds"));
}

// The music is read off the filesystem and is the worst-cased data: `../music/town.ogg`
// ships as `Town.ogg`, `../MUSIC/TOWNFIGHT.OGG` as `TownFight.ogg`.
TL_TEST(asset_index_resolves_the_music_the_town_asks_for) {
    const auto root = std::filesystem::temp_directory_path() / "tl_asset_music_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "music");
    for (const char* name : {"Town.ogg", "TownFight.ogg", "Title.ogg"}) {
        std::ofstream(root / "music" / name) << "x";
    }

    AssetIndex idx;
    TL_CHECK(idx.build(root.string()));

    // Both spellings the town uses, with the leading `../` already stripped.
    TL_CHECK_EQ(idx.resolve("music/town.ogg"), std::string("music/Town.ogg"));
    TL_CHECK_EQ(idx.resolve("MUSIC/TOWNFIGHT.OGG"), std::string("music/TownFight.ogg"));

    std::filesystem::remove_all(root);
}
