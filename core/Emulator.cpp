#include "Emulator.h"

#include <libretro.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>

namespace
{
// libretro's C API has no per-instance context pointer on its callbacks, so
// there's nowhere to stash a `this` - this whole block is necessarily
// global/singleton state, same as the core itself only supports one loaded
// game at a time. Fine here since the app only ever has one Emulator.
constexpr uint32_t kFbWidth = 384 * 2 + 256;  // matches libretro.cpp's FB_WIDTH
constexpr uint32_t kFbHeight = 224 * 2;       // matches libretro.cpp's FB_HEIGHT

// Set by the video_cb callback each retro_run() call. The pointer is always
// the core's own persistent surf.pixels buffer (allocated once in
// retro_load_game, kFbWidth*kFbHeight*4 bytes) - never null once a ROM is
// loaded, so it's safe to memcpy the whole fixed-size buffer every time
// regardless of what the current DisplayRect sub-region actually is.
const void *g_pendingFrame = nullptr;
unsigned g_pendingWidth = 0;
unsigned g_pendingHeight = 0;
bool g_frameReady = false;

// Set by Emulator::SetGameplayInput each app frame, read back by
// RetroInputState - see VBButtonBit in Emulator.h for what each bit means.
uint32_t g_joypadBitmask = 0;

// The core has at least one call site (SettingChanged's "3D mode changed"
// log line) that calls log_cb unconditionally with no null check, unlike
// every other log_cb use in libretro.cpp - so GET_LOG_INTERFACE can't just
// return false like the rest of the environment calls this pass doesn't
// otherwise care about. Without this, that log line dereferences a null
// function pointer the moment the core changes 3D mode during
// retro_load_game, crashing (found via a real access violation there).
void RetroLogPrintf(retro_log_level, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, fmt, args);
    va_end(args);
}

bool RetroEnvironment(unsigned cmd, void *data)
{
    switch (cmd)
    {
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
    {
        auto *cb = static_cast<retro_log_callback *>(data);
        if (!cb)
            return false;
        cb->log = RetroLogPrintf;
        return true;
    }
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
    {
        // Only XRGB8888 is supported here - UiRenderer's streaming texture
        // is created as VK_FORMAT_B8G8R8A8_UNORM, which is exactly this
        // format's in-memory byte order (Rshift16/Gshift8/Bshift0/Ashift24
        // on little-endian == bytes B,G,R,A) - no CPU-side conversion.
        const auto *fmt = static_cast<const retro_pixel_format *>(data);
        return fmt && *fmt == RETRO_PIXEL_FORMAT_XRGB8888;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE:
    {
        auto *var = static_cast<retro_variable *>(data);
        if (!var || !var->key)
            return false;
        // Force side-by-side stereo - see Emulator.h's class comment. Every
        // other variable this core asks about just falls back to its own
        // built-in default (returning false here is safe - libretro.cpp
        // guards every other GET_VARIABLE call with `&& var.value`).
        if (std::strcmp(var->key, "vb_3dmode") == 0)
        {
            var->value = "side-by-side";
            return true;
        }
        return false;
    }
    // Silently accept/ignore anything else this core probes for (performance
    // interface, input descriptors, geometry updates, overscan, variable-
    // update polling, ...) - none of it is needed for video-only playback
    // this pass.
    default:
        return false;
    }
}

void RetroVideoRefresh(const void *data, unsigned width, unsigned height, size_t pitch)
{
    if (!data)
        return; // core is signalling "same as last frame" (e.g. hardware-render path) - not used by this core

    // TEMP debug: confirm frames are actually arriving and aren't just a
    // black/zeroed buffer (which would mean the core is running but not
    // drawing, vs. this data never reaching the screen at all). Scans the
    // *whole* valid width x height region row-by-row (respecting pitch,
    // not just the first N flat bytes - the VB's initial "Health and
    // Safety" text is roughly screen-centered, so a truncated top-rows-only
    // sample can read as all-zero even when the frame has real content
    // further down).
    static int frameCount = 0;
    if (frameCount < 5 || frameCount % 120 == 0)
    {
        const auto *bytes = static_cast<const uint8_t *>(data);
        uint64_t sum = 0;
        uint8_t maxByte = 0;
        for (unsigned row = 0; row < height; ++row)
        {
            const uint8_t *rowBytes = bytes + row * pitch;
            for (unsigned col = 0; col < width * 4; ++col)
            {
                sum += rowBytes[col];
                if (rowBytes[col] > maxByte)
                    maxByte = rowBytes[col];
            }
        }
        std::fprintf(stderr, "[Emulator] video_cb #%d: %ux%u pitch=%zu byteSum=%llu maxByte=%u\n", frameCount, width,
                    height, pitch, static_cast<unsigned long long>(sum), maxByte);
    }
    ++frameCount;

    g_pendingFrame = data;
    g_pendingWidth = width;
    g_pendingHeight = height;
    g_frameReady = true;
}

void RetroAudioSampleNoop(int16_t, int16_t) {}

// Audio is silent this pass - discard everything, report all frames consumed
// (some frontends validate the return value against what they passed in).
size_t RetroAudioSampleBatchNoop(const int16_t *, size_t frames) { return frames; }

// No-op: g_joypadBitmask is refreshed once per app frame by
// Emulator::SetGameplayInput (called before RunFrame's retro_run() calls),
// not by polling a device here.
void RetroInputPollNoop() {}

int16_t RetroInputState(unsigned port, unsigned device, unsigned /*index*/, unsigned id)
{
    if (port != 0 || device != RETRO_DEVICE_JOYPAD || id > 31)
        return 0;
    return (g_joypadBitmask & (1u << id)) ? 1 : 0;
}
} // namespace

