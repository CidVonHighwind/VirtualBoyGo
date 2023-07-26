
#include <stdarg.h>

#include "mednafen.h"
#include "mempatcher.h"
#include "git.h"

static MDFNGI *game;
static bool overscan;
static double last_sound_rate;
static MDFN_PixelFormat last_pixel_format;
static MDFN_Surface *surf;

#include "vb/vb.h"
#include "vb/timer.h"
#include "vb/vsu.h"
#include "vb/vip.h"
#include "vb/input.h"
#include "mempatcher.h"
#include "vrvb.h"

#define MAX_PLAYERS 1
#define MAX_BUTTONS 14

enum {
    ANAGLYPH_PRESET_DISABLED = 0,
    ANAGLYPH_PRESET_RED_BLUE,
    ANAGLYPH_PRESET_RED_CYAN,
    ANAGLYPH_PRESET_RED_ELECTRICCYAN,
    ANAGLYPH_PRESET_RED_GREEN,
    ANAGLYPH_PRESET_GREEN_MAGENTA,
    ANAGLYPH_PRESET_YELLOW_BLUE,
};

static const uint32 AnaglyphPreset_Colors[][2] = {
        {0x000000, 0x000000},
        {0xFF0000, 0x0000FF},
        {0xFF0000, 0x00B7EB},
        {0xFF0000, 0x00FFFF},
        {0xFF0000, 0x00FF00},
        {0x00FF00, 0xFF00FF},
        {0xFFFF00, 0x0000FF},
};

#define STICK_DEADZONE 0x4000
#define RIGHT_DPAD_LEFT 0x1000
#define RIGHT_DPAD_RIGHT 0x0020
#define RIGHT_DPAD_UP 0x0010
#define RIGHT_DPAD_DOWN 0x2000

int32 VB_InDebugPeek;

// static uint32 VB3DMode = 2;

static Blip_Buffer sbuf[2];

static uint8 *WRAM = NULL;

static uint8 *GPRAM = NULL;
static uint32 GPRAM_Mask;

static uint8 *GPROM = NULL;
static uint32 GPROM_Mask;

V810 *VB_V810 = NULL;

VSU *VB_VSU = NULL;
static uint32 VSU_CycleFix;

static uint8 WCR;

static int32 next_vip_ts, next_timer_ts, next_input_ts;

static uint32 IRQ_Asserted;

static bool initial_ports_hookup = false;

#define MEDNAFEN_CORE_NAME_MODULE "vb"
#define MEDNAFEN_CORE_NAME "Mednafen VB"
#define MEDNAFEN_CORE_VERSION "v0.9.36.1"
#define MEDNAFEN_CORE_EXTENSIONS "vb|vboy|bin"
#define MEDNAFEN_CORE_TIMING_FPS 50.27
#define MEDNAFEN_CORE_GEOMETRY_BASE_W (game->nominal_width)
#define MEDNAFEN_CORE_GEOMETRY_BASE_H (game->nominal_height)
#define MEDNAFEN_CORE_GEOMETRY_MAX_W 384
#define MEDNAFEN_CORE_GEOMETRY_MAX_H 224
#define MEDNAFEN_CORE_GEOMETRY_ASPECT_RATIO (12.0 / 7.0)
//#define FB_WIDTH 768
#define FB_WIDTH 384
#define FB_HEIGHT 224 * 2 + 12

#define FB_MAX_HEIGHT FB_HEIGHT

const char *mednafen_core_str = MEDNAFEN_CORE_NAME;

static INLINE int32 CalcNextTS(void) {
    int32 next_timestamp = next_vip_ts;

    if (next_timestamp > next_timer_ts)
        next_timestamp = next_timer_ts;

    if (next_timestamp > next_input_ts)
        next_timestamp = next_input_ts;

    return (next_timestamp);
}

static INLINE void RecalcIntLevel(void) {
    int ilevel = -1;

    for (int i = 4; i >= 0; i--) {
        if (IRQ_Asserted & (1 << i)) {
            ilevel = i;
            break;
        }
    }

    VB_V810->SetInt(ilevel);
}

// Called externally from debug.cpp in some cases.
void ForceEventUpdates(const v810_timestamp_t timestamp) {
    next_vip_ts = VIP_Update(timestamp);
    next_timer_ts = TIMER_Update(timestamp);
    next_input_ts = VBINPUT_Update(timestamp);

    VB_V810->SetEventNT(CalcNextTS());
    // printf("FEU: %d %d %d\n", next_vip_ts, next_timer_ts, next_input_ts);
}

