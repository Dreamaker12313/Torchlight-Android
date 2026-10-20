// Switch audio output: libnx's `audout`, a queue of PCM buffers at a fixed 48 kHz stereo Int16.
// Decode, resample and mix are shared in src/core/audio.cpp; `audren` would duplicate them.
#include "core/audio.h"

#include "core/log.h"

#include <switch.h>

#include <cstdlib>
#include <cstring>

namespace tl::core::audio_device {

namespace {

/// Frames per block, and how many blocks are in flight. `audout` wants each buffer aligned to
/// 0x1000 and sized in multiples of it: 1024 stereo Int16 frames is exactly one page. 21 ms a
/// block, 85 ms in flight, the same as the desktop.
constexpr size_t kBlockFrames = 1024;
constexpr size_t kBlocks = 4;
constexpr size_t kBlockBytes = kBlockFrames * 2 * sizeof(int16_t);

struct Device {
    AudioOutBuffer buffers[kBlocks]{};
    void* storage = nullptr;
    /// Which block write() will fill next, and whether it is still queued.
    size_t next = 0;
    size_t queued = 0;
    bool open = false;
};

Device& device() {
    static Device d;
    return d;
}

}  // namespace

int open() {
    Device& d = device();
    if (d.open) return static_cast<int>(audoutGetSampleRate());

    if (R_FAILED(audoutInitialize())) {
        TL_LOG_WARN("audoutInitialize failed");
        return 0;
    }
    if (R_FAILED(audoutStartAudioOut())) {
        TL_LOG_WARN("audoutStartAudioOut failed");
        audoutExit();
        return 0;
    }

    // One page-aligned allocation for every block, because the service wants
    // each buffer aligned to 0x1000 and a block is exactly one page.
    d.storage = aligned_alloc(0x1000, kBlockBytes * kBlocks);
    if (d.storage == nullptr) {
        TL_LOG_WARN("no memory for the audio buffers");
        audoutStopAudioOut();
        audoutExit();
        return 0;
    }
    std::memset(d.storage, 0, kBlockBytes * kBlocks);

    for (size_t i = 0; i < kBlocks; ++i) {
        AudioOutBuffer& buffer = d.buffers[i];
        buffer.next = nullptr;
        buffer.buffer = static_cast<uint8_t*>(d.storage) + i * kBlockBytes;
        buffer.buffer_size = kBlockBytes;
        buffer.data_size = kBlockBytes;
        buffer.data_offset = 0;
    }
    d.next = 0;
    d.queued = 0;
    d.open = true;

    const u32 rate = audoutGetSampleRate();
    const u32 channels = audoutGetChannelCount();
    TL_LOG_INFO("audout: %u Hz, %u channels", rate, channels);
    // Reported rather than assumed: the mixer produces stereo.
    if (channels != 2) TL_LOG_WARN("audout reports %u channels; the mixer produces 2", channels);
    return static_cast<int>(rate);
}

void close() {
    Device& d = device();
    if (!d.open) return;
    d.open = false;

    // Stopping is what releases every queued buffer, and it is also what wakes
    // a write() blocked in audoutWaitPlayFinish - the same role waveOutReset
    // plays on the desktop.
    audoutStopAudioOut();
    audoutExit();
    std::free(d.storage);
    d.storage = nullptr;
    d.queued = 0;
}

size_t block_frames() { return kBlockFrames; }

void prepare_mixer_thread() {
    // libnx starts a std::thread on the game thread's core below its priority (0x3B against
    // 0x2C), where it would only run when the game blocks - never during a synchronous level load,
    // so the sound would stop. Core 2 gets the mixer, shared only with the log writer below it;
    // core 1 is kept for game work.
    constexpr s32 kMixerCore = 2;
    const u32 from = svcGetCurrentProcessorNumber();
    const Result rc = svcSetThreadCoreMask(CUR_THREAD_HANDLE, kMixerCore, 1u << kMixerCore);
    if (R_FAILED(rc)) {
        TL_LOG_WARN("audio: mixer stays on core %u (0x%x)", from, static_cast<unsigned>(rc));
        return;
    }
    TL_LOG_INFO("audio: mixer moved from core %u to core %u", from, svcGetCurrentProcessorNumber());
}

bool write(const int16_t* interleaved, size_t frames) {
    Device& d = device();
    if (!d.open) return false;
    if (frames > kBlockFrames) frames = kBlockFrames;

    // Wait for a block to come back once every one of them is out with the
    // service. Below that - the first four calls - there is one free already.
    if (d.queued >= kBlocks) {
        AudioOutBuffer* released = nullptr;
        u32 count = 0;
        // A second rather than UINT64_MAX: a service that stops answering must not hold the
        // shutdown that is trying to join this thread.
        if (R_FAILED(audoutWaitPlayFinish(&released, &count, 1000000000ULL))) {
            return d.open;
        }
        if (count == 0) return d.open;
        if (count > d.queued) count = static_cast<u32>(d.queued);
        d.queued -= count;
    }
    if (!d.open) return false;

    AudioOutBuffer& buffer = d.buffers[d.next];
    std::memcpy(buffer.buffer, interleaved, frames * 2 * sizeof(int16_t));
    // A short final block would otherwise play whatever the last full one left
    // in the tail of the page.
    if (frames < kBlockFrames) {
        std::memset(static_cast<uint8_t*>(buffer.buffer) + frames * 2 * sizeof(int16_t), 0,
                    kBlockBytes - frames * 2 * sizeof(int16_t));
    }
    buffer.data_size = kBlockBytes;

    if (R_FAILED(audoutAppendAudioOutBuffer(&buffer))) return false;
    d.next = (d.next + 1) % kBlocks;
    ++d.queued;
    return true;
}

}  // namespace tl::core::audio_device
