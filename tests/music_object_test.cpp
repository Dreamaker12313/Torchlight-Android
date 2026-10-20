// A layout's `Music` node (core/music_object.h): `CMusicObject` against a
// stand-in sound manager.

#include "core/music_object.h"
#include "test_support.h"

#include <string>
#include <vector>

namespace {

using tl::core::MusicHost;
using tl::core::MusicObject;

struct Manager {
    std::string playing;
    std::vector<std::string> played;
    std::vector<uint32_t> outputs;
    std::string level = "TOWN.OGG";
    float length = 10.0f;

    MusicHost host() {
        MusicHost h;
        h.playing = [this] { return playing; };
        h.play = [this](const std::string& file, bool) {
            played.push_back(file);
            playing = tl::core::music_track_key(file);
            return length;
        };
        h.stop = [this] { playing.clear(); };
        h.level_music = [this] { return level; };
        h.output = [this](uint32_t o) { outputs.push_back(o); };
        return h;
    }
};

}  // namespace

// `setMusicFile` and `playMusic`'s name.
TL_TEST(music_names_follow_set_music_file_and_play_music) {
    TL_CHECK_EQ(tl::core::music_object_file("music/fight.ogg"), std::string("../MUSIC/FIGHT.OGG"));
    TL_CHECK_EQ(tl::core::music_object_file("../music/town.ogg"), std::string("../MUSIC/TOWN.OGG"));
    TL_CHECK_EQ(tl::core::music_object_file("c:x.ogg"), std::string("C:X.OGG"));
    TL_CHECK_EQ(tl::core::music_object_file("ab"), std::string("AB"));
    TL_CHECK_EQ(tl::core::music_track_key("../MUSIC/TOWNFIGHT.OGG"), std::string("TOWNFIGHT.OGG"));
    TL_CHECK_EQ(tl::core::music_track_key("../media/a.ogg"), std::string("MEDIA/A.OGG"));
}

// `play` starts the track and raises output 11; a track that
// does not loop gives the level's music back when its length has run out.
TL_TEST(a_music_node_that_does_not_loop_hands_the_level_music_back) {
    Manager m;
    m.playing = "TOWN.OGG";
    MusicObject music;
    music.set_file("../MUSIC/TOWNFIGHT.OGG");
    music.loops = false;
    const MusicHost host = m.host();
    tl::core::music_play(music, host);
    TL_CHECK_EQ(m.played.size(), size_t{1});
    TL_CHECK_EQ(m.playing, std::string("TOWNFIGHT.OGG"));
    TL_CHECK_EQ(m.outputs.front(), tl::core::kMusicStarted);
    TL_CHECK(music.playing);
    // A second `Play` while it plays changes nothing.
    tl::core::music_play(music, host);
    TL_CHECK_EQ(m.played.size(), size_t{1});
    tl::core::music_update(music, host, 6.0f);
    TL_CHECK_EQ(m.playing, std::string("TOWNFIGHT.OGG"));
    tl::core::music_update(music, host, 6.0f);
    TL_CHECK_EQ(m.playing, std::string("TOWN.OGG"));
    TL_CHECK(!music.playing);
}

// `stop` raises output 12; a looping node whose track has gone
// lets go without putting the level's music back.
TL_TEST(a_looping_music_node_lets_go_when_its_track_is_gone) {
    Manager m;
    MusicObject music;
    music.set_file("../MUSIC/BOSS.OGG");
    const MusicHost host = m.host();
    tl::core::music_play(music, host);
    m.playing = "OTHER.OGG";
    tl::core::music_update(music, host, 0.25f);
    TL_CHECK(!music.playing);
    TL_CHECK_EQ(m.playing, std::string("OTHER.OGG"));
    tl::core::music_stop(music, host);
    TL_CHECK_EQ(m.outputs.back(), tl::core::kMusicStopped);
    TL_CHECK(m.playing.empty());
}