void VB_SetEvent(const int type, const v810_timestamp_t next_timestamp) {
    // assert(next_timestamp > VB_V810->v810_timestamp);

    if (type == VB_EVENT_VIP)
        next_vip_ts = next_timestamp;
    else if (type == VB_EVENT_TIMER)
        next_timer_ts = next_timestamp;
    else if (type == VB_EVENT_INPUT)
        next_input_ts = next_timestamp;

    if (next_timestamp < VB_V810->GetEventNT())
        VB_V810->SetEventNT(next_timestamp);
}

void VB_ExitLoop(void) { VB_V810->Exit(); }

void VBIRQ_Assert(int source, bool assert) {
    assert(source >= 0 && source <= 4);

    IRQ_Asserted &= ~(1 << source);

    if (assert)
        IRQ_Asserted |= 1 << source;

    RecalcIntLevel();
}

int StateAction(StateMem *sm, int load, int data_only) {
    const v810_timestamp_t timestamp = VB_V810->v810_timestamp;
    int ret = 1;

    SFORMAT StateRegs[] = {
            SFARRAY(WRAM, 65536), SFARRAY(GPRAM, GPRAM_Mask ? (GPRAM_Mask + 1) : 0),
            SFVAR(WCR), SFVAR(IRQ_Asserted),
            SFVAR(VSU_CycleFix), SFEND};

    ret &= MDFNSS_StateAction(sm, load, data_only, StateRegs, "MAIN", false);

    ret &= VB_V810->StateAction(sm, load, data_only);

    ret &= VB_VSU->StateAction(sm, load, data_only);
    ret &= TIMER_StateAction(sm, load, data_only);
    ret &= VBINPUT_StateAction(sm, load, data_only);
    ret &= VIP_StateAction(sm, load, data_only);

    if (load) {
        // Needed to recalculate next_*_ts since we don't bother storing their
        // deltas in save states.
        ForceEventUpdates(timestamp);
    }
    return (ret);
}

// --

static void SettingChanged(const char *name) {
    if (!strcasecmp(name, "vb.3dmode")) {
        // FIXME, TODO (complicated)
        //VB3DMode = MDFN_GetSettingUI("vb.3dmode");
        //VIP_Set3DMode(VB3DMode);
    } else if (!strcasecmp(name, "vb.disable_parallax")) {
        VIP_SetParallaxDisable(MDFN_GetSettingB("vb.disable_parallax"));

    } else if (!strcasecmp(name, "vb.anaglyph.lcolor") || !strcasecmp(name, "vb.anaglyph.rcolor") ||
               !strcasecmp(name, "vb.anaglyph.preset") || !strcasecmp(name, "vb.default_color")) {

        uint32 lcolor = MDFN_GetSettingUI("vb.anaglyph.lcolor"),
                rcolor = MDFN_GetSettingUI("vb.anaglyph.rcolor");

        int preset = MDFN_GetSettingI("vb.anaglyph.preset");

        if (preset != ANAGLYPH_PRESET_DISABLED) {
            lcolor = AnaglyphPreset_Colors[preset][0];
            rcolor = AnaglyphPreset_Colors[preset][1];
        }

        VIP_SetAnaglyphColors(lcolor, rcolor);
        VIP_SetDefaultColor(MDFN_GetSettingUI("vb.default_color"));

    } else if (!strcasecmp(name, "vb.input.instant_read_hack")) {
        VBINPUT_SetInstantReadHack(MDFN_GetSettingB("vb.input.instant_read_hack"));
    } else if (!strcasecmp(name, "vb.instant_display_hack"))
        VIP_SetInstantDisplayHack(MDFN_GetSettingB("vb.instant_display_hack"));
    else if (!strcasecmp(name, "vb.allow_draw_skip"))
        VIP_SetAllowDrawSkip(MDFN_GetSettingB("vb.allow_draw_skip"));
    else
        abort();
}

static const MDFNSetting_EnumList VB3DMode_List[] =
        {
                {"anaglyph",   VB3DMODE_ANAGLYPH,   "Anaglyph",
                        "Used in conjunction with classic dual-lens-color glasses."},
                {"cscope",     VB3DMODE_CSCOPE,     "CyberScope",
                        "Intended for use with the CyberScope 3D device."},
                {"sidebyside", VB3DMODE_SIDEBYSIDE, "Side-by-Side",
                        "The left-eye image is displayed on the left, and the right-eye image is displayed on the right."},
// { "overunder", VB3DMODE_OVERUNDER },
                {"vli",        VB3DMODE_VLI,        "Vertical Line Interlaced",
                        "Vertical lines alternate between left view and right view."},
                {"hli",        VB3DMODE_HLI,        "Horizontal Line Interlaced",
                        "Horizontal lines alternate between left view and right view."},
                {NULL,         0},
        };

