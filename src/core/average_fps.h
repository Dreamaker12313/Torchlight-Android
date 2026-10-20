// `KSETTINGS_F_AVERAGE_FPS`, the frame rate `CGame::update`
// keeps over fewer than 60 frames (`CGame::CGame`). A
// body's deletion asks it (`CCharacter::update`: 30 or under).
//
// No OGRE and no file I/O.
#pragma once

#include <cstddef>
#include <deque>

namespace tl::core {

class AverageFps {
public:
    /// One frame of `delta` seconds; a frame of no time is not counted.
    // PORT: the debug FPS overlay's average.
    void add(float delta) {
        if (!(0.0f < delta)) return;
        sum_ = delta + sum_;
        frames_.push_back(delta);
        if (kWindow <= frames_.size()) {
            sum_ = sum_ - frames_.front();
            frames_.pop_front();
        }
        value_ = 1.0f / (sum_ / static_cast<float>(frames_.size()));
    }
    /// The setting as the last frame wrote it: 0 before the first frame
    /// (`CSettings`' default).
    float value() const { return value_; }

private:
    static constexpr std::size_t kWindow = 60;
    std::deque<float> frames_;
    float sum_ = 0.0f;
    float value_ = 0.0f;
};

}  // namespace tl::core
