#include "emu/AudioOutput.h"

// miniaudio.h pulls in <windows.h> for its WASAPI backend, which without
// this defines min/max function-like macros that mangle every std::min/
// std::max call in this file (see vb_core's own NOMINMAX in CMakeLists.txt
// for the same fix elsewhere).
#ifndef NOMINMAX
#define NOMINMAX
#endif

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <algorithm>
#include <cstring>

void AudioOutput::DataCallback(ma_device *device, void *output, const void * /*input*/, uint32_t frameCount)
{
    auto *self = static_cast<AudioOutput *>(device->pUserData);
    auto *out = static_cast<int16_t *>(output);

    // Ever-increasing frame counters (never wrapped down to a 0..kRingFrames
    // index directly) - the classic SPSC ring buffer trick: unsigned
    // subtraction between the two still gives the correct "frames available"
    // count even after either counter wraps around size_t's range, without
    // needing a separate "full vs empty" flag.
    const size_t readFrame = self->m_readFrame.load(std::memory_order_relaxed);
    const size_t writeFrame = self->m_writeFrame.load(std::memory_order_acquire);
    const size_t available = writeFrame - readFrame;

    const size_t framesToCopy = std::min<size_t>(frameCount, available);
    for (size_t i = 0; i < framesToCopy; ++i)
    {
        const size_t srcFrame = (readFrame + i) % AudioOutput::kRingFrames;
        out[i * 2 + 0] = self->m_ringBuffer[srcFrame * 2 + 0];
        out[i * 2 + 1] = self->m_ringBuffer[srcFrame * 2 + 1];
    }
    // Silence for whatever the ring buffer couldn't cover (underrun) rather
    // than leaving garbage/repeating the last block.
    if (framesToCopy < frameCount)
        std::memset(out + framesToCopy * 2, 0, (frameCount - framesToCopy) * 2 * sizeof(int16_t));

    self->m_readFrame.store(readFrame + framesToCopy, std::memory_order_release);
}

bool AudioOutput::Initialize()
{
    if (m_initialized)
        return true;

    m_ringBuffer.assign(kRingFrames * 2, 0);

    auto *device = new ma_device();
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_s16;
    config.playback.channels = 2;
    config.sampleRate = static_cast<ma_uint32>(kSampleRate);
    config.dataCallback = DataCallback;
    config.pUserData = this;

    if (ma_device_init(nullptr, &config, device) != MA_SUCCESS)
    {
        delete device;
        return false;
    }

    if (ma_device_start(device) != MA_SUCCESS)
    {
        ma_device_uninit(device);
        delete device;
        return false;
    }

    m_device = device;
    m_initialized = true;
    return true;
}

void AudioOutput::Shutdown()
{
    if (!m_initialized)
        return;

    ma_device_uninit(m_device);
    delete m_device;
    m_device = nullptr;
    m_initialized = false;
}

AudioOutput::~AudioOutput() { Shutdown(); }

void AudioOutput::PushSamples(const int16_t *interleaved, size_t frames)
{
    if (!m_initialized || frames == 0)
        return;

    const size_t writeFrame = m_writeFrame.load(std::memory_order_relaxed);
    const size_t readFrame = m_readFrame.load(std::memory_order_acquire);
    const size_t used = writeFrame - readFrame;
    const size_t freeFrames = kRingFrames - used;              // "free" collides with a CRT debug macro on MSVC
    const size_t framesToWrite = std::min(frames, freeFrames); // drop the rest rather than block or overwrite unread data

    for (size_t i = 0; i < framesToWrite; ++i)
    {
        const size_t dstFrame = (writeFrame + i) % kRingFrames;
        m_ringBuffer[dstFrame * 2 + 0] = interleaved[i * 2 + 0];
        m_ringBuffer[dstFrame * 2 + 1] = interleaved[i * 2 + 1];
    }

    m_writeFrame.store(writeFrame + framesToWrite, std::memory_order_release);
}