static const MDFNSetting_EnumList V810Mode_List[] =
        {
                {"fast",     (int) V810_EMU_MODE_FAST,     "Fast Mode",
                        "Fast mode trades timing accuracy, cache emulation, and executing from hardware registers and RAM not intended for code use for performance."},
                {"accurate", (int) V810_EMU_MODE_ACCURATE, "Accurate Mode",
                        "Increased timing accuracy, though not perfect, along with cache emulation, at the cost of decreased performance.  Additionally, even the pipeline isn't correctly and fully emulated in this mode."},
                {NULL,       0},
        };

static const MDFNSetting_EnumList AnaglyphPreset_List[] =
        {
                {"disabled",         ANAGLYPH_PRESET_DISABLED,         "Disabled",
                                                                                   "Forces usage of custom anaglyph colors."},
                {"0",                ANAGLYPH_PRESET_DISABLED},

                {"red_blue",         ANAGLYPH_PRESET_RED_BLUE,         "Red/Blue", "Classic red/blue anaglyph."},
                {"red_cyan",         ANAGLYPH_PRESET_RED_CYAN,         "Red/Cyan", "Improved quality red/cyan anaglyph."},
                {"red_electriccyan", ANAGLYPH_PRESET_RED_ELECTRICCYAN, "Red/Electric Cyan",
                                                                                   "Alternate version of red/cyan"},
                {"red_green",        ANAGLYPH_PRESET_RED_GREEN,        "Red/Green"},
                {"green_magenta",    ANAGLYPH_PRESET_GREEN_MAGENTA,    "Green/Magenta"},
                {"yellow_blue",      ANAGLYPH_PRESET_YELLOW_BLUE,      "Yellow/Blue"},

                {NULL,               0},
        };

static MDFNSetting VBSettings[] =
        {
                {"vb.cpu_emulation",           MDFNSF_EMU_STATE | MDFNSF_UNTRUSTED_SAFE, "CPU emulation mode.",                           NULL,
                                                                                                                                                MDFNST_ENUM, "fast",           NULL, NULL,               NULL,                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             NULL, V810Mode_List},
                {"vb.input.instant_read_hack", MDFNSF_EMU_STATE | MDFNSF_UNTRUSTED_SAFE,
                                                                                         "Input latency reduction hack.",
                                                                                                                                    "Reduces latency in some games by 20ms by returning the current pad state, rather than latched state, on serial port data reads.  This hack may cause some homebrew software to malfunction, but it should be relatively safe for commercial official games.",
                                                                                                                                                MDFNST_BOOL, "1",              NULL, NULL, NULL,                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       SettingChanged},

                {"vb.instant_display_hack",    MDFNSF_NOFLAGS, "Display latency reduction hack.",
                        "Reduces latency in games by displaying the framebuffer 20ms earlier.  This hack has some potential of causing graphical glitches, so it is disabled by default.",
                                                                                                                                          MDFNST_BOOL,         "0",        NULL, NULL,             NULL,              SettingChanged},
                {"vb.allow_draw_skip",         MDFNSF_EMU_STATE | MDFNSF_UNTRUSTED_SAFE, "Allow draw skipping.",
                        "If vb.instant_display_hack is set to \"1\", and this setting is set to \"1\", then frame-skipping the drawing to the emulated framebuffer will be allowed.  THIS WILL CAUSE GRAPHICAL GLITCHES, AND THEORETICALLY(but unlikely) GAME CRASHES, ESPECIALLY WITH DIRECT FRAMEBUFFER DRAWING GAMES.",
                                                                                                                                                MDFNST_BOOL, "0",              NULL, NULL, NULL, SettingChanged},

                // FIXME: We're going to have to set up some kind of video mode change notification for changing vb.3dmode while the game is running to work properly.
                {"vb.3dmode",                  MDFNSF_NOFLAGS, "3D mode.", NULL,                                                          MDFNST_ENUM, "anaglyph", NULL, NULL,
                                                                                                                                                                               NULL, /*SettingChanged*/                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                NULL, VB3DMode_List},
                {"vb.liprescale",              MDFNSF_NOFLAGS, "Line Interlaced prescale.", NULL,                                         MDFNST_UINT, "2",         "1",
                                                                                                                                                                               "10", NULL,                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             NULL},

                {"vb.disable_parallax",        MDFNSF_EMU_STATE | MDFNSF_UNTRUSTED_SAFE,
                                                                                         "Disable parallax for BG and OBJ rendering.",    NULL, MDFNST_BOOL, "0",              NULL, NULL, NULL,
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       SettingChanged},
                {"vb.default_color",           MDFNSF_NOFLAGS,
                                                               "Default maximum-brightness color to use in non-anaglyph 3D modes.", NULL, MDFNST_UINT,
                                                                                                                                                       "0xF0F0F0", "0x000000", "0xFFFFFF", NULL, SettingChanged},

                {"vb.anaglyph.preset",         MDFNSF_NOFLAGS, "Anaglyph preset colors.",                                           NULL, MDFNST_ENUM,
                                                                                                                                                       "red_blue", NULL, NULL,             NULL, SettingChanged,                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             AnaglyphPreset_List},
                {"vb.anaglyph.lcolor",         MDFNSF_NOFLAGS, "Anaglyph maximum-brightness color for left view.",
                                                                                                                                    NULL, MDFNST_UINT, "0xffba00", "0x000000", "0xFFFFFF", NULL, SettingChanged},
                {"vb.anaglyph.rcolor",         MDFNSF_NOFLAGS, "Anaglyph maximum-brightness color for right view.",
                                                                                                                                    NULL, MDFNST_UINT, "0x00baff", "0x000000", "0xFFFFFF", NULL, SettingChanged},

                {"vb.sidebyside.separation",   MDFNSF_NOFLAGS, "Number of pixels to separate L/R views by.",
                        "This setting refers to pixels before vb.xscale(fs) scaling is taken into consideration.  For example, a value of \"100\" here will result in a separation of 300 screen pixels if vb.xscale(fs) is set to \"3\".",
                                                                                                                                          MDFNST_UINT, /*"96"*/"0", "0",       "1024",             NULL,                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               NULL},

                {"vb.3dreverse",               MDFNSF_NOFLAGS, "Reverse left/right 3D views.",                                      NULL, MDFNST_BOOL,         "0",
                                                                                                                                                                           NULL, NULL,             NULL, SettingChanged},
                {NULL}
        };