void Emulator::Initialize(UiRenderer &ui)
{
    m_ui = &ui;

    retro_set_environment(RetroEnvironment);
    retro_set_video_refresh(RetroVideoRefresh);
    retro_set_audio_sample(RetroAudioSampleNoop);
    retro_set_audio_sample_batch(RetroAudioSampleBatchNoop);
    retro_set_input_poll(RetroInputPollNoop);
    retro_set_input_state(RetroInputState);
    retro_init();
    m_coreInitialized = true;

    // Fixed max-size streaming texture (see UiRenderer::CreateStreamingImage)
    // - big enough for any 3D mode the core supports, even though we only
    // ever force side-by-side. DrawScreen UV-crops to whatever the current
    // frame's actual valid region is.
    m_screenTexture = ui.CreateStreamingImage(kFbWidth, kFbHeight, VK_FORMAT_B8G8R8A8_UNORM);
    m_frameBufferRgba.resize(static_cast<size_t>(kFbWidth) * kFbHeight * 4);
}

bool Emulator::LoadRom(const std::string &romPath)
{
    if (!m_coreInitialized)
        return false;

    std::ifstream in(romPath, std::ios::binary);
    if (!in)
        return false;
    const std::vector<uint8_t> romBytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (romBytes.empty())
        return false;

    if (m_romLoaded)
    {
        retro_unload_game();
        m_romLoaded = false;
    }

    retro_game_info info{};
    info.path = romPath.c_str(); // need_fullpath is false, but the core prefers a real path over null
    info.data = romBytes.data();
    info.size = romBytes.size();

    m_romLoaded = retro_load_game(&info);
    m_frameAccumulator = 0.0f;
    std::fprintf(stderr, "[Emulator] LoadRom(\"%s\"): %zu bytes read, retro_load_game -> %s\n", romPath.c_str(),
                romBytes.size(), m_romLoaded ? "success" : "FAILED");
    return m_romLoaded;
}

void Emulator::SetGameplayInput(uint32_t joypadBitmask) { g_joypadBitmask = joypadBitmask; }

