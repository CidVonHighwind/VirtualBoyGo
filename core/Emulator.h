#pragma once

#include "ui/UiRenderer.h"

#include <cstdint>
#include <string>
#include <vector>

// Bit positions for Emulator::SetGameplayInput's bitmask - one bit per VB
// button. Values match libretro's RETRO_DEVICE_ID_JOYPAD_* ids exactly (see
// beetle-vb-libretro/libretro.cpp's retro_load_game input descriptor table,
// which is what actually defines which VB button each id means - the VB has
// two D-pads, not one, hence Left*/Right* instead of a single Up/Down/Left/
// Right) so Emulator.cpp's input_state_cb can use id directly as a bit
// index. Kept here (not libretro.h) so frontends (Main.cpp/OpenXrApp) don't
// need to include the core's headers just to feed it input.
namespace VBButtonBit
{
    constexpr uint32_t B = 0;
    constexpr uint32_t Select = 2;
    constexpr uint32_t Start = 3;
    constexpr uint32_t LeftUp = 4;
    constexpr uint32_t LeftDown = 5;
    constexpr uint32_t LeftLeft = 6;
    constexpr uint32_t LeftRight = 7;
    constexpr uint32_t A = 8;
    constexpr uint32_t L = 10;
    constexpr uint32_t R = 11;
    constexpr uint32_t RightUp = 12;
    constexpr uint32_t RightLeft = 13;
    constexpr uint32_t RightDown = 14;
    constexpr uint32_t RightRight = 15;
} // namespace VBButtonBit

// Thin adapter over the real Virtual Boy core (libretro/beetle-vb-libretro,
// vendored as a git submodule at third_party/beetle-vb-libretro - see that
// folder's COPYING for its GPL-2.0 license), statically linked and called
// directly through its standard libretro.h API (retro_load_game/retro_run/
// etc.), not dlopen'd. See RunFrame/the retro_* callback implementations in
// Emulator.cpp for what's still stubbed (audio is discarded).
//
// The core always renders in "side-by-side" 3D mode (forced via this
// class's environment callback) - both VB eyes packed into one wide
// combined frame - which DrawScreen exposes as a single texture; splitting
// that into two per-eye OpenXR quad layers (via subImage.imageRect crops)
// is OpenXrApp's job, not this class's.
class Emulator
{
   public:
    // Screens are shown at this fixed integer upscale (pixel-perfect,
    // nearest-neighbor - see UiRenderer::LoadImage) everywhere the emulator
    // screen is displayed, so PC2D and the headset builds look consistent.
    static constexpr int kScale = 3;

    // Fixed side-by-side-mode geometry (384-wide base VB screen * 2 eyes,
    // 224 tall) - used to size the screen swapchain/texture up front, before
    // any ROM is loaded (GetScreenWidth/Height must be valid immediately
    // after Initialize(), well before the user picks a ROM from the menu).
    // This is an assumption about what libretro.cpp's side-by-side geometry
    // actually reports - the core's own base geometry constants
    // (MEDNAFEN_CORE_GEOMETRY_BASE_W/H = 384/224) are fixed regardless of
    // ROM content, so this should hold for any ROM, but hasn't been verified
    // against the real DisplayRect the core reports at runtime yet. If it's
    // off, the visible symptom is a stretched/letterboxed aspect ratio, not
    // a crash - adjust these two constants once confirmed.
    static constexpr uint32_t kSideBySideWidth = 384 * 2;
    static constexpr uint32_t kSideBySideHeight = 224;

    void Initialize(UiRenderer &ui);

    // Reads romPath's bytes and hands them to the core via retro_load_game.
    // Safe to call more than once (unloads whatever ROM was previously
    // loaded first). Returns false if the file couldn't be read or the core
    // rejected it.
    bool LoadRom(const std::string &romPath);

    // True once Initialize() has set up the streaming screen texture -
    // *not* tied to whether a ROM is loaded, so the screen quad layer/
    // texture exists (showing black) from app start, and DrawScreen doesn't
    // need special-casing for the "no ROM loaded yet" state.
    bool HasScreen() const { return m_screenTexture.IsValid(); }
    uint32_t GetScreenWidth() const { return kSideBySideWidth; }
    uint32_t GetScreenHeight() const { return kSideBySideHeight; }

    // Fixed-timestep accumulator against the VB's native ~50.27Hz refresh -
    // call once per app frame with the same deltaSeconds already computed
    // for AppMenu::Update. Runs retro_run() zero or more times to catch up,
    // then re-uploads the latest video frame to the streaming texture if at
    // least one retro_run() happened. A no-op before any ROM is loaded.
    void RunFrame(float deltaSeconds);

    // Sets the VB gamepad state RunFrame's next retro_run() call(s) will
    // read - bits per VBButtonBit, 1 = held. Call once per app frame (before
    // RunFrame) with whatever the current frontend's input maps to; pass 0
    // while the menu is open so gameplay input doesn't leak through it.
    void SetGameplayInput(uint32_t joypadBitmask);