MDFNGI EmulatedVB =
        {
                VBSettings,
                MDFN_MASTERCLOCK_FIXED(VB_MASTER_CLOCK),
                0,
                false, // Multires possible?

                0,   // lcm_width
                0,   // lcm_height
                NULL,  // Dummy

                384,    // Nominal width
                224 * 2 + 12,    // Nominal height

                384,    // Framebuffer width
                256 * 2 + 12,    // Framebuffer height

                2,     // Number of output sound channels
        };

MDFNGI *MDFNGameInfo = &EmulatedVB;

struct VB_HeaderInfo {
    char game_title[256];
    uint32 game_code;
    uint16 manf_code;
    uint8 version;
};

static int32 MDFN_FASTCALL EventHandler(const v810_timestamp_t timestamp) {
    if (timestamp >= next_vip_ts)
        next_vip_ts = VIP_Update(timestamp);

    if (timestamp >= next_timer_ts)
        next_timer_ts = TIMER_Update(timestamp);

    if (timestamp >= next_input_ts)
        next_input_ts = VBINPUT_Update(timestamp);

    return (CalcNextTS());
}

static void FixNonEvents(void) {
    if (next_vip_ts & 0x40000000)
        next_vip_ts = VB_EVENT_NONONO;

    if (next_timer_ts & 0x40000000)
        next_timer_ts = VB_EVENT_NONONO;

    if (next_input_ts & 0x40000000)
        next_input_ts = VB_EVENT_NONONO;
}

static void RebaseTS(const v810_timestamp_t timestamp) {
    //printf("Rebase: %08x %08x %08x\n", timestamp, next_vip_ts, next_timer_ts);

    assert(next_vip_ts > timestamp);
    assert(next_timer_ts > timestamp);
    assert(next_input_ts > timestamp);

    next_vip_ts -= timestamp;
    next_timer_ts -= timestamp;
    next_input_ts -= timestamp;
}

static void Emulate(EmulateSpecStruct *espec) {
    v810_timestamp_t v810_timestamp;

    MDFNMP_ApplyPeriodicCheats();

    VBINPUT_Frame();

    if (espec->SoundFormatChanged) {
        for (int y = 0; y < 2; y++) {
            Blip_Buffer_set_sample_rate(&sbuf[y], espec->SoundRate ? espec->SoundRate : 44100, 50);
            Blip_Buffer_set_clock_rate(&sbuf[y], (long) (VB_MASTER_CLOCK / 4));
            Blip_Buffer_bass_freq(&sbuf[y], 20);
        }
    }

    VIP_StartFrame(espec);

    v810_timestamp = VB_V810->Run(EventHandler);

    FixNonEvents();
    ForceEventUpdates(v810_timestamp);

    VB_VSU->EndFrame((v810_timestamp + VSU_CycleFix) >> 2);

    if (espec->SoundBuf) {
        for (int y = 0; y < 2; y++) {
            Blip_Buffer_end_frame(&sbuf[y], (v810_timestamp + VSU_CycleFix) >> 2);
            espec->SoundBufSize =
                    Blip_Buffer_read_samples(&sbuf[y], espec->SoundBuf + y, espec->SoundBufMaxSize);
        }
    }

    VSU_CycleFix = (v810_timestamp + VSU_CycleFix) & 3;

    espec->MasterCycles = v810_timestamp;

    TIMER_ResetTS();
    VBINPUT_ResetTS();
    VIP_ResetTS();

    RebaseTS(v810_timestamp);

    VB_V810->ResetTS(0);
}

