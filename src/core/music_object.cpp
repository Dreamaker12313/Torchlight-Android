#include "core/music_object.h"

#include <cctype>

namespace tl::core {

namespace {

// PORT: ASCII case folding, the case-blind compare of names in the port's narrow strings.
std::string upper(std::string text) {
    for (char& c : text) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return text;
}

}  // namespace

// `CMusicObject::setMusicFile`.
std::string music_object_file(const std::string& name) {
    std::string file = upper(name);
    if (file.size() > 2 && file[0] != '.' && file[1] != '.' && file[1] != ':') {
        file = "../" + file;
    }
    return file;
}

// `CSoundManager::playMusic`, the name it
// plays: `StringUpper`, `find(L"MUSIC")` and the rest past it, then the
// leading `.` and `/` off one at a time.
std::string music_track_key(const std::string& name) {
    std::string key = upper(name);
    const size_t at = key.find("MUSIC");
    if (at != std::string::npos) key = key.substr(at + 5);
    size_t lead = 0;
    while (lead < key.size() && (key[lead] == '.' || key[lead] == '/')) ++lead;
    return key.substr(lead);
}

void MusicObject::set_file(const std::string& name) {
    file = music_object_file(name);
    // The console assigns the new file to X360 +0x78 as well.
    started = file;
}

// `CMusicObject::getIsPlaying`: the manager's track is the
// one this node last started.
bool music_is_playing(const MusicObject& music, const MusicHost& host) {
    return host.playing && host.playing() == music.started;
}

// `CMusicObject::play`.
void music_play(MusicObject& music, const MusicHost& host) {
    if (music_is_playing(music, host)) return;
    music.remaining = host.play ? host.play(music.file, music.loops) : 0.0f;
    // `playMusic` takes the name by reference and leaves it as it played it.
    music.file = music_track_key(music.file);
    music.started = host.playing ? host.playing() : std::string();
    music.playing = true;
    if (host.output) host.output(kMusicStarted);
}

// `CMusicObject::stop`.
void music_stop(MusicObject& music, const MusicHost& host) {
    music.playing = false;
    if (host.stop) host.stop();
    if (host.output) host.output(kMusicStopped);
}

// `CMusicObject::playNormalLevelMusic`: the
// level template's `MUSIC`, looping, unless it is already what plays.
void music_play_level(MusicObject& music, const MusicHost& host) {
    const std::string level = host.level_music ? host.level_music() : std::string();
    const std::string playing = host.playing ? host.playing() : std::string();
    if (level == playing) return;
    if (host.play) host.play(level, true);
    music.playing = false;
}

// `CMusicObject::updateMusic(float)`.
void music_update(MusicObject& music, const MusicHost& host, float delta) {
    if (!music.playing) return;
    // Every 0.2 s: a track no longer on the channel ends the node's turn, and
    // one that did not loop gives the level's music back.
    music.poll -= delta;
    if (music.poll <= 0.0f) {
        music.poll = 0.2f;
        if (!music_is_playing(music, host)) {
            music.playing = false;
            if (music.loops) return;
            music_play_level(music, host);
        }
    }
    // A track that does not loop is also timed out by its own length.
    if (!music.loops) {
        music.remaining -= delta;
        if (music.remaining <= 0.0f) {
            music_play_level(music, host);
            music.playing = false;
        }
    }
}

}  // namespace tl::core