    // Which half of the combined side-by-side frame DrawScreen shows -
    // Both is the real stereo image (what OpenXrApp uses, splitting it into
    // per-eye quad layers itself); Left/Right are for flat/mono display
    // (pc2d's debug window) where there's no second eye to show the other
    // half to. TODO: expose Left vs Right as a user-facing setting instead
    // of pc2d hardcoding Left - filed as a known follow-up, not implemented
    // yet.
    enum class Eye
    {
        Both,
        Left,
        Right
    };

    // Stretches the current screen into the given destination rect (in
    // target pixels) - callers typically size that rect to
    // GetScreenWidth/Height() * kScale for a pixel-perfect look (Both only -
    // Left/Right are half that width, see Eye). Draws whatever the last
    // RunFrame produced (all-black before any ROM loads). tint multiplies
    // the drawn pixels (white = no-op) - the VB color palette/custom RGB
    // tint (AppSettings::colorR/G/B) is applied this way, since the core
    // itself has no palette concept and always outputs pre-colored frames.
    void DrawScreen(UiRenderer &ui, float x, float y, float w, float h, Eye eye = Eye::Both,
                    const XrColor4f &tint = XrColor4f{1.0f, 1.0f, 1.0f, 1.0f}) const;

    // UI slots are 1-9, shifted by one internally so slot 1 maps to
    // FrontendGo's unsuffixed slot 0 (see StateFilePath). Raw
    // retro_serialize dump, binary-compatible with FrontendGo's .state[N].
    bool SaveState(int uiSlot);
    bool LoadState(int uiSlot);
    bool SaveStateExists(int uiSlot) const;

    // Native VB resolution - matches FrontendGo's .stateimg exactly.
    static constexpr uint32_t kPreviewWidth = 384;
    static constexpr uint32_t kPreviewHeight = 224;

    // Reads the preview from the last SaveState(uiSlot) call, expanded to
    // RGBA (R=G=B=lum, A=255) - false if that slot has never been saved.
    // On-disk format (.stateimg[N]) is raw grayscale, byte-compatible with
    // FrontendGo; palette tint is applied at display time, not baked in.
    bool LoadStatePreview(int uiSlot, std::vector<uint8_t> &outRgba) const;

    // Flushes cart SRAM for the currently-loaded ROM, if any - call once on
    // app exit (LoadRom already flushes on every ROM switch).
    void Shutdown();

   private:
    // <m_romStateDir>/<m_romBaseName>.<ext><suffix>; suffix empty for
    // uiSlot==1, else uiSlot-1.
    std::string StateFilePath(int uiSlot, const char *ext) const;

    void CaptureScreenshotGrayscale(std::vector<uint8_t> &outGray) const;

    // Cart battery-save, matches FrontendGo's <romDir>/<stem>.srm. Must run
    // before retro_unload_game() - the SRAM pointer isn't valid after.
    void SaveRam();
    void LoadRam();

    UiRenderer *m_ui = nullptr;
    UiImageHandle m_screenTexture;
    bool m_coreInitialized = false;
    bool m_romLoaded = false;

    // Set by LoadRom - the currently-loaded ROM's own directory (for the
    // .srm SRAM path, next to the ROM) and a "<romDir>/States" subfolder +
    // filename stem (for save-state/preview paths). Empty/unset before any
    // ROM has ever loaded, in which case SaveRam/SaveState etc. are no-ops.
    std::string m_romDir;
    std::string m_romStateDir;
    std::string m_romBaseName;

    // Native VB refresh rate (retro_get_system_av_info's timing.fps) -
    // RunFrame accumulates real deltaSeconds against this to decide how many
    // times to call retro_run() per app frame.
    static constexpr float kCoreFps = 50.27f;
    float m_frameAccumulator = 0.0f;

    // Updated by the video_cb callback each retro_run() call - the portion
    // of the fixed-size streaming texture that's actually valid for the
    // current frame (used to UV-crop in DrawScreen). Defaults to the
    // side-by-side geometry assumption above until the first real frame.
    uint32_t m_lastFrameWidth = kSideBySideWidth;
    uint32_t m_lastFrameHeight = kSideBySideHeight;

    // The core's XRGB8888 output has an unused byte in the alpha position
    // (not a real alpha channel - typically 0), but ui_image.frag multiplies
    // its output alpha by whatever the sampled texture's alpha channel is.
    // Every other UiRenderer image source has real alpha (stb_image forces
    // opaque alpha for alpha-less PNGs, icons are authored with real alpha),
    // so this is the first data source where that byte is meaningless - copy
    // each frame through this buffer forcing alpha to 0xFF rather than
    // uploading the core's buffer directly, or the screen renders fully
    // transparent (black, since nothing else is behind it).
    std::vector<uint8_t> m_frameBufferRgba;
};