static void EventReset(void) {
    next_vip_ts = VB_EVENT_NONONO;
    next_timer_ts = VB_EVENT_NONONO;
    next_input_ts = VB_EVENT_NONONO;
}

static void VB_Power(void) {
    memset(WRAM, 0, 65536);

    VIP_Power();
    VB_VSU->Power();
    TIMER_Power();
    VBINPUT_Power();

    EventReset();
    IRQ_Asserted = 0;
    RecalcIntLevel();
    VB_V810->Reset();

    VSU_CycleFix = 0;
    WCR = 0;

    ForceEventUpdates(0);    //VB_V810->v810_timestamp);
}

static uint8 HWCTRL_Read(v810_timestamp_t &timestamp, uint32 A) {
    uint8 ret = 0;

    if (A & 0x3) {
        puts("HWCtrl Bogus Read?");
        return (ret);
    }

    switch (A & 0xFF) {
        default:
            printf("Unknown HWCTRL Read: %08x\n", A);
            break;

        case 0x18:
        case 0x1C:
        case 0x20:
            ret = TIMER_Read(timestamp, A);
            break;

        case 0x24:
            ret = WCR | 0xFC;
            break;

        case 0x10:
        case 0x14:
        case 0x28:
            ret = VBINPUT_Read(timestamp, A);
            break;

    }

    return (ret);
}

uint8 MDFN_FASTCALL MemRead8(v810_timestamp_t &timestamp, uint32 A) {
    uint8 ret = 0;
    A &= (1 << 27) - 1;

    //if((A >> 24) <= 2)
    // printf("Read8: %d %08x\n", timestamp, A);

    switch (A >> 24) {
        case 0:
            ret = VIP_Read8(timestamp, A);
            break;

        case 1:
            break;

        case 2:
            ret = HWCTRL_Read(timestamp, A);
            break;

        case 3:
            break;
        case 4:
            break;

        case 5:
            ret = WRAM[A & 0xFFFF];
            break;

        case 6:
            if (GPRAM)
                ret = GPRAM[A & GPRAM_Mask];
            else
                printf("GPRAM(Unmapped) Read: %08x\n", A);
            break;

        case 7:
            ret = GPROM[A & GPROM_Mask];
            break;
    }
    return (ret);
}

static void HWCTRL_Write(v810_timestamp_t &timestamp, uint32 A, uint8 V) {
    if (A & 0x3) {
        puts("HWCtrl Bogus Write?");
        return;
    }

    switch (A & 0xFF) {
        default:
            printf("Unknown HWCTRL Write: %08x %02x\n", A, V);
            break;

        case 0x18:
        case 0x1C:
        case 0x20:
            TIMER_Write(timestamp, A, V);
            break;

        case 0x24:
            WCR = V & 0x3;
            break;

        case 0x10:
        case 0x14:
        case 0x28:
            VBINPUT_Write(timestamp, A, V);
            break;
    }
}

uint16 MDFN_FASTCALL MemRead16(v810_timestamp_t &timestamp, uint32 A) {
    uint16 ret = 0;

    A &= (1 << 27) - 1;

    //if((A >> 24) <= 2)
    // printf("Read16: %d %08x\n", timestamp, A);


    switch (A >> 24) {
        case 0:
            ret = VIP_Read16(timestamp, A);
            break;

        case 1:
            break;

        case 2:
            ret = HWCTRL_Read(timestamp, A);
            break;

        case 3:
            break;

        case 4:
            break;

        case 5:
            ret = LoadU16_LE((uint16 *) &WRAM[A & 0xFFFF]);
            break;

        case 6:
            if (GPRAM)
                ret = LoadU16_LE((uint16 *) &GPRAM[A & GPRAM_Mask]);
            else printf("GPRAM(Unmapped) Read: %08x\n", A);
            break;

        case 7:
            ret = LoadU16_LE((uint16 *) &GPROM[A & GPROM_Mask]);
            break;
    }
    return (ret);
}

void MDFN_FASTCALL MemWrite16(v810_timestamp_t &timestamp, uint32 A, uint16 V) {
    A &= (1 << 27) - 1;

    //if((A >> 24) <= 2)
    // printf("Write16: %d %08x %04x\n", timestamp, A, V);

    switch (A >> 24) {
        case 0:
            VIP_Write16(timestamp, A, V);
            break;

        case 1:
            VB_VSU->Write((timestamp + VSU_CycleFix) >> 2, A, V);
            break;

        case 2:
            HWCTRL_Write(timestamp, A, V);
            break;

        case 3:
            break;

        case 4:
            break;

        case 5:
            StoreU16_LE((uint16 *) &WRAM[A & 0xFFFF], V);
            break;

        case 6:
            if (GPRAM)
                StoreU16_LE((uint16 *) &GPRAM[A & GPRAM_Mask], V);
            break;

        case 7: // ROM, no writing allowed!
            break;
    }
}

