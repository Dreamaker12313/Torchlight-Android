// A layout's `Music` node: `CMusicObject`, the music it starts and how it hands
// the level's own music back. A track that does not loop gives the level's
// music back by itself when it runs out. No audio and no files here.
#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace tl::core {

/// `CMusicObject::setMusicFile`: the name
/// upper-cased, and `../` put in front of one longer than two characters that
/// starts with neither `.` nor a drive (`?.` or `?:`) - a `FILE NAME` is
/// relative to `media/`.
std::string music_object_file(const std::string& name);

/// The name `CSoundManager::playMusic` plays
/// and then reports as the one playing (`getMusicFilePlaying`): upper-cased,
/// what follows the first `MUSIC` when there is one, and every leading `.` and
/// `/` taken off.
std::string music_track_key(const std::string& name);

/// What a `CMusicObject` asks of the sound manager and of its level.
struct MusicHost {
    /// `CSoundManager::getMusicFilePlaying`, as `music_track_key` spells it;
    /// empty when nothing plays.
    std::function<std::string()> playing;
    /// `CSoundManager::playMusic(file, loops)`: the track's length in
    /// seconds (`getSoundLength`), 0 when it could not be played.
    std::function<float(const std::string& file, bool loops)> play;
    /// `CSoundManager::stopMusic`.
    std::function<void()> stop;
    /// The level template's `MUSIC`, empty for a level with none.
    std::function<std::string()> level_music;
    /// An output of the node (`CEditorBaseObject` vtable +0x30).
    std::function<void(uint32_t output)> output;
};

/// `CMusicObject`: what one `Music` node keeps, at the values
/// `CMusicObject::CMusicObject` gives them: no file, looping,
/// nothing left to play, not playing, the channel looked at after 0.2 s.
struct MusicObject {
    std::string file;     ///< `music_object_file(FILE NAME)`
    std::string started;  ///< the track it last started, as playing reports it
    bool loops = true;
    float remaining = 0.0f;  ///< a non-looping track's time left
    bool playing = false;    ///< started and not yet handed back
    float poll = 0.2f;       ///< until the next look at the channel

    /// `setMusicFile`: the file, and that file as the one last started.
    void set_file(const std::string& name);
};

/// The node's outputs.
inline constexpr uint32_t kMusicStarted = 0x0b;  ///< `play`
inline constexpr uint32_t kMusicStopped = 0x0c;  ///< `stop`

/// `CMusicObject::play`.
void music_play(MusicObject& music, const MusicHost& host);
/// `CMusicObject::stop`.
void music_stop(MusicObject& music, const MusicHost& host);
/// `CMusicObject::playNormalLevelMusic`.
void music_play_level(MusicObject& music, const MusicHost& host);
/// `CMusicObject::getIsPlaying`.
bool music_is_playing(const MusicObject& music, const MusicHost& host);
/// `CMusicObject::updateMusic(float)`, once a frame from
/// `CMusicObjectDescriptor::update`.
void music_update(MusicObject& music, const MusicHost& host, float delta);

}  // namespace tl::core