void Emulator::RunFrame(float deltaSeconds)
{
    if (!m_romLoaded)
        return;

    const float framePeriod = 1.0f / kCoreFps;
    m_frameAccumulator += deltaSeconds;

    // Cap catch-up so a debugger pause/hitch doesn't spin retro_run() an
    // enormous number of times on the next real frame.
    const float kMaxCatchUp = framePeriod * 4.0f;
    if (m_frameAccumulator > kMaxCatchUp)
        m_frameAccumulator = kMaxCatchUp;

    bool ranAny = false;
    int runCount = 0;
    while (m_frameAccumulator >= framePeriod)
    {
        retro_run();
        m_frameAccumulator -= framePeriod;
        ranAny = true;
        ++runCount;
    }

    // TEMP debug: confirm RunFrame is actually being called/pumping
    // retro_run(), and whether a new frame made it to UpdateStreamingImage.
    static int callCount = 0;
    if (callCount < 5 || callCount % 120 == 0)
    {
        std::fprintf(stderr, "[Emulator] RunFrame #%d: deltaSeconds=%.4f runCount=%d g_frameReady=%d\n", callCount,
                    deltaSeconds, runCount, g_frameReady ? 1 : 0);
    }
    ++callCount;

    if (ranAny && g_frameReady && m_ui)
    {
        // Force alpha to opaque while copying - see m_frameBufferRgba's doc
        // comment for why the core's own buffer can't be uploaded directly.
        const auto *src = static_cast<const uint8_t *>(g_pendingFrame);
        for (size_t i = 0; i < m_frameBufferRgba.size(); i += 4)
        {
            m_frameBufferRgba[i + 0] = src[i + 0];
            m_frameBufferRgba[i + 1] = src[i + 1];
            m_frameBufferRgba[i + 2] = src[i + 2];
            m_frameBufferRgba[i + 3] = 0xFF;
        }
        m_ui->UpdateStreamingImage(m_screenTexture, m_frameBufferRgba.data(), m_frameBufferRgba.size());
        m_lastFrameWidth = g_pendingWidth > 0 ? g_pendingWidth : kSideBySideWidth;
        m_lastFrameHeight = g_pendingHeight > 0 ? g_pendingHeight : kSideBySideHeight;
        g_frameReady = false;
    }
}

void Emulator::DrawScreen(UiRenderer &ui, float x, float y, float w, float h, Eye eye) const
{
    if (!m_screenTexture.IsValid())
        return;
    // UV-crop the fixed-size streaming texture down to just the current
    // frame's valid region, stretched to fill the destination rect - same
    // "stretch to fill" behaviour the old static-image stub had. For a
    // single eye, crop that region's left/right half on top of the same
    // valid-region crop (the combined frame is left-eye-then-right-eye).
    const float fullU1 = static_cast<float>(m_lastFrameWidth) / static_cast<float>(kFbWidth);
    const float v1 = static_cast<float>(m_lastFrameHeight) / static_cast<float>(kFbHeight);

    float u0 = 0.0f;
    float u1 = fullU1;
    if (eye == Eye::Left)
        u1 = fullU1 * 0.5f;
    else if (eye == Eye::Right)
        u0 = fullU1 * 0.5f;

    // TEMP debug: confirm DrawScreen is actually being called and with what
    // rect/UV crop.
    static int drawCount = 0;
    if (drawCount < 5 || drawCount % 120 == 0)
    {
        std::fprintf(stderr, "[Emulator] DrawScreen #%d: rect=(%.1f,%.1f,%.1f,%.1f) uv=(%.3f,%.3f) romLoaded=%d\n",
                    drawCount, x, y, w, h, u0, u1, m_romLoaded ? 1 : 0);
    }
    ++drawCount;

    ui.DrawImageRegion(m_screenTexture, x, y, w, h, u0, 0.0f, u1, v1);
}