void MDFN_FASTCALL MemWrite8(v810_timestamp_t &timestamp, uint32 A, uint8 V) {
    A &= (1 << 27) - 1;

    //if((A >> 24) <= 2)
    // printf("Write8: %d %08x %02x\n", timestamp, A, V);

    switch (A >> 24) {
        case 0:
            VIP_Write8(timestamp, A, V);
            break;

        case 1:
            VB_VSU->Write((timestamp + VSU_CycleFix) >> 2, A, V);
            break;

        case 2:
            HWCTRL_Write(timestamp, A, V);
            break;

        case 3:
            break;

        case 4:
            break;

        case 5:
            WRAM[A & 0xFFFF] = V;
            break;

        case 6:
            if (GPRAM)
                GPRAM[A & GPRAM_Mask] = V;
            break;

        case 7: // ROM, no writing allowed!
            break;
    }
}

static int Load(const uint8_t *data, size_t size) {
    V810_Emu_Mode cpu_mode;

    VB_InDebugPeek = 0;

    cpu_mode = (V810_Emu_Mode) MDFN_GetSettingI("vb.cpu_emulation");

    if (size != round_up_pow2(size)) {
        puts("VB ROM image size is not a power of 2???");
        return (0);
    }

    if (size < 256) {
        puts("VB ROM image size is too small??");
        return (0);
    }

    if (size > (1 << 24)) {
        puts("VB ROM image size is too large??");
        return (0);
    }

    VB_HeaderInfo hinfo;

    /*
    ReadHeader(data, size, &hinfo);

    log_cb(RETRO_LOG_INFO, "Title:     %s\n", hinfo.game_title);
    log_cb(RETRO_LOG_INFO, "Game ID Code: %u\n", hinfo.game_code);
    log_cb(RETRO_LOG_INFO, "Manufacturer Code: %d\n", hinfo.manf_code);
    log_cb(RETRO_LOG_INFO, "Version:   %u\n", hinfo.version);

    log_cb(RETRO_LOG_INFO, "ROM:       %dKiB\n", (int)(size / 1024));

    log_cb(RETRO_LOG_INFO, "V810 Emulation Mode: %s\n", (cpu_mode == V810_EMU_MODE_ACCURATE) ? "Accurate" : "Fast");
    */

    VB_V810 = new V810();
    VB_V810->Init(cpu_mode, true);

    VB_V810->SetMemReadHandlers(MemRead8, MemRead16, NULL);
    VB_V810->SetMemWriteHandlers(MemWrite8, MemWrite16, NULL);

    VB_V810->SetIOReadHandlers(MemRead8, MemRead16, NULL);
    VB_V810->SetIOWriteHandlers(MemWrite8, MemWrite16, NULL);

    for (int i = 0; i < 256; i++) {
        VB_V810->SetMemReadBus32(i, false);
        VB_V810->SetMemWriteBus32(i, false);
    }

    std::vector<uint32> Map_Addresses;

    for (uint64 A = 0; A < 1ULL << 32; A += (1 << 27)) {
        for (uint64 sub_A = 5 << 24; sub_A < (6 << 24); sub_A += 65536) {
            Map_Addresses.push_back(A + sub_A);
        }
    }

    WRAM = VB_V810->SetFastMap(&Map_Addresses[0], 65536, Map_Addresses.size(), "WRAM");
    Map_Addresses.clear();

    // Round up the ROM size to 65536(we mirror it a little later)
    GPROM_Mask = (size < 65536) ? (65536 - 1) : (size - 1);

    for (uint64 A = 0; A < 1ULL << 32; A += (1 << 27)) {
        for (uint64 sub_A = 7 << 24; sub_A < (8 << 24); sub_A += GPROM_Mask + 1) {
            Map_Addresses.push_back(A + sub_A);
            //printf("%08x\n", (uint32)(A + sub_A));
        }
    }

    GPROM = VB_V810->SetFastMap(&Map_Addresses[0], GPROM_Mask + 1, Map_Addresses.size(), "Cart ROM");
    Map_Addresses.clear();

    // Mirror ROM images < 64KiB to 64KiB
    for (uint64 i = 0; i < 65536; i += size)
        memcpy(GPROM + i, data, size);

    GPRAM_Mask = 0xFFFF;

    for (uint64 A = 0; A < 1ULL << 32; A += (1 << 27)) {
        for (uint64 sub_A = 6 << 24; sub_A < (7 << 24); sub_A += GPRAM_Mask + 1) {
            //printf("GPRAM: %08x\n", A + sub_A);
            Map_Addresses.push_back(A + sub_A);
        }
    }
    GPRAM = VB_V810->SetFastMap(&Map_Addresses[0], GPRAM_Mask + 1, Map_Addresses.size(), "Cart RAM");
    Map_Addresses.clear();

    memset(GPRAM, 0, GPRAM_Mask + 1);

    VIP_Init();

    uint32 VB3DMode = 0;
    VIP_Set3DMode(VB3DMode, 0, 1, 0);

    VB_VSU = new VSU(&sbuf[0], &sbuf[1]);
    VBINPUT_Init();


    //VIP_Set3DMode();

    //VB3DMode = 0;
    uint32 prescale = 1;
    uint32 sbs_separation = 0;

    // MDFN_GetSettingUI("vb.3dreverse")

    //VB3DMode = MDFN_GetSettingUI("vb.3dmode");
    //uint32 prescale = MDFN_GetSettingUI("vb.liprescale");
    //uint32 sbs_separation = MDFN_GetSettingUI("vb.sidebyside.separation");

    // prescale, sbs_separation);

    // SettingChanged("vb.3dmode");
    SettingChanged("vb.disable_parallax");
    SettingChanged("vb.anaglyph.lcolor");
    SettingChanged("vb.anaglyph.rcolor");
    SettingChanged("vb.anaglyph.preset");
    SettingChanged("vb.default_color");

    SettingChanged("vb.instant_display_hack");
    SettingChanged("vb.allow_draw_skip");

    SettingChanged("vb.input.instant_read_hack");

    VIP_SetAnaglyphColors(0xFFFFFF, 0xFFFFFF);
    // VIP_SetLEDOnScale();


    MDFNGameInfo->fps = (int64) 20000000 * 65536 * 256 / (259 * 384 * 4);

    VB_Power();

    MDFNGameInfo->nominal_width = 384;
    MDFNGameInfo->nominal_height = 224;
    MDFNGameInfo->fb_width = 384;
    MDFNGameInfo->fb_height = 224 * 2 + 12;

    switch (VB3DMode) {
        default:
            break;

        case VB3DMODE_VLI:
            MDFNGameInfo->nominal_width = 768 * prescale;
            MDFNGameInfo->nominal_height = 224;
            MDFNGameInfo->fb_width = 768 * prescale;
            MDFNGameInfo->fb_height = 224;
            break;

        case VB3DMODE_HLI:
            MDFNGameInfo->nominal_width = 384;
            MDFNGameInfo->nominal_height = 448 * prescale;
            MDFNGameInfo->fb_width = 384;
            MDFNGameInfo->fb_height = 448 * prescale;
            break;

        case VB3DMODE_CSCOPE:
            MDFNGameInfo->nominal_width = 512;
            MDFNGameInfo->nominal_height = 384;
            MDFNGameInfo->fb_width = 512;
            MDFNGameInfo->fb_height = 384;
            break;

        case VB3DMODE_SIDEBYSIDE:
            MDFNGameInfo->nominal_width = 384 * 2 + sbs_separation;
            MDFNGameInfo->nominal_height = 224;
            MDFNGameInfo->fb_width = 384 * 2 + sbs_separation;
            MDFNGameInfo->fb_height = 224;
            break;
    }

    MDFNGameInfo->lcm_width = MDFNGameInfo->fb_width;
    MDFNGameInfo->lcm_height = MDFNGameInfo->fb_height;

    MDFNMP_Init(32768, ((uint64) 1 << 27) / 32768);
    MDFNMP_AddRAM(65536, 5 << 24, WRAM);
    if ((GPRAM_Mask + 1) >= 32768)
        MDFNMP_AddRAM(GPRAM_Mask + 1, 6 << 24, GPRAM);

    return (1);
}

