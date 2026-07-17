#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

// Forward-declared (ma_device is a real tag/typedef pair in miniaudio.h, not
// an anonymous struct, so this is legal) rather than including miniaudio.h
// here - see class comment below for why.
struct ma_device;

// Plays interleaved stereo 16-bit PCM pushed in from the emulator core via a
// lock-free single-producer/single-consumer ring buffer - PushSamples is
// called from Emulator::RunFrame's retro_run() calls on the main thread
// (the producer); the platform's own audio thread (spun up internally by
// miniaudio - see AudioOutput.cpp) pulls from the same buffer via a device
// callback (the consumer). Fixed 44100Hz stereo, matching the core's own
// always-on sample rate (see beetle-vb-libretro's Blip_Buffer_set_sample_rate
// call) - no resampling.
//
// miniaudio.h itself (core/third_party/miniaudio.h) is ~2MB of source and
// only needed by the .cpp, so m_device is an opaque pointer here rather than
// a real ma_device - keeps that header out of every includer's compile.
class AudioOutput
{
public:
    ~AudioOutput();

    // false if the platform has no usable audio device - callers should keep
    // running silently (PushSamples becomes a no-op) rather than treat that
    // as fatal. Safe to call more than once; a no-op if already initialized.
    bool Initialize();
    void Shutdown();

    // frames = stereo sample pairs (interleaved L,R,L,R,...), not a raw
    // int16_t count. Drops samples rather than blocking if the ring buffer
    // is full - a slow/stalled audio thread shouldn't stall emulation.
    void PushSamples(const int16_t *interleaved, size_t frames);

private:
    static void DataCallback(ma_device *device, void *output, const void *input, uint32_t frameCount);

    static constexpr size_t kSampleRate = 44100;
    static constexpr size_t kRingFrames = kSampleRate / 2; // 0.5s of buffering

    ma_device *m_device = nullptr;       // heap-allocated - see class comment
    std::vector<int16_t> m_ringBuffer;   // interleaved stereo, kRingFrames * 2 int16_t
    std::atomic<size_t> m_writeFrame{0}; // ever-increasing frame counters (not wrapped) - see .cpp
    std::atomic<size_t> m_readFrame{0};
    bool m_initialized = false;
};
