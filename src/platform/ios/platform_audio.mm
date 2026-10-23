#include "core/audio.h"

#include "core/log.h"

#import <AVFoundation/AVFoundation.h>
#import <AudioToolbox/AudioToolbox.h>

#include <algorithm>
#include <condition_variable>
#include <cstring>
#include <mutex>

namespace tl::core::audio_device {
namespace {

constexpr int kRate = 48000;
constexpr size_t kBlockFrames = 1024;
constexpr size_t kQueuedBuffers = 3;

struct Device {
    AudioQueueRef queue = nullptr;
    AudioQueueBufferRef buffers[kQueuedBuffers]{};
    bool free[kQueuedBuffers]{};
    std::mutex mutex;
    std::condition_variable available;
    bool active = false;
};

Device& device() {
    static Device d;
    return d;
}

const char* error_message(NSError* error) {
    const char* message = error.localizedDescription.UTF8String;
    return message != nullptr ? message : "unknown error";
}

void completed(void* context, AudioQueueRef, AudioQueueBufferRef buffer) {
    auto& d = *static_cast<Device*>(context);
    {
        std::lock_guard<std::mutex> guard(d.mutex);
        for (size_t i = 0; i < kQueuedBuffers; ++i) {
            if (d.buffers[i] == buffer) {
                d.free[i] = true;
                break;
            }
        }
    }
    d.available.notify_one();
}

void dispose(Device& d) {
    if (d.queue != nullptr) {
        AudioQueueStop(d.queue, true);
        AudioQueueDispose(d.queue, true);
        d.queue = nullptr;
    }
    for (auto& buffer : d.buffers) buffer = nullptr;
}

}  // namespace

int open() {
    Device& d = device();
    {
        std::lock_guard<std::mutex> guard(d.mutex);
        if (d.active) return kRate;
    }

    @autoreleasepool {
        AVAudioSession* session = [AVAudioSession sharedInstance];
        NSError* error = nil;
        if (![session setCategory:AVAudioSessionCategoryPlayback
                             mode:AVAudioSessionModeDefault
                          options:0
                            error:&error]) {
            TL_LOG_WARN("ios audio: could not set playback session category: %s",
                        error_message(error));
            return 0;
        }
        if (![session setActive:YES error:&error]) {
            TL_LOG_WARN("ios audio: could not activate playback session: %s",
                        error_message(error));
            return 0;
        }
    }

    AudioStreamBasicDescription format{};
    format.mSampleRate = kRate;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    format.mBytesPerPacket = sizeof(int16_t) * 2;
    format.mFramesPerPacket = 1;
    format.mBytesPerFrame = sizeof(int16_t) * 2;
    format.mChannelsPerFrame = 2;
    format.mBitsPerChannel = sizeof(int16_t) * 8;

    OSStatus status = AudioQueueNewOutput(&format, completed, &d, nullptr, nullptr, 0, &d.queue);
    if (status != noErr) {
        TL_LOG_WARN("ios audio: AudioQueueNewOutput failed (%d)", static_cast<int>(status));
        d.queue = nullptr;
        return 0;
    }

    const UInt32 buffer_bytes = static_cast<UInt32>(kBlockFrames * format.mBytesPerFrame);
    for (size_t i = 0; i < kQueuedBuffers; ++i) {
        status = AudioQueueAllocateBuffer(d.queue, buffer_bytes, &d.buffers[i]);
        if (status != noErr) {
            TL_LOG_WARN("ios audio: AudioQueueAllocateBuffer failed (%d)",
                        static_cast<int>(status));
            dispose(d);
            return 0;
        }
        std::memset(d.buffers[i]->mAudioData, 0, buffer_bytes);
        d.buffers[i]->mAudioDataByteSize = buffer_bytes;
        status = AudioQueueEnqueueBuffer(d.queue, d.buffers[i], 0, nullptr);
        if (status != noErr) {
            TL_LOG_WARN("ios audio: initial AudioQueueEnqueueBuffer failed (%d)",
                        static_cast<int>(status));
            dispose(d);
            return 0;
        }
        d.free[i] = false;
    }

    status = AudioQueueStart(d.queue, nullptr);
    if (status != noErr) {
        TL_LOG_WARN("ios audio: AudioQueueStart failed (%d)", static_cast<int>(status));
        dispose(d);
        return 0;
    }
    {
        std::lock_guard<std::mutex> guard(d.mutex);
        d.active = true;
    }
    TL_LOG_INFO("ios audio: %d Hz stereo playback started", kRate);
    return kRate;
}

void close() {
    Device& d = device();
    {
        std::lock_guard<std::mutex> guard(d.mutex);
        if (!d.active && d.queue == nullptr) return;
        d.active = false;
    }
    d.available.notify_all();
    dispose(d);
    @autoreleasepool {
        NSError* error = nil;
        if (![[AVAudioSession sharedInstance] setActive:NO error:&error]) {
            TL_LOG_INFO("ios audio: session deactivation returned: %s",
                        error_message(error));
        }
    }
}

bool write(const int16_t* samples, size_t frames) {
    Device& d = device();
    size_t done = 0;
    while (done < frames) {
        std::unique_lock<std::mutex> lock(d.mutex);
        d.available.wait(lock, [&d] {
            return !d.active || std::any_of(std::begin(d.free), std::end(d.free),
                                            [](bool is_free) { return is_free; });
        });
        if (!d.active) return false;

        size_t index = 0;
        while (index < kQueuedBuffers && !d.free[index]) ++index;
        if (index == kQueuedBuffers) continue;

        AudioQueueBufferRef buffer = d.buffers[index];
        const size_t chunk = std::min(kBlockFrames, frames - done);
        const size_t byte_count = chunk * sizeof(int16_t) * 2;
        std::memcpy(buffer->mAudioData, samples + done * 2, byte_count);
        buffer->mAudioDataByteSize = static_cast<UInt32>(byte_count);
        d.free[index] = false;
        const OSStatus status = AudioQueueEnqueueBuffer(d.queue, buffer, 0, nullptr);
        if (status != noErr) {
            d.free[index] = true;
            TL_LOG_WARN("ios audio: AudioQueueEnqueueBuffer failed (%d)",
                        static_cast<int>(status));
            return false;
        }
        done += chunk;
    }
    return true;
}

size_t block_frames() { return kBlockFrames; }
void prepare_mixer_thread() {}

}  // namespace tl::core::audio_device
