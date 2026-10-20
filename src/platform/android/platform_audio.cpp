// Android audio device: AAudio. A 16-bit stereo stream at the device's own rate, fed by the
// mixer thread with blocking writes, so the device's pace is the mixer's clock; about four blocks
// (85 ms) queued, as on the other targets.
//
// AAudio aborts if a stream is closed while another thread is inside AAudioStream_write, so
// close() stops the stream (waking the writer), waits until it has left write(), then closes.
// A disconnected stream (output changed) is reopened at the same rate by write().
// `audio_silent` in the data root runs the stream exactly as usual but feeds it zeros.
#include "core/audio.h"

#include "core/log.h"
#include "core/platform.h"

#include <aaudio/AAudio.h>
#include <sys/resource.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

namespace tl::core::audio_device {

namespace {

constexpr size_t kBlockFrames = 1024;
constexpr int32_t kBlocksQueued = 4;
constexpr int64_t kWriteTimeoutNs = 1000000000;  // a second: only a stalled device waits that long

struct Device {
    AAudioStream* stream = nullptr;
    int32_t rate = 0;
    bool silent = false;
    std::vector<int16_t> zeros;
    /// For the periodic line: frames written since it, and when it was.
    int64_t frames = 0;
    std::chrono::steady_clock::time_point since;
    /// close() has begun; write() returns at once.
    std::atomic<bool> closing{false};
    /// Threads inside write(): close() waits for none before closing.
    std::atomic<int> writers{0};
};

Device g_device;

AAudioStream* open_stream(int32_t rate) {
    AAudioStreamBuilder* builder = nullptr;
    if (AAudio_createStreamBuilder(&builder) != AAUDIO_OK) return nullptr;
    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setChannelCount(builder, 2);
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setUsage(builder, AAUDIO_USAGE_GAME);
    AAudioStreamBuilder_setContentType(builder, AAUDIO_CONTENT_TYPE_SONIFICATION);
    // 0 lets the device choose; a reopen asks for the rate the mixer runs at.
    AAudioStreamBuilder_setSampleRate(builder, rate);
    AAudioStream* stream = nullptr;
    const aaudio_result_t opened = AAudioStreamBuilder_openStream(builder, &stream);
    AAudioStreamBuilder_delete(builder);
    if (opened != AAUDIO_OK) {
        TL_LOG_WARN("aaudio: no output stream (%s)", AAudio_convertResultToText(opened));
        return nullptr;
    }
    const int32_t capacity = AAudioStream_getBufferCapacityInFrames(stream);
    const int32_t wanted = static_cast<int32_t>(kBlockFrames) * kBlocksQueued;
    AAudioStream_setBufferSizeInFrames(stream, wanted < capacity ? wanted : capacity);
    const aaudio_result_t started = AAudioStream_requestStart(stream);
    if (started != AAUDIO_OK) {
        TL_LOG_WARN("aaudio: the stream would not start (%s)", AAudio_convertResultToText(started));
        AAudioStream_close(stream);
        return nullptr;
    }
    return stream;
}

void log_stream(const char* what) {
    AAudioStream* s = g_device.stream;
    TL_LOG_INFO("aaudio: %s: %d Hz, %d channel(s), buffer %d of %d frames, burst %d, %s%s", what,
                AAudioStream_getSampleRate(s), AAudioStream_getChannelCount(s),
                AAudioStream_getBufferSizeInFrames(s), AAudioStream_getBufferCapacityInFrames(s),
                AAudioStream_getFramesPerBurst(s),
                AAudioStream_getPerformanceMode(s) == AAUDIO_PERFORMANCE_MODE_LOW_LATENCY
                    ? "low latency"
                    : "normal latency",
                g_device.silent ? ", silent (audio_silent): zeros are written" : "");
}

}  // namespace

int open() {
    Device& d = g_device;
    if (d.stream != nullptr) return d.rate;
    d.closing.store(false);
    std::FILE* silent = std::fopen((platform_data_root() + "audio_silent").c_str(), "rb");
    d.silent = silent != nullptr;
    if (silent != nullptr) std::fclose(silent);
    d.stream = open_stream(0);
    if (d.stream == nullptr) return 0;
    d.rate = AAudioStream_getSampleRate(d.stream);
    d.frames = 0;
    d.since = std::chrono::steady_clock::now();
    if (AAudioStream_getChannelCount(d.stream) != 2) {
        TL_LOG_WARN("aaudio: the stream has %d channels; the mixer produces 2",
                    AAudioStream_getChannelCount(d.stream));
    }
    log_stream("open");
    return d.rate;
}

void close() {
    Device& d = g_device;
    if (d.stream == nullptr) return;
    // Stopping wakes a write() blocked on the stream, as waveOutReset and
    // audoutStopAudioOut do on the other targets; the stream is closed only
    // once the writer has left.
    d.closing.store(true);
    AAudioStream_requestStop(d.stream);
    while (d.writers.load() > 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    TL_LOG_INFO("aaudio: closed after %d underrun(s)", AAudioStream_getXRunCount(d.stream));
    AAudioStream_close(d.stream);
    d.stream = nullptr;
}

size_t block_frames() { return kBlockFrames; }

namespace {

/// write()'s body, with the writer counted in (below).
bool write_counted(Device& d, const int16_t* interleaved, size_t frames) {
    if (d.closing.load() || d.stream == nullptr) return false;
    if (d.silent) {
        d.zeros.assign(frames * 2, 0);
        interleaved = d.zeros.data();
    }
    size_t done = 0;
    bool reopened = false;
    while (done < frames) {
        if (d.closing.load()) return false;
        const aaudio_result_t wrote = AAudioStream_write(
            d.stream, interleaved + done * 2, static_cast<int32_t>(frames - done), kWriteTimeoutNs);
        if (wrote == AAUDIO_ERROR_DISCONNECTED && !reopened) {
            // The output changed under the stream: a new one at the mixer's rate.
            reopened = true;
            AAudioStream_close(d.stream);
            d.stream = open_stream(d.rate);
            if (d.stream == nullptr) return false;
            log_stream("reopened after the output changed");
            continue;
        }
        if (wrote < 0) {
            TL_LOG_WARN("aaudio: write failed (%s); no more sound",
                        AAudio_convertResultToText(wrote));
            return false;
        }
        done += static_cast<size_t>(wrote);
    }

    // Every ten seconds, the device's own account of how the mixer is keeping up.
    d.frames += static_cast<int64_t>(frames);
    const auto now = std::chrono::steady_clock::now();
    const double seconds = std::chrono::duration<double>(now - d.since).count();
    if (seconds >= 10.0) {
        TL_LOG_INFO("aaudio: %.0f frames/s over %.1f s, %d underrun(s) so far",
                    static_cast<double>(d.frames) / seconds, seconds,
                    AAudioStream_getXRunCount(d.stream));
        d.frames = 0;
        d.since = now;
    }
    return true;
}

}  // namespace

bool write(const int16_t* interleaved, size_t frames) {
    Device& d = g_device;
    d.writers.fetch_add(1);
    const bool ok = write_counted(d, interleaved, frames);
    d.writers.fetch_sub(1);
    return ok;
}

void prepare_mixer_thread() {
    // Android's audio priority (THREAD_PRIORITY_AUDIO, nice -16), which an app
    // may give its own threads: the mixer must not wait behind a level load.
    if (setpriority(PRIO_PROCESS, static_cast<id_t>(gettid()), -16) != 0) {
        TL_LOG_INFO("audio: the mixer keeps the default priority");
    }
}

}  // namespace tl::core::audio_device