static MDFNGI *MDFNI_LoadGame(const uint8_t *data, size_t size) {
    MDFNGameInfo = &EmulatedVB;

    // Load per-game settings
    // Maybe we should make a "pgcfg" subdir, and automatically load all files in it?
    // End load per-game settings
    //

    if (Load(data, size) <= 0)
        goto error;

    MDFN_LoadGameCheats(NULL);
    MDFNMP_InstallReadPatches();

    return (MDFNGameInfo);

    error:
    MDFNGameInfo = NULL;
    return NULL;
}

namespace VRVB {

    uint16_t input_buf[MAX_PLAYERS];

    void Init() {
        VBINPUT_SetInput(0, "gamepad", &input_buf[0]);

        //VIP_Set3DMode(2, false, 1, 0);
    }

    void Reset() {
        VB_Power();
    }

    void LoadRom(const uint8_t *data, size_t size) {
        overscan = false;

        setting_vb_anaglyph_preset = 0;

        game = MDFNI_LoadGame(data, size);
        if (!game)
            return;

        MDFN_PixelFormat pix_fmt(MDFN_COLORSPACE_RGB, 0, 8, 16, 24);
        memset(&last_pixel_format, 0, sizeof(MDFN_PixelFormat));

        surf = new MDFN_Surface(NULL, FB_WIDTH, FB_HEIGHT, FB_WIDTH, pix_fmt);
    }

