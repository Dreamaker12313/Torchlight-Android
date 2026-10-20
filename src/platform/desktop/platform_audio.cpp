// Desktop audio output: waveOut, out of winmm (already linked for OGRE). Mixing, resampling and
// decode are shared in src/core/audio.cpp; this is only the device.
//
// Four buffers of a fixed block, cycled: write() waits for a free one, so write() is the mixer's
// clock, which src/core/audio.cpp relies on.
#include "core/audio.h"

#include "core/log.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
// After windows.h, whose types it uses.
#include <mmsystem.h>
#else
#include <alsa/asoundlib.h>

#include <cerrno>
#include <chrono>
#include <mutex>
#include <thread>
#endif

#include <cstring>
#include <vector>

namespace tl::core::audio_device {

#if defined(_WIN32)

namespace {

/// Frames per block, and how many blocks are in flight: 1024 frames is 21 ms at 48 kHz, four of
/// them 85 ms - enough that a 60 ms frame does not starve the device.
constexpr size_t kBlockFrames = 1024;
constexpr size_t kBlocks = 4;

/// 48 kHz to match libnx's fixed `audout`, so the 44.1 kHz resampler runs on both targets.
constexpr int kRate = 48000;

struct Device {
    HWAVEOUT handle = nullptr;
    HANDLE done = nullptr;
    std::vector<WAVEHDR> headers;
    std::vector<int16_t> storage;
    size_t next = 0;
    bool open = false;
};

Device& device() {
    static Device d;
    return d;
}

}  // namespace

int open() {
    Device& d = device();
    if (d.open) return kRate;

    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 2;
    format.nSamplesPerSec = kRate;
    format.wBitsPerSample = 16;
    format.nBlockAlign = static_cast<WORD>(format.nChannels * format.wBitsPerSample / 8);
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

    d.done = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (d.done == nullptr) return 0;

    const MMRESULT result = waveOutOpen(&d.handle, WAVE_MAPPER, &format,
                                        reinterpret_cast<DWORD_PTR>(d.done), 0, CALLBACK_EVENT);
    if (result != MMSYSERR_NOERROR) {
        TL_LOG_WARN("waveOutOpen failed (%u)", static_cast<unsigned>(result));
        CloseHandle(d.done);
        d.done = nullptr;
        return 0;
    }

    d.storage.assign(kBlocks * kBlockFrames * 2, 0);
    d.headers.assign(kBlocks, WAVEHDR{});
    for (size_t i = 0; i < kBlocks; ++i) {
        WAVEHDR& header = d.headers[i];
        header.lpData = reinterpret_cast<LPSTR>(d.storage.data() + i * kBlockFrames * 2);
        header.dwBufferLength = static_cast<DWORD>(kBlockFrames * 2 * sizeof(int16_t));
        waveOutPrepareHeader(d.handle, &header, sizeof(WAVEHDR));
        // Marked done so the first four write() calls find a free block rather
        // than waiting for an event that will never come.
        header.dwFlags |= WHDR_DONE;
    }
    d.next = 0;
    d.open = true;
    return kRate;
}

void close() {
    Device& d = device();
    if (!d.open) return;
    d.open = false;

    // Reset before unpreparing: waveOutUnprepareHeader refuses a header that is
    // still queued, and reset() is what returns every one of them at once. It
    // also signals the event, which is what wakes a write() blocked in here.
    waveOutReset(d.handle);
    for (WAVEHDR& header : d.headers) {
        waveOutUnprepareHeader(d.handle, &header, sizeof(WAVEHDR));
    }
    waveOutClose(d.handle);
    d.handle = nullptr;
    if (d.done != nullptr) {
        SetEvent(d.done);
        CloseHandle(d.done);
        d.done = nullptr;
    }
    d.headers.clear();
    d.storage.clear();
}

size_t block_frames() { return kBlockFrames; }

bool write(const int16_t* interleaved, size_t frames) {
    Device& d = device();
    if (!d.open) return false;
    if (frames > kBlockFrames) frames = kBlockFrames;

    WAVEHDR& header = d.headers[d.next];
    while ((header.dwFlags & WHDR_DONE) == 0) {
        // One second rather than INFINITE: a device that has been yanked out -
        // a USB headset unplugged - would otherwise hang the mixer thread and
        // with it the shutdown that is trying to join it.
        if (WaitForSingleObject(d.done, 1000) == WAIT_OBJECT_0) continue;
        if (!d.open) return false;
    }
    if (!d.open) return false;

    std::memcpy(header.lpData, interleaved, frames * 2 * sizeof(int16_t));
    header.dwBufferLength = static_cast<DWORD>(frames * 2 * sizeof(int16_t));
    header.dwFlags &= ~WHDR_DONE;
    if (waveOutWrite(d.handle, &header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) return false;

    d.next = (d.next + 1) % kBlocks;
    return true;
}

#else

// Linux: ALSA's default device (PulseAudio or PipeWire behind it on a desktop,
// WSLg's server under WSL), 48 kHz stereo as the other targets. The device is
// non-blocking and write() waits in short sleeps, still the mixer's clock,
// so that close() - called from the thread that joins the mixer - can take the
// handle away under the lock without a write() blocked inside it.
namespace {

constexpr size_t kBlockFrames = 1024;
constexpr int kRate = 48000;
constexpr unsigned kLatencyUs = 85000;  // four blocks, as the Windows build queues

struct Device {
    std::mutex lock;
    snd_pcm_t* pcm = nullptr;
};

Device& device() {
    static Device d;
    return d;
}

}  // namespace

int open() {
    Device& d = device();
    const std::lock_guard<std::mutex> guard(d.lock);
    if (d.pcm != nullptr) return kRate;
    if (snd_pcm_open(&d.pcm, "default", SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK) < 0) {
        d.pcm = nullptr;
        TL_LOG_WARN("audio: no ALSA playback device");
        return 0;
    }
    if (snd_pcm_set_params(d.pcm, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, 2, kRate, 1,
                           kLatencyUs) < 0) {
        TL_LOG_WARN("audio: the ALSA device takes no 48 kHz stereo");
        snd_pcm_close(d.pcm);
        d.pcm = nullptr;
        return 0;
    }
    return kRate;
}

void close() {
    Device& d = device();
    const std::lock_guard<std::mutex> guard(d.lock);
    if (d.pcm == nullptr) return;
    snd_pcm_drop(d.pcm);
    snd_pcm_close(d.pcm);
    d.pcm = nullptr;
}

size_t block_frames() { return kBlockFrames; }

bool write(const int16_t* interleaved, size_t frames) {
    Device& d = device();
    while (frames > 0) {
        snd_pcm_sframes_t written = 0;
        {
            const std::lock_guard<std::mutex> guard(d.lock);
            if (d.pcm == nullptr) return false;
            written = snd_pcm_writei(d.pcm, interleaved, frames);
            if (written == -EPIPE || written == -ESTRPIPE) {
                written = snd_pcm_recover(d.pcm, static_cast<int>(written), 1);
                if (written < 0) return false;
                continue;
            }
        }
        if (written == -EAGAIN) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }
        if (written < 0) return false;
        interleaved += static_cast<size_t>(written) * 2;
        frames -= static_cast<size_t>(written);
    }
    return true;
}

#endif

// The operating system's scheduler already spreads threads across cores.
void prepare_mixer_thread() {}

}  // namespace tl::core::audio_device