    void UpdateInput() {

    }

    static void CloseGame(void) {
        //VIP_Kill();

        if (VB_VSU) {
            delete VB_VSU;
            VB_VSU = NULL;
        }

        /*
        if(GPRAM)
        {
         MDFN_free(GPRAM);
         GPRAM = NULL;
        }

        if(GPROM)
        {
         MDFN_free(GPROM);
         GPROM = NULL;
        }
        */

        if (VB_V810) {
            VB_V810->Kill();
            delete VB_V810;
            VB_V810 = NULL;
        }
    }

    static void MDFNI_CloseGame(void) {
      if (!MDFNGameInfo)
        return;

      MDFN_FlushGameCheats(0);

      CloseGame();

      MDFNMP_Kill();

      MDFNGameInfo = NULL;
    }

    void unload_game() {
      if (!game)
        return;

      MDFNI_CloseGame();
    }

    void Run() {
        MDFNGI *curgame = game;

        UpdateInput();

        static int16_t sound_buf[0x10000];
        static MDFN_Rect rects[FB_MAX_HEIGHT];
        rects[0].w = ~0;

        EmulateSpecStruct spec = {0};
        spec.surface = surf;
        spec.SoundRate = 44100;
        spec.SoundBuf = sound_buf;
        spec.LineWidths = rects;
        spec.SoundBufMaxSize = sizeof(sound_buf) / 2;
        spec.SoundVolume = 1.0;
        spec.soundmultiplier = 1.0;
        spec.SoundBufSize = 0;
        spec.VideoFormatChanged = false;
        spec.SoundFormatChanged = false;

        if (memcmp(&last_pixel_format, &spec.surface->format, sizeof(MDFN_PixelFormat))) {
            spec.VideoFormatChanged = true;

            last_pixel_format = spec.surface->format;
        }

        if (spec.SoundRate != last_sound_rate) {
            spec.SoundFormatChanged = true;
            last_sound_rate = spec.SoundRate;
        }

        Emulate(&spec);

        int16 *const SoundBuf = spec.SoundBuf + spec.SoundBufSizeALMS * curgame->soundchan;
        int32 SoundBufSize = spec.SoundBufSize - spec.SoundBufSizeALMS;
        const int32 SoundBufMaxSize = spec.SoundBufMaxSize - spec.SoundBufSizeALMS;

        spec.SoundBufSize = spec.SoundBufSizeALMS + SoundBufSize;

        int32 width = spec.DisplayRect.w;
        int32 height = spec.DisplayRect.h;

        // 32bpp
        const void *pix = surf->pixels8;
        // video_cb(pix, width, height, FB_WIDTH << 2);

        VRVB::audio_cb(spec.SoundBuf, spec.SoundBufSize);

        VRVB::video_cb(pix, width, height);

        // audio
        // spec.SoundBuf, spec.SoundBufSize
    }

    size_t save_ram_size() {
        return GPRAM_Mask + 1;
    }

    void *save_ram() {
        return GPRAM;
    }

    void load_ram(const void *data, size_t size) {
        memcpy(GPRAM, data, size);
    }

    size_t retro_serialize_size() {
        StateMem st;

        st.data = NULL;
        st.loc = 0;
        st.len = 0;
        st.malloced = 0;
        st.initial_malloc = 0;

        if (!MDFNSS_SaveSM(&st, 0, 0, NULL, NULL, NULL))
            return 0;

        free(st.data);
        return st.len;
    }

    bool retro_serialize(void *data, size_t size) {
        StateMem st;
        bool ret = false;
        uint8_t *_dat = (uint8_t *) malloc(size);

        if (!_dat)
            return false;

        /* Mednafen can realloc the buffer so we need to ensure this is safe. */
        st.data = _dat;
        st.loc = 0;
        st.len = 0;
        st.malloced = size;
        st.initial_malloc = 0;

        ret = MDFNSS_SaveSM(&st, 0, 0, NULL, NULL, NULL);

        memcpy(data, st.data, size);
        free(st.data);

        return ret;
    }

    bool retro_unserialize(const void *data, size_t size) {
        StateMem st;
        st.data = (uint8_t *) data;
        st.loc = 0;
        st.len = size;
        st.malloced = 0;
        st.initial_malloc = 0;

        return MDFNSS_LoadSM(&st, 0, 0);
    }

}