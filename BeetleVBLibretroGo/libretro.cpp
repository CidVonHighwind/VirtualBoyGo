#include <stdarg.h>

#include <libretro.h>

#include "mednafen/mednafen.h"
#include "mednafen/mempatcher.h"
#include "mednafen/git.h"

static MDFNGI *game;

struct retro_perf_callback perf_cb;
retro_get_cpu_features_t perf_get_cpu_features_cb = NULL;
retro_log_printf_t log_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

static bool overscan;
static double last_sound_rate;
static MDFN_PixelFormat last_pixel_format;

static MDFN_Surface *surf;

/* Mednafen - Multi-system Emulator
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "mednafen/vb/vb.h"
#include "mednafen/vb/timer.h"
#include "mednafen/vb/vsu.h"
#include "mednafen/vb/vip.h"
#ifdef WANT_DEBUGGER
#include "mednafen/vb/debug.h"
#endif
#include "mednafen/vb/input.h"
#include "mednafen/mempatcher.h"
#if 0
#include <iconv.h>
#endif

enum
{
 ANAGLYPH_PRESET_DISABLED = 0,
 ANAGLYPH_PRESET_RED_BLUE,
 ANAGLYPH_PRESET_RED_CYAN,
 ANAGLYPH_PRESET_RED_ELECTRICCYAN,
 ANAGLYPH_PRESET_RED_GREEN,
 ANAGLYPH_PRESET_GREEN_MAGENTA,
 ANAGLYPH_PRESET_YELLOW_BLUE,
};

static const uint32 AnaglyphPreset_Colors[][2] =
{
 { 0, 0 },
 { 0xFF0000, 0x0000FF },
 { 0xFF0000, 0x00B7EB },
 { 0xFF0000, 0x00FFFF },
 { 0xFF0000, 0x00FF00 },
 { 0x00FF00, 0xFF00FF },
 { 0xFFFF00, 0x0000FF },
};

#define STICK_DEADZONE 0x4000
#define RIGHT_DPAD_LEFT 0x1000
#define RIGHT_DPAD_RIGHT 0x0020
#define RIGHT_DPAD_UP 0x0010
#define RIGHT_DPAD_DOWN 0x2000

int32 VB_InDebugPeek;

static uint32 VB3DMode;

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

static INLINE void RecalcIntLevel(void)
{
 int ilevel = -1;

 for(int i = 4; i >= 0; i--)
 {
  if(IRQ_Asserted & (1 << i))
  {
   ilevel = i;
   break;
  }
 }

 VB_V810->SetInt(ilevel);
}

void VBIRQ_Assert(int source, bool assert)
{
 assert(source >= 0 && source <= 4);

 IRQ_Asserted &= ~(1 << source);

 if(assert)
  IRQ_Asserted |= 1 << source;
 
 RecalcIntLevel();
}

static uint8 HWCTRL_Read(v810_timestamp_t &timestamp, uint32 A)
{
 uint8 ret = 0;

 if(A & 0x3)
 { 
  puts("HWCtrl Bogus Read?");
  return(ret);
 }

 switch(A & 0xFF)
 {
  default: printf("Unknown HWCTRL Read: %08x\n", A);
	   break;

  case 0x18:
  case 0x1C:
  case 0x20: ret = TIMER_Read(timestamp, A);
	     break;

  case 0x24: ret = WCR | 0xFC;
	     break;

  case 0x10:
  case 0x14:
  case 0x28: ret = VBINPUT_Read(timestamp, A);
             break;

 }

 return(ret);
}

static void HWCTRL_Write(v810_timestamp_t &timestamp, uint32 A, uint8 V)
{
 if(A & 0x3)
 {
  puts("HWCtrl Bogus Write?");
  return;
 }

 switch(A & 0xFF)
 {
  default: printf("Unknown HWCTRL Write: %08x %02x\n", A, V);
           break;

  case 0x18:
  case 0x1C:
  case 0x20: TIMER_Write(timestamp, A, V);
	     break;

  case 0x24: WCR = V & 0x3;
	     break;

  case 0x10:
  case 0x14:
  case 0x28: VBINPUT_Write(timestamp, A, V);
	     break;
 }
}

uint8 MDFN_FASTCALL MemRead8(v810_timestamp_t &timestamp, uint32 A)
{
 uint8 ret = 0;
 A &= (1 << 27) - 1;

 //if((A >> 24) <= 2)
 // printf("Read8: %d %08x\n", timestamp, A);

 switch(A >> 24)
 {
  case 0: ret = VIP_Read8(timestamp, A);
	  break;

  case 1: break;

  case 2: ret = HWCTRL_Read(timestamp, A);
	  break;

  case 3: break;
  case 4: break;

  case 5: ret = WRAM[A & 0xFFFF];
	  break;

  case 6: if(GPRAM)
	   ret = GPRAM[A & GPRAM_Mask];
	  else
	   printf("GPRAM(Unmapped) Read: %08x\n", A);
	  break;

  case 7: ret = GPROM[A & GPROM_Mask];
	  break;
 }
 return(ret);
}

uint16 MDFN_FASTCALL MemRead16(v810_timestamp_t &timestamp, uint32 A)
{
 uint16 ret = 0;

 A &= (1 << 27) - 1;

 //if((A >> 24) <= 2)
 // printf("Read16: %d %08x\n", timestamp, A);


 switch(A >> 24)
 {
  case 0: ret = VIP_Read16(timestamp, A);
	  break;

  case 1: break;

  case 2: ret = HWCTRL_Read(timestamp, A);
	  break;

  case 3: break;

  case 4: break;

  case 5: ret = LoadU16_LE((uint16 *)&WRAM[A & 0xFFFF]);
	  break;

  case 6: if(GPRAM)
           ret = LoadU16_LE((uint16 *)&GPRAM[A & GPRAM_Mask]);
	  else printf("GPRAM(Unmapped) Read: %08x\n", A);
	  break;

  case 7: ret = LoadU16_LE((uint16 *)&GPROM[A & GPROM_Mask]);
	  break;
 }
 return(ret);
}

void MDFN_FASTCALL MemWrite8(v810_timestamp_t &timestamp, uint32 A, uint8 V)
{
 A &= (1 << 27) - 1;

 //if((A >> 24) <= 2)
 // printf("Write8: %d %08x %02x\n", timestamp, A, V);

 switch(A >> 24)
 {
  case 0: VIP_Write8(timestamp, A, V);
          break;

  case 1: VB_VSU->Write((timestamp + VSU_CycleFix) >> 2, A, V);
          break;

  case 2: HWCTRL_Write(timestamp, A, V);
          break;

  case 3: break;

  case 4: break;

  case 5: WRAM[A & 0xFFFF] = V;
          break;

  case 6: if(GPRAM)
           GPRAM[A & GPRAM_Mask] = V;
          break;

  case 7: // ROM, no writing allowed!
          break;
 }
}

void MDFN_FASTCALL MemWrite16(v810_timestamp_t &timestamp, uint32 A, uint16 V)
{
 A &= (1 << 27) - 1;

 //if((A >> 24) <= 2)
 // printf("Write16: %d %08x %04x\n", timestamp, A, V);

 switch(A >> 24)
 {
  case 0: VIP_Write16(timestamp, A, V);
          break;

  case 1: VB_VSU->Write((timestamp + VSU_CycleFix) >> 2, A, V);
          break;

  case 2: HWCTRL_Write(timestamp, A, V);
          break;

  case 3: break;

  case 4: break;

  case 5: StoreU16_LE((uint16 *)&WRAM[A & 0xFFFF], V);
          break;

  case 6: if(GPRAM)
           StoreU16_LE((uint16 *)&GPRAM[A & GPRAM_Mask], V);
          break;

  case 7: // ROM, no writing allowed!
          break;
 }
}

static void FixNonEvents(void)
{
 if(next_vip_ts & 0x40000000)
  next_vip_ts = VB_EVENT_NONONO;

 if(next_timer_ts & 0x40000000)
  next_timer_ts = VB_EVENT_NONONO;

 if(next_input_ts & 0x40000000)
  next_input_ts = VB_EVENT_NONONO;
}

static void EventReset(void)
{
 next_vip_ts = VB_EVENT_NONONO;
 next_timer_ts = VB_EVENT_NONONO;
 next_input_ts = VB_EVENT_NONONO;
}

static INLINE int32 CalcNextTS(void)
{
 int32 next_timestamp = next_vip_ts;

 if(next_timestamp > next_timer_ts)
  next_timestamp  = next_timer_ts;

 if(next_timestamp > next_input_ts)
  next_timestamp  = next_input_ts;

 return(next_timestamp);
}

static void RebaseTS(const v810_timestamp_t timestamp)
{
 //printf("Rebase: %08x %08x %08x\n", timestamp, next_vip_ts, next_timer_ts);

 assert(next_vip_ts > timestamp);
 assert(next_timer_ts > timestamp);
 assert(next_input_ts > timestamp);

 next_vip_ts -= timestamp;
 next_timer_ts -= timestamp;
 next_input_ts -= timestamp;
}

void VB_SetEvent(const int type, const v810_timestamp_t next_timestamp)
{
 //assert(next_timestamp > VB_V810->v810_timestamp);

 if(type == VB_EVENT_VIP)
  next_vip_ts = next_timestamp;
 else if(type == VB_EVENT_TIMER)
  next_timer_ts = next_timestamp;
 else if(type == VB_EVENT_INPUT)
  next_input_ts = next_timestamp;

 if(next_timestamp < VB_V810->GetEventNT())
  VB_V810->SetEventNT(next_timestamp);
}


static int32 MDFN_FASTCALL EventHandler(const v810_timestamp_t timestamp)
{
 if(timestamp >= next_vip_ts)
  next_vip_ts = VIP_Update(timestamp);

 if(timestamp >= next_timer_ts)
  next_timer_ts = TIMER_Update(timestamp);

 if(timestamp >= next_input_ts)
  next_input_ts = VBINPUT_Update(timestamp);

 return(CalcNextTS());
}

// Called externally from debug.cpp in some cases.
void ForceEventUpdates(const v810_timestamp_t timestamp)
{
 next_vip_ts = VIP_Update(timestamp);
 next_timer_ts = TIMER_Update(timestamp);
 next_input_ts = VBINPUT_Update(timestamp);

 VB_V810->SetEventNT(CalcNextTS());
 //printf("FEU: %d %d %d\n", next_vip_ts, next_timer_ts, next_input_ts);
}

static void VB_Power(void)
{
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


 ForceEventUpdates(0);	//VB_V810->v810_timestamp);
}

static void SettingChanged(const char *name)
{
 if(!strcasecmp(name, "vb.3dmode"))
 {
  // FIXME, TODO (complicated)
  //VB3DMode = MDFN_GetSettingUI("vb.3dmode");
  //VIP_Set3DMode(VB3DMode);
 }
 else if(!strcasecmp(name, "vb.disable_parallax"))
 {
  VIP_SetParallaxDisable(MDFN_GetSettingB("vb.disable_parallax"));
 }
 else if(!strcasecmp(name, "vb.anaglyph.lcolor") || !strcasecmp(name, "vb.anaglyph.rcolor") ||
	!strcasecmp(name, "vb.anaglyph.preset") || !strcasecmp(name, "vb.default_color"))

 {
  uint32 lcolor = MDFN_GetSettingUI("vb.anaglyph.lcolor"), rcolor = MDFN_GetSettingUI("vb.anaglyph.rcolor");
  int preset = MDFN_GetSettingI("vb.anaglyph.preset");

  if(preset != ANAGLYPH_PRESET_DISABLED)
  {
   lcolor = AnaglyphPreset_Colors[preset][0];
   rcolor = AnaglyphPreset_Colors[preset][1];
  }
  VIP_SetAnaglyphColors(lcolor, rcolor);
  VIP_SetDefaultColor(MDFN_GetSettingUI("vb.default_color"));
 }
 else if(!strcasecmp(name, "vb.input.instant_read_hack"))
 {
  VBINPUT_SetInstantReadHack(MDFN_GetSettingB("vb.input.instant_read_hack"));
 }
 else if(!strcasecmp(name, "vb.instant_display_hack"))
  VIP_SetInstantDisplayHack(MDFN_GetSettingB("vb.instant_display_hack"));
 else if(!strcasecmp(name, "vb.allow_draw_skip"))
  VIP_SetAllowDrawSkip(MDFN_GetSettingB("vb.allow_draw_skip"));
 else
  abort();


}

struct VB_HeaderInfo
{
 char game_title[256];
 uint32 game_code;
 uint16 manf_code;
 uint8 version;
};

static void ReadHeader(const uint8_t *data, size_t size, VB_HeaderInfo *hi)
{
#if 0
 iconv_t sjis_ict = iconv_open("UTF-8", "shift_jis");

 if(sjis_ict != (iconv_t)-1)
 {
  char *in_ptr, *out_ptr;
  size_t ibl, obl;

  ibl = 20;
  obl = sizeof(hi->game_title) - 1;

  in_ptr = (char*)data + (0xFFFFFDE0 & (size - 1));
  out_ptr = hi->game_title;

  iconv(sjis_ict, (ICONV_CONST char **)&in_ptr, &ibl, &out_ptr, &obl);
  iconv_close(sjis_ict);

  *out_ptr = 0;

  MDFN_RemoveControlChars(hi->game_title);
  MDFN_trim(hi->game_title);
 }
 else
  hi->game_title[0] = 0;

 hi->game_code = MDFN_de32lsb(data + (0xFFFFFDFB & (size - 1)));
 hi->manf_code = MDFN_de16lsb(data + (0xFFFFFDF9 & (size - 1)));
 hi->version = data[0xFFFFFDFF & (size - 1)];
#endif
}

struct VBGameEntry
{
 uint32 checksums[16];
 const char *title;
 uint32 patch_address[512];
};

static const struct VBGameEntry VBGames[] =
{
 { { 0xbb71b522 } , "3D Tetris (US)", { 0xFFF1E740,
       0xfff1e75a,
       0xfff5c958,
       0xfff5c9a4,
       0xfff5c9b6,
       0xfff677a6,
       0xfff677f0,
       0xfff6c72a,
       0xfffeffc2,
       0xfff6df22,
       0xfff6e01a,
       0xfff6e20a,
       0xfff6e302,
       0xfff6e5e4,
       0xfff6eb34,
       0xfff6eb8a,
       0xfff00cd0,
       0xfffd9508,
       0xfffdad90,
       0xfffd9f1c,
       0xfffca7a2,
       0xfffca986,
       0xfffcaad4,
       0xfffcacb0,
       0xfff3dbc6,
       0xfff3dc58,
       0xfff3ca1a,
       0xfff3effe,
       0xfff3f06c,
       0xfff3f122,
       0xfff3f2da,
       0xfff3c9d8,
       0xfff01892,
       0xfff017f4,
       0xfff016c8,
       0xfff4677c,
       0xfff4620a,
       0xfff3503e,
       0xfff3f97a,
       0xfff3fae0,
       0xfff01270,
       0xfff473c8,
       0xfff472dc,
       0xfff0160a,
       0xfff6e112,
       0xfff6e7d2,
       0xfffc1730,
       0xfff3f1e6,
       0xfffc22da,
       0xfffc20a2,
       0xfffc2378,
       0xfff36152,
       0xfff37120,
       0xfff378b4,
       0xfffc1b44,
       0xfffc1b8e,
       0xfff3f258,
       0xfff3655c,
       0xfffd9902,
       0xfff49b60,
       0xfff86ef0,
       0xfff3cf08,
       0xfff5894c,
       0xfff3cec2,
       0xfff5a73e,
       0xfff400ce,
       0xfff3c2ec,
       0xfff5b5c2,
       0xfff5b64a,
       0xfffd2968,
       0xfffd2ca0,
       0xfffd2be0,
       0xfffd2b40,
       0xfffd2d10,
       0xfffd2d26,
       0xfff5a6a0,
       0xfff5a564,
       0xfffd9800,
       0xfffd9a04,
       0xfffd9b30,
       0xfffcd8d0,
       0xfffcd830,
       0xfffc15a4,
       0xfff4b5f2,
       0xfff3f346,
       0xfff374ae,
       0xfff3fa42,
       0xfff3720c,
       0xfff38298,
       0xfff38370,
       0xfff2b8a2,
       0xfff2bf52,
       0xfff0199a,
       0xfff01dc0,
       0xfffd169e,
       0xfffd19d6,
       0xfffd1876,
       0xfffd1a46,
       0xfffd1a5c,
       0xfff676b6,
       0xfff675fc,
       0xfff37b48,
       0xfffc246e,
       0xfffc24b8,
       0xfff365e8,
       0xfff2dfa0,
       0xfff36674,
       0xfff36700,
       0xfff598f4,
       0xfff59992,
       0xfff59c2a,
       0xfff59c74,
       0xfff4709a,
       0xfff46eec,
       0xfff4714c,
       0xfff4b808,
       0xfff3f3b2,
       0xfffdaeda,
       0xfffdb044,
       0xfffdae36,
       0xfff4b698,
       0xfff4b73c,
       0xfff49c06,
       0xfffdb322,
       0xfffdb18c,
       0xfff4ba68,
       0xfff4b8d2,
       0xfff474aa,
       0xfffd1916,
       0xfff3d0ac,
       0xfff477ac,
       0xfff3d2ca,
       0xfff476c0,
       0xfff5a41e,
       0xfff5a04c,
       0xfff4788e,
       0xfffc1392,
       0xfffc14f8,
 } },

 { { 0xe81a3703 }, "Bound High!", { 0x0703181A , 0x070229BE, 0x07002CA6,
       0x07024f20,
       0x070317ae,
       0x07030986,
       0x070309ca,
       0x07031968,
       0x07030ad2,
       0x07030b16,
       0x070319be,
       0x07031a20,
       0x070296b4,
       0x0702984e,
       0x07029888,
       0x070298d0,
       0x07029910,
       0x070299ce,
       0x07030c3a,
       0x07030dfc,
       0x07030e52,
       0x07030eb4,
       0x070250fa,
       0x07025148,
       0x07025776,
       0x070257c6,
       0x07025828,
       0x07021dc0,
       0x07021e50,
       0x070008e6,
       0x0700155c,
       0x070005f4,
       0x07022a08,
       0x07031e2e,
       0x07032308,
       0x0703234a,
       0x070323ac,
       0x0703a27a,
       0x0703a754,
       0x0703a77c,
       0x0703a7b4,
       0x0703a816,
       0x07002104,
       0x070028e2,
       0x0700299e,
       0x070029d8,
       0x07001782,
       0x07001912,
       0x070024da,
       0x070347e6,
       0x07035ec4,
       0x07035f16,
       0x07035f78,
       0x0702d152,
       0x0702d19a,
       0x0702d548,
       0x0702d5cc,
       0x07001c30,
       0x0702e97e,
       0x0702e9bc,
       0x070009b2,
       0x07030422,
       0x070305fc,
       0x0703064c,
       0x070306ae,
       0x07026c80,
       0x07027618,
       0x07027656,
       0x07001a92,
       0x0702eff8,
       0x0702f4e6,
       0x0702f54e,
       0x0702f5b0,
       0x07025df6,
       0x07025fb2,
       0x07026036,
       0x0702e01a,
       0x0702e090,
       0x0702e114,
       0x0702e8ea,
       0x07001440,
       0x0702c46e,
       0x0702c4b6,
       0x0702c9e4,
       0x0702ca6e,
       0x0702a532,
       0x0702a5bc,
       0x0702aba0,
       0x0702b4ce,
       0x0702b536,
 } },

 { { 0xc9710a36} , "Galactic Pinball", { 0xFFF4018A,
       0xfff40114,
       0xfff51886,
       0xfff51abe,
       0xfff4b704,
       0xfff4ec7a,
       0xfff53708,
       0xfff53a3c,
 }},

 { { 0x2199af41 }, "Golf (US)", { 0xFFE0141A, 0xFFE097C4, 0xFFE1F47A, 0xFFE0027E, 0xFFE05CA0, 0xFFE014A0,
       0xffe00286,
       0xffe013d0,
       0xffe013f0,
       0xffe01482,
       0xffe01004,
       0xffe01024,
       0xffe00d30,
       0xffe00d50,
       0xffe00db4,
       0xffe00dd4,
       0xffe13efe,
       0xffe13f62,
       0xffe13fd2,
       0xffe01744,
       0xffe01764,
       0xffe05c90,
       0xffe017b2,
       0xffe017d2,
       0xffe01930,
       0xffe01950,
       0xffe01c4a,
       0xffe01c6a,
       0xffe01cb8,
       0xffe01cd8,
       0xffe01dce,
       0xffe01de8,
       0xffe01e5a,
       0xffe01e7a,
       0xffe1409e,
       0xffe02450,
       0xffe02470,
       0xffe024b0,
       0xffe024d0,
       0xffe02530,
       0xffe02550,
       0xffe0257e,
       0xffe0259e,
       0xffe1429c,
       0xffe027c4,
       0xffe027e4,
       0xffe13f2a,
       0xffe097a4,
       0xffe181bc,
       0xffe1ce40,
       0xffe1ce60,
       0xffe07e80,
       0xffe07ea0,
       0xffe07ec0,
       0xffe07ee0,
       0xffe0810e,
       0xffe08158,
       0xffe1e468,
       0xffe1e488,
       0xffe2198c,
       0xffe09a96,
       0xffe09ab6,
       0xffe1f7be,
       0xffe1f7de,
       0xffe1f8ae,
       0xffe1f9d6,
       0xffe1fa18,
       0xffe1fa38,
       0xffe05c68,
       0xffe05c78,
       0xffe14344,
       0xffe09e06,
       0xffe16d8a,
       0xffe16daa,
       0xffe21712,
       0xffe1f37a,
       0xffe1f39a,
       0xffe1f624,
       0xffe1f666,
       0xffe1f686,
       0xffe141f8,
       0xffe151d2,
       0xffe17622,
 } },

 { { 0xefd0ac36 }, "Innsmouth House (Japan)", { 0x070082F4,
       0x07017546,
       0x07016f9a,
       0x07017558,
       0x07016fac,
       0x07017692,
       0x070176a4,
 }},

 { { 0xa44de03c, 0x81af4d6d /*[b1]*/,  0xb15d706e /*(Enable Cheats Hack)*/, 0x79a99f3c /*[T+Ger1.0_KR155E]*/ }, "Jack Bros (US)", {  0x070001A6 }},
 { { 0xcab61e8b }, "Jack Bros (Japan)", {
       0x07000192,
 } },

 // Is 0xbf0d0ab0 the bad dump?
 { { 0xa47de78c, 0xbf0d0ab0 }, "Mario Clash", { 0xFFFF5BB4, 0xFFFE6FF0, 0xFFFE039E,
       0xfffdd786,
       0xfffdd76c,
       0xffff5bc6,
       0xffff73a4,
       0xffff73b6,
       0xfffdd836,
       0xfffdd848,
       0xffff916c,
       0xffff917e,
       0xfffe7002,
       0xfffe7c36,
       0xfffe7c48,
       0xffff44f4,
       0xffff4506,
       0xfffe03b0,
       0xffff2514,
       0xffff2526,
       0xffff9be2,
       0xffff9bf4,
 } },

 { { 0x7ce7460d }, "Mario's Tennis", { 0xFFF8017C, 0xFFF90FEA, 0xFFF86DFA, 0xFFF873D6, 0xFFF9301C,
       0xfff801a2,
       0xfff90f98,
       0xfff90fae,
       0xfff90fc8,
       0xfff914c4,
       0xfff9150e,
       0xfff82a00,
       0xfff82a1a,
       0xfff82a38,
       0xfff86b98,
       0xfff86e66,
       0xfff871a2,
       0xfff8d9b6,
       0xfff8da5a,
       0xfff8dcf8,
       0xfff87aa8,
       0xfff93c86,
       0xfff93ca8,
       0xfff93ce0,
       0xfff94338,
       0xfff93158,
 } },

 { { 0xdf4d56b4 }, "Nester's Funky Bowling (US)", { 0xFFE00B78, 0xFFE00D82, 0xFFE0105A, 0xFFE00FCE,
       0xffe00f6a,
       0xffe00f72,
       0xffe00ddc,
       0xffe0089e,
       0xffe00fba,
       0xffe03702,
       0xffe01064,
       0xffe0c024,
       0xffe00bba,
       0xffe0d86c,
       0xffe0d51c,
 } },

 { { 0x19bb2dfb, 0x25fb89bb /*[b1]*/, 0x21d224af /*[h1]*/, 0xc767fe4b /*[h2]*/ }, "Panic Bomber (US)", { 0x07001FE8,
       0x07001f34,
       0x07001fc6,
 } },

 // Japan and US versions probably use the same code.
 { { 0xaa10a7b4, 0x7e85c45d }, "Red Alarm", { 0x202CE, 0x070202B4,
       0x070202e2,
       0x07020642,
       0x0702074c,
       0x07001f50,
       0x0703ca0a,
       0x07045c9e,
       0x0703326a,
       0x07031aae,
       0x070328a4,
       0x07032e5e,
       0x0703748e,
       0x07035868,
       0x07035948,
       0x07031c88,
       0x0703786a,
       0x0703628e,
       0x07035b20,
       0x07035bc0,
       0x07033856,
       0x07037370,
       0x07031a2e,
       0x070373c4,
       0x0703cfd8,
       0x0703d03c,
       0x0703d0a4,
       0x0702eae2,
       0x0703ce00,
       0x07044f02,
       0x07041922,
       0x07041df0,
       0x0701ba38,
       0x07045176,
       0x07044f8c,
       0x0703c926,
       0x0703c940,
       0x0703c712,
       0x0703c73c,
       0x07023158,
       0x070231d6,
       0x0702320c,
       0x07020a60,
       0x0700489c,
       0x07032174,
       0x070324ca,
       0x0702ef56,
       0x070414b0,
       0x0702052e,
       0x070321a6,
       0x070321fe,
       0x07032224,
       0x07021866,
       0x07021aa4,
       0x07021b36,
       0x07021b5e,
       0x07021b88,
       0x0703c6ba,
       0x0703cb3c,
       0x0703c67a,
       0x070412d2,
       0x0704132a,
       0x07041428,
       0x07020d2e,
       0x07020d48,
       0x0703755a,
       0x070375ac,
       0x070375da,
       0x07020aaa,
       0x0701ba8a,
       0x07041b0c,
       0x070355d8,
       0x07051ce0,
       0x070049e0,
       0x070049fa,
       0x07004a94,
       0x07020fb8,
       0x07021046,
       0x070210dc,
       0x07004bc2,
       0x07004ac0,
       0x07041640,
       0x07041698,
       0x07041744,
       0x07020eb0,
       0x070208bc,
       0x07020db6,
       0x07020e40,
       0x07004b28,
       0x070417c0,
       0x07041d3c,
       0x07035f18,

       0x07022bd2,
       0x07020504,
       0x070419a4,
       0x07041a78,
 } },

 { { 0x44788197 }, "SD Gundam Dimension War (Japan)", {
       0xfff296de,
       0xfff2970a,
       0xfff29730,
       0xfff298b0,
       0xfff298fa,
       0xfff29aac,
       0xfff29af2,
       0xfff29b1a,
       0xfff065ba,
       0xfff29b56,
       0xfff29cd6,
       0xfff4b7aa,
       0xfff4b8e4,
       0xfff4b9e2,
       0xfff4ba2e,
       0xfff4bbba,
       0xfff2824c,
       0xfff2835a,
       0xfff2848e,
       0xfff28aa4,
       0xfff28b20,
       0xfff28d66,
       0xfff28e5c,
       0xfff28e76,
       0xfff065e6,
       0xfff29504,
       0xfff5869e,
       0xfff58134,
       0xfff563e4,
       0xfff5687e,
       0xfff19686,
       0xfff196e6,
       0xfff19726,
       0xfff1974e,
       0xfff1a3a2,
       0xfff1083e,
       0xfff140b2,
       0xfff1372c,
       0xfff13c0e,
       0xfff13c70,
       0xfff166f2,
       0xfff18010,
       0xfff18070,
       0xfff18604,
       0xfff10654,
       0xfff44a7a,
       0xfff44a90,
       0xfff44ab4,
       0xfff44af8,
       0xfff4bd2e,
       0xfff4517a,
       0xfff439e6,
       0xfff43e92,
       0xfff43ea8,
       0xfff43ecc,
       0xfff43ef8,
       0xfff442ee,
       0xfff444c8,
       0xfff1995c,
       0xfff191ce,
       0xfff264d4,
       0xfff267ae,
       0xfff489aa,
       0xfff489ee,
       0xfff48a32,
       0xfff48a76,
       0xfff48aba,
       0xfff48ccc,
       0xfff48dae,
       0xfff493f2,
       0xfff49422,
       0xfff494c0,
       0xfff49620,
       0xfff4a822,
       0xfff4a940,
       0xfff4aa46,
       0xfff4ab2a,
       0xfff1b3b6,
       0xfff1b3fc,
       0xfff1b456,
       0xfff1b496,
       0xfff1b4d6,
       0xfff1b516,
       0xfff1b556,
       0xfff1bc6e,
       0xfff1bc94,
       0xfff1bcaa,
       0xfff1bcce,
       0xfff1bcf6,
       0xfff1bd0c,
       0xfff1bd48,
       0xfff1bdca,
       0xfff1be2a,
       0xfff1bed6,
       0xfff26c88,
       0xfff4ac00,
       0xfff4ad68,
       0xfff4b0f8,
       0xfff26a70,
       0xfff26b18,
       0xfff1bf5e,
       0xfff26dbe,
       0xfff26e54,
       0xfff441c4,
       0xfff2d548,
       0xfff2d630,
       0xfff2d67c,
       0xfff2d6c0,
       0xfff2d750,
       0xfff2d7de,
       0xfff2d872,
       0xfff2d90e,
       0xfff2d9bc,
       0xfff2da64,
       0xfff2daec,
       0xfff2db74,
       0xfff2dc1c,
       0xfff2dc94,
       0xfff2dd32,
       0xfff2ddf4,
       0xfff2df4c,
       0xfff3017a,
       0xfff3091c,
       0xfff271f2,
       0xfff272a0,
       0xfff4be52,
       0xfff4bf06,
       0xfff4bf36,
       0xfff4bf5e,
       0xfff4c4fe,
       0xfff19852,
       0xfff44b38,
       0xfff44e4a,
       0xfff44f4c,
       0xfff0edc0,
       0xfff0f142,
 } },

 { { 0xfa44402d }, "Space Invaders VC (Japan)", {
       0xfff80154,
       0xfff87e04,
       0xfff87e18,
 } },

 // Is 0xc2211fcc a bad dump?
 { {0x60895693, 0xc2211fcc, 0x7cb69b3a /*[T+Eng]*/ }, "Space Squash (Japan)", {
       0x0701a97e,
 } },

 { { 0x6ba07915, 0x41fb63bf /*[b1]*/ }, "T&E Virtual Golf (Japan)", {
       0x0700027e,
       0x07000286,
       0x070013d0,
       0x070013f0,
       0x0700141a,
       0x07001004,
       0x07001024,
       0x07000d30,
       0x07000d50,
       0x07000db4,
       0x07000dd4,
       0x07013c4e,
       0x07013cb2,
       0x07013d22,
       0x07001718,
       0x07001738,
       0x07001a32,
       0x07001a52,
       0x07005988,
       0x07005998,
       0x07001aa0,
       0x07001ac0,
       0x07001bb6,
       0x07001bd0,
       0x07001c36,
       0x07001c56,
       0x07013dee,
       0x0700223c,
       0x0700225c,
       0x0700229c,
       0x070022bc,
       0x07013fec,
       0x070024ee,
       0x0700250e,
       0x07013c7a,
       0x0700947c,
       0x0700949c,
       0x07009ade,
       0x0702162c,
       0x07017e2c,
       0x0701cae0,
       0x0701cb00,
       0x07007b70,
       0x07007b90,
       0x07007bb0,
       0x07007bd0,
       0x07007dfe,
       0x07007e48,
       0x0701e108,
       0x0701e128,
       0x07017280,
       0x0700976e,
       0x0700978e,
       0x0701f45e,
       0x0701f47e,
       0x0701f54e,
       0x0701f676,
       0x0701f6b8,
       0x0701f6d8,
       0x07005960,
       0x07005970,
       0x07014094,
       0x070169e6,
       0x07016a06,
       0x07013f48,
       0x0701f01a,
       0x0701f03a,
       0x0701f11a,
       0x0701f2c4,
       0x0701f306,
       0x0701f326,
       0x07014f22,
       0x07007982,
       0x070079a2,
 } },

 { { 0x36103000, 0xa6e0d6db /*[T+Ger.4b_KR155E]*/, 0x126123ad /*[T+Ger1.0_KR155E]*/ }, "Teleroboxer", {
       0xfff2c408,
       0xfff2c3f2,
       0xfff2b626,
       0xfff2c2ee,
       0xfff2c2ae,
       0xfff2b71c,
       0xfff2b736,
       0xfff2c1da,
       0xfff2c21a,
       0xfff2c36c,
       0xfff2b876,
       0xfff2b996,
       0xfff2b9b0,
       0xfff2b970,
       0xfff2baf4,
       0xfff2bb50,
       0xfff2bc7e,
       0xfff2bc9c,
       0xfff6c2a2,
       0xfff6c386,
       0xfff6c39c,
       0xfff6ad22,
       0xfff0304c,
       0xfff03b8e,
       0xfff042ce,
       0xfff04782,
       0xfff04c5c,
       0xfff04d90,
       0xfff11242,
       0xfff12f4a,
       0xfff141b8,
       0xfff04192,
       0xfff0414c,
       0xfff0616c,
       0xfff069d0,
       0xfff07912,
       0xfff1a980,
       0xfff1dc7e,
       0xfff1f060,
       0xfff22c92,
       0xfff2ae50,
       0xfff2af42,
       0xfff2af60,
       0xfff2e08c,
       0xfff32112,
       0xfff3213a,
       0xfff2e9aa,

       0xfff2e66c,
       0xfff2eb50,
       0xfff2eb6e,
       0xfff29f8c,
       0xfff2a1e4,
       0xfff2a36e,
       0xfff2a38c,
       0xfff339b0,
       0xfff339c6,
       0xfff25802,
       0xfff25a1e,
       0xfff2637c,
       0xfff2745c,
       0xfff27a74,
       0xfff29010,
       0xfff04014,

       0xfff32368,
       0xfff32382,
       0xfff32398,
       0xfff325ea,
       0xfff325a4,
       0xfff32668,
       0xfff326c0,
       0xfff326fc,
       0xfff32730,
       0xfff32764,
       0xfff32798,
       0xfff327c2,
       0xfff327f0,
       0xfff0811c,
       0xfff083ac,
       0xfff098e4,

       0xfff0ed12,
       0xfff0f114,
       0xfff0f7aa,
       0xfff101f0,
       0xfff102ae,
       0xfff108be,
       0xfff0a568,
       0xfff0aa78,
       0xfff0be3c,
       0xfff043a4,
       0xfff0435e,
       0xfff0dcf8,
       0xfff0dcb2,

       0xfff054a4,
       0xfff05706,
       0xfff05934,
       0xfff23ad2,
       0xfff23b76,
       0xfff2420e,

       0xfff5d454,
       0xfff5d51a,
       0xfff5d550,
       0xfff30eac,
 } },

 { { 0x40498f5e }, "Tobidase! Panibomb (Japan)", {
       0x07001fe4,
       0x07001f30,
       0x07001fc2,
 } },

 { { 0x133e9372 }, "VB Wario Land", { 0x1c232e, 0x1BFB98, 0x1C215C, 0x1C27C6,
       0xfffc2814,
       0xfffbf49a,
       0xfffbf48c,
       0xfffc45bc,
       0xfffc2956,
 } },

 // Good grief(probably not even all of them!):
 { { 0x3ccb67ae }, "V-Tetris (Japan)", { 0xFFFA2ED4, 0xFFFA9FDC, 0xFFFA776A, 0xFFFA341C, 0xFFFABAB2, 0xFFFACCAE, 0xFFF8B38A, 0xFFFA9C14, 0xFFF8F086, 0xFFF925FE,
       0xfffa2e58,
       0xfffa2e78,
       0xfffa2e98,
       0xfffa2f02,
       0xfffa2f32,
       0xfffa2fb6,
       0xfffa2fd4,
       0xfff965e4,
       0xfffa9f6c,
       0xfffa9f8c,
       0xfffa9fac,
       0xfffaa026,
       0xfffaa044,
       0xfffa76c6,
       0xfffa7704,
       0xfffa7724,
       0xfffa7a2a,
       0xfffa7aba,
       0xfffa30d4,
       0xfffa30f2,
       0xfffa3142,
       0xfffa3158,
       0xfffa3268,
       0xfffa33c0,
       0xfffa33f4,
       0xfffa34b6,
       0xfffa35dc,
       0xfffa35f2,
       0xfffa3684,
       0xfffa369a,
       0xfffa376e,
       0xfffa3784,
       0xfffa3918,
       0xfffa3930,
       0xfffa3946,
       0xfffa39e4,
       0xfffa3a4e,
       0xfffa3ba6,
       0xfffa3bea,
       0xfffa3c00,
       0xfffa3cde,
       0xfffa3db4,
       0xfffa3de0,
       0xfffa3dfe,
       0xfffaba16,
       0xfffaba44,
       0xfffaba76,
       0xfffabc48,
       0xfffabf3e,
       0xfffac0f6,
       0xfffac45e,
       0xfffac59e,
       0xfffac904,
       0xfffaca44,
       0xfffaca94,
       0xfffacbc0,
       0xfffacbf0,
       0xfffacc2c,
       0xfffad262,
       0xfffacfe6,
       0xfffad01c,
       0xfffad296,
       0xfffad2f0,
       0xfff8b012,
       0xfff8b2de,
       0xfff8b2f2,
       0xfff8b31e,
       0xfff8b33e,
       0xfff8bf62,
       0xfff8c20e,
       0xfff8b98e,
       0xfff8bc4e,
       0xfff8c554,
       0xfff8c800,
       0xfff8c824,
       0xfff8c842,
       0xfff8d15e,
       0xfff8d188,
       0xfff8d1c0,
       0xfff8d1f6,
       0xfff8d5da,
       0xfff8d25e,
       0xfff8d27c,
       0xfffa9b98,
       0xfffa9bb8,
       0xfffa9bd8,
       0xfffa9f22,
       0xfffa9f40,
       0xfff9233c,
       0xfff923d0,
       0xfff923e4,
       0xfff9244a,
       0xfff92492,
       0xfff924b2,
       0xfff8f056,
       0xfff8f46a,
       0xfff9250e,
       0xfff925ba,
       0xfff925e0,
       0xfff94036,
       0xfff92f56,
       0xfff93d72,
       0xfff9409c,
       0xfff94270,
       0xfff928c0,
       0xfff92920,
       0xfff92940,
       0xfff9296e,
       0xfff929c8,
       0xfff92a74,
       0xfff92a9c,
       0xfff92aba,
       0xfff9265c,
       0xfff92688,
       0xfff926ba,
       0xfff926e6,
       0xfff92712,
       0xfff8f7b0,
       0xfff8f886,
       0xfff8f8a8,
       0xfff8f96a,
       0xfff8f998,
       0xfff8fae0,
       0xfff8faf6,
       0xfff8fb14,
       0xfff8fba4,
       0xfff8fcd0,
       0xfff8fd0c,
       0xfff8fd9c,
       0xfff8fe4c,
       0xfff8ff7a,
       0xfff8ffc8,
       0xfff90058,
       0xfff90108,
       0xfff90236,
       0xfff90284,
       0xfff90314,
       0xfff903c4,
       0xfff904f2,
       0xfff90540,
       0xfff905d0,
       0xfff90680,
       0xfff907ae,
       0xfff907fc,
       0xfff9088c,
       0xfff908ae,
       0xfff908de,
       0xfff909cc,
       0xfff8f4c6,
       0xfff8f586,
       0xfff8f59a,
       0xfff8f5bc,
       0xfff8f5ec,
       0xfff8f642,
       0xfff8f714,
       0xfff8f740,
       0xfff8f77c,
       0xfff940f4,
       0xfff9411a,
       0xfff94134,
       0xfffad91e,
       0xfff94152,
       0xfffad98a,
       0xfffad9f8,
       0xfffada66,
       0xfffadad8,
       0xfffadb4a,
       0xfffadbbc,
       0xfffadc2e,
       0xfffadc7e,
       0xfffb72e2,
       0xfff94172,
       0xfff941a6,
       0xfffa79b0,
       0xfffa7924,
       0xfffabd9e,
       0xfffabe32,
       0xfffabe54,
       0xfffabee8,
       0xfffabf18,
       0xfffac11a,
       0xfffac2c2,
       0xfffac41a,
       0xfffac5c0,
       0xfffac5d6,
       0xfffac768,
       0xfffac8c0,
       0xfffaca66,
       0xfffacc78,
       0xfffaddb2,
       0xfffade64,
       0xfffadf18,
       0xfffadfcc,
       0xfffae084,
       0xfffae13c,
       0xfffae1f4,
       0xfffae2ac,
       0xfffae2d8,
       0xfffa6dda,
       0xfffa6dfa,
       0xfffa6e24,
       0xfffa6e62,
       0xfffa6e82,
       0xfff9c9e2,
       0xfff9ca02,
       0xfff9ca2e,
       0xfff9cbd4,
       0xfff9cc14,
       0xfff9ccba,
       0xfff9cce6,
       0xfff9cd8c,
       0xfff9cdea,
       0xfff9ce86,
       0xfff9cea4,
       0xfffa6f46,
       0xfffa6f70,
       0xfffa6fae,
       0xfffa6fce,
       0xfff944fe,
       0xfff9453a,
       0xfff945bc,
       0xfff94604,
       0xfff946b0,
       0xfff946d6,
       0xfff946f4,
       0xfff9616a,
       0xfff95e7e,
       0xfff950be,
       0xfff961d0,
       0xfff964f8,
       0xfff948ca,
       0xfff948ea,
       0xfff94918,
       0xfff94972,
       0xfff94a1e,
       0xfff94a46,
       0xfff94a64,
       0xfff962d2,
       0xfff962f8,
       0xfff96312,
       0xfff96330,
       0xfffb7226,
       0xfff96350,
       0xfff96384,
       0xfff9652a,
       0xfff96554,
       0xfffa706a,
       0xffface1a,
       0xffface50,
       0xffface70,
       0xfff8c238,
       0xfff8c3a4,
       0xfff8c3ce,
       0xfff8c536,
       0xfff8de2a,
       0xfff8de54,
       0xfff8de8e,
       0xfff8dec6,
       0xfff8e284,
       0xfff8df1e,
       0xfff8df3c,
       0xfffa9dce,
       0xfffa9e56,
       0xfff943a2,
       0xfff94454,
       0xfff944e8,
       0xfff9486a,
       0xfff8cc76,
       0xfff8cca0,
       0xfff8ccda,
       0xfff8cd12,
       0xfff8d0d0,
       0xfff8cd6a,
       0xfff8cd88,
       0xfff8dda4,
       0xfff8d8bc,
       0xfff8ddc8,
       0xfff8ddf0,
       0xfff8c8de,
       0xfff8cbaa,
       0xfff8cbbe,
       0xfff8cbe8,
       0xfff8cc10,
       0xfff8d0f4,
       0xfff8d11c,
       0xfff8e2a8,
       0xfff8e2d0,
       0xfff8bc6c,
       0xfff8bf2c,
       0xfff8bf44,
       0xfff8f680,
       0xfff8e4f8,
       0xfff8e518,
       0xfff8e5ce,
       0xfff8e606,
       0xfff8ed2e,
       0xfff8eb30,
       0xfff8efa6,
       0xfff8efd0,
       0xfff8f012,
       0xfff9cdb2,
       0xfffa146a,
       0xfffa169c,
       0xfffa16ca,
       0xfffa16fa,
       0xfffa1766,
       0xfffa179e,
       0xfffa21d8,
       0xfffa2272,
       0xfffa2290,
       0xfffa22a6,
       0xfffa24d4,
       0xfff8df6e,
       0xfff8df98,
       0xfff8dfc6,
       0xfff8cdba,
       0xfff8cde4,
       0xfff8ce12,
       0xfff8d2ae,
       0xfff8d2d8,
       0xfff8d30e,
 } },

 { { 0x4c32ba5e, 0xdc8c0bf4 /*[h1]*/ }, "Vertical Force (US)", { 0x7000BF4 } },

 // Is 0x05d06377 a bad dump?
 { { 0x9e9b8b92, 0x05d06377 }, "Vertical Force (Japan)", { 0x7000BF4 } },

 { { 0x20688279, 0xddf9abe0 /*(Debug Menu Hack)*/ }, "Virtual Bowling (Japan)", {
       0xfff24eda,
       0xfff28bea,
       0xfff28c00,
       0xfff1fb9a,
       0xfff1c284,
       0xfff1ddc4,
       0xfff0b93e,
       0xfff249ac,
       0xfff0b9a4,
       0xfff258fc,
       0xfff172aa,
       0xfff2606e,
       0xfff1a0e6,
       0xfff1a0fc,
       0xfff17222,
       0xfff26058,
       0xfff0b984,
       0xfff21080,
       0xfff21096,
       0xfff1ddae,
       0xfff13288,
       0xfff132d8,
       0xfff13ec8,
       0xfff0b920,
       0xfff284e8,
       0xfff284fe,
       0xfff154ac,
       0xfff155f8,
       0xfff15706,
       0xfff2f51e,
       0xfff1fb84,
       0xfff27310,
       0xfff0c480,
       0xfff2d618,
       0xfff2d62e,
 } },

 { { 0x526cc969, 0x45471e40 /*[b1]*/ }, "Virtual Fishing (Japan)", {
       0x07000388,
       0x07000368,
 } },


 { { 0x8989fe0a }, "Virtual Lab (Japan)", {
       0x070000ae,
       0x0700395a,
       0x070051ac,
       0x070039cc,
       0x07003a96,
       0x07003b1c,
       0x07003bce,
       0x07003c4e,
       0x07003d42,
       0x07003e42,
       0x07003e9c,
       0x07003f30,
       0x07004098,
       0x070041ee,
       0x07003180,
       0x070050f4,
       0x07002fc4,
       0x07000960,
       0x070009a6,
       0x07000b60,
       0x07000bb4,
       0x07001764,
       0x070020ec,
       0x070075cc,
       0x07003112,
       0x070001b2,
       0x07001074,
       0x070010e4,
       0x070011d0,
       0x07001262,
       0x070012f6,
       0x070013f4,
       0x070014bc,
       0x070015a2,
       0x0700162e,
       0x070023b8,
       0x07002790,
       0x070033ca,
       0x070034c8,
       0x07003254,
       0x070035d0,
       0x0700369e,
       0x0700370a,
       0x07003796,
       0x070037fc,
       0x070032f6,
 } },

 // Is 0xcc62ab38 a bad dump?
 { { 0x736b40d6, 0xcc62ab38 }, "Virtual League Baseball (US)", {
       0x07000bbc,
       0x070011a4,
       0x07000cc4,
       0x07000c1c,
       0x07000c6a,
       0x07000be0,
       0x07000c40,
 } },

 { { 0x9ba8bb5e }, "Virtual Pro Yakyuu '95", {
       0x07000bbc,
       0x070011a4,
       0x07000cc4,
       0x07000c1c,
       0x07000c6a,
       0x07000be0,
       0x07000c40,
 } },

  { { 0x82a95e51, 0x742298d1 /*[b1]*/  }, "Waterworld (US)", { 	// Apparently has complex wait loop.
       0x070008fc,
       0x0700090e,
       0x0700209e,
       0x070020b4,
       0x070009da,
       0x0700222a,
       0x07002312,
       0x070023f8,
       0x07002680,
       0x07002c68,
       0x0700303c,
       0x07003052,
       0x0700397e,
       0x07003994,
       0x07000bb4,
       0x07000ac8,
 } },

 { { 0x44C2B723 } , "Space Pinball (Prototype)", { 
       0x0702EA7A
 }},
};

static int Load(const uint8_t *data, size_t size)
{
   V810_Emu_Mode cpu_mode;

   VB_InDebugPeek = 0;

   cpu_mode = (V810_Emu_Mode)MDFN_GetSettingI("vb.cpu_emulation");

   if(size != round_up_pow2(size))
   {
      puts("VB ROM image size is not a power of 2???");
      return(0);
   }

   if(size < 256)
   {
      puts("VB ROM image size is too small??");
      return(0);
   }

   if(size > (1 << 24))
   {
      puts("VB ROM image size is too large??");
      return(0);
   }

   VB_HeaderInfo hinfo;

   ReadHeader(data, size, &hinfo);

   log_cb(RETRO_LOG_INFO, "Title:     %s\n", hinfo.game_title);
   log_cb(RETRO_LOG_INFO, "Game ID Code: %u\n", hinfo.game_code);
   log_cb(RETRO_LOG_INFO, "Manufacturer Code: %d\n", hinfo.manf_code);
   log_cb(RETRO_LOG_INFO, "Version:   %u\n", hinfo.version);

   log_cb(RETRO_LOG_INFO, "ROM:       %dKiB\n", (int)(size / 1024));

   log_cb(RETRO_LOG_INFO, "V810 Emulation Mode: %s\n", (cpu_mode == V810_EMU_MODE_ACCURATE) ? "Accurate" : "Fast");

   VB_V810 = new V810();
   VB_V810->Init(cpu_mode, true);

   VB_V810->SetMemReadHandlers(MemRead8, MemRead16, NULL);
   VB_V810->SetMemWriteHandlers(MemWrite8, MemWrite16, NULL);

   VB_V810->SetIOReadHandlers(MemRead8, MemRead16, NULL);
   VB_V810->SetIOWriteHandlers(MemWrite8, MemWrite16, NULL);

   for(int i = 0; i < 256; i++)
   {
      VB_V810->SetMemReadBus32(i, false);
      VB_V810->SetMemWriteBus32(i, false);
   }

   std::vector<uint32> Map_Addresses;

   for(uint64 A = 0; A < 1ULL << 32; A += (1 << 27))
   {
      for(uint64 sub_A = 5 << 24; sub_A < (6 << 24); sub_A += 65536)
      {
         Map_Addresses.push_back(A + sub_A);
      }
   }

   WRAM = VB_V810->SetFastMap(&Map_Addresses[0], 65536, Map_Addresses.size(), "WRAM");
   Map_Addresses.clear();


   // Round up the ROM size to 65536(we mirror it a little later)
   GPROM_Mask = (size < 65536) ? (65536 - 1) : (size - 1);

   for(uint64 A = 0; A < 1ULL << 32; A += (1 << 27))
   {
      for(uint64 sub_A = 7 << 24; sub_A < (8 << 24); sub_A += GPROM_Mask + 1)
      {
         Map_Addresses.push_back(A + sub_A);
         //printf("%08x\n", (uint32)(A + sub_A));
      }
   }


   GPROM = VB_V810->SetFastMap(&Map_Addresses[0], GPROM_Mask + 1, Map_Addresses.size(), "Cart ROM");
   Map_Addresses.clear();

   // Mirror ROM images < 64KiB to 64KiB
   for(uint64 i = 0; i < 65536; i += size)
      memcpy(GPROM + i, data, size);

   GPRAM_Mask = 0xFFFF;

   for(uint64 A = 0; A < 1ULL << 32; A += (1 << 27))
   {
      for(uint64 sub_A = 6 << 24; sub_A < (7 << 24); sub_A += GPRAM_Mask + 1)
      {
         //printf("GPRAM: %08x\n", A + sub_A);
         Map_Addresses.push_back(A + sub_A);
      }
   }


   GPRAM = VB_V810->SetFastMap(&Map_Addresses[0], GPRAM_Mask + 1, Map_Addresses.size(), "Cart RAM");
   Map_Addresses.clear();

   memset(GPRAM, 0, GPRAM_Mask + 1);

   VIP_Init();
   VB_VSU = new VSU(&sbuf[0], &sbuf[1]);
   VBINPUT_Init();

   VB3DMode = MDFN_GetSettingUI("vb.3dmode");
   uint32 prescale = MDFN_GetSettingUI("vb.liprescale");
   uint32 sbs_separation = MDFN_GetSettingUI("vb.sidebyside.separation");

   VIP_Set3DMode(VB3DMode, MDFN_GetSettingUI("vb.3dreverse"), prescale, sbs_separation);


   //SettingChanged("vb.3dmode");
   SettingChanged("vb.disable_parallax");
   SettingChanged("vb.anaglyph.lcolor");
   SettingChanged("vb.anaglyph.rcolor");
   SettingChanged("vb.anaglyph.preset");
   SettingChanged("vb.default_color");

   SettingChanged("vb.instant_display_hack");
   SettingChanged("vb.allow_draw_skip");

   SettingChanged("vb.input.instant_read_hack");

   MDFNGameInfo->fps = (int64)20000000 * 65536 * 256 / (259 * 384 * 4);


   VB_Power();


#ifdef WANT_DEBUGGER
   VBDBG_Init();
#endif


   MDFNGameInfo->nominal_width = 384;
   MDFNGameInfo->nominal_height = 224;
   MDFNGameInfo->fb_width = 384;
   MDFNGameInfo->fb_height = 224;

   switch(VB3DMode)
   {
      default: break;

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


   MDFNMP_Init(32768, ((uint64)1 << 27) / 32768);
   MDFNMP_AddRAM(65536, 5 << 24, WRAM);
   if((GPRAM_Mask + 1) >= 32768)
      MDFNMP_AddRAM(GPRAM_Mask + 1, 6 << 24, GPRAM);
   return(1);
}

static void CloseGame(void)
{
 //VIP_Kill();
 
 if(VB_VSU)
 {
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

 if(VB_V810)
 {
  VB_V810->Kill();
  delete VB_V810;
  VB_V810 = NULL;
 }
}

void VB_ExitLoop(void)
{
 VB_V810->Exit();
}

static void Emulate(EmulateSpecStruct *espec)
{
 v810_timestamp_t v810_timestamp;

 MDFNMP_ApplyPeriodicCheats();

 VBINPUT_Frame();

 if(espec->SoundFormatChanged)
 {
  for(int y = 0; y < 2; y++)
  {
   Blip_Buffer_set_sample_rate(&sbuf[y], espec->SoundRate ? espec->SoundRate : 44100, 50);
   Blip_Buffer_set_clock_rate(&sbuf[y], (long)(VB_MASTER_CLOCK / 4));
   Blip_Buffer_bass_freq(&sbuf[y], 20);
  }
 }

 VIP_StartFrame(espec);

 v810_timestamp = VB_V810->Run(EventHandler);

 FixNonEvents();
 ForceEventUpdates(v810_timestamp);

 VB_VSU->EndFrame((v810_timestamp + VSU_CycleFix) >> 2);

 if(espec->SoundBuf)
 {
  for(int y = 0; y < 2; y++)
  {
   Blip_Buffer_end_frame(&sbuf[y], (v810_timestamp + VSU_CycleFix) >> 2);
   espec->SoundBufSize = Blip_Buffer_read_samples(&sbuf[y], espec->SoundBuf + y, espec->SoundBufMaxSize);
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

#ifdef WANT_DEBUGGER
static DebuggerInfoStruct DBGInfo =
{
 "shift_jis",
 4,
 2,             // Instruction alignment(bytes)
 32,
 32,
 0x00000000,
 ~0U,

 VBDBG_MemPeek,
 VBDBG_Disassemble,
 NULL,
 NULL,	//ForceIRQ,
 NULL,
 VBDBG_FlushBreakPoints,
 VBDBG_AddBreakPoint,
 VBDBG_SetCPUCallback,
 VBDBG_EnableBranchTrace,
 VBDBG_GetBranchTrace,
 NULL, 	//KING_SetGraphicsDecode,
 VBDBG_SetLogFunc,
};
#endif


int StateAction(StateMem *sm, int load, int data_only)
{
 const v810_timestamp_t timestamp = VB_V810->v810_timestamp;
 int ret = 1;

 SFORMAT StateRegs[] =
 {
  SFARRAY(WRAM, 65536),
  SFARRAY(GPRAM, GPRAM_Mask ? (GPRAM_Mask + 1) : 0),
  SFVAR(WCR),
  SFVAR(IRQ_Asserted),
  SFVAR(VSU_CycleFix),
  SFEND
 };

 ret &= MDFNSS_StateAction(sm, load, data_only, StateRegs, "MAIN", false);

 ret &= VB_V810->StateAction(sm, load, data_only);

 ret &= VB_VSU->StateAction(sm, load, data_only);
 ret &= TIMER_StateAction(sm, load, data_only);
 ret &= VBINPUT_StateAction(sm, load, data_only);
 ret &= VIP_StateAction(sm, load, data_only);

 if(load)
 {
  // Needed to recalculate next_*_ts since we don't bother storing their deltas in save states.
  ForceEventUpdates(timestamp);
 }
 return(ret);
}

static void SetLayerEnableMask(uint64 mask)
{

}

static void DoSimpleCommand(int cmd)
{
 switch(cmd)
 {
  case MDFN_MSC_POWER:
  case MDFN_MSC_RESET: VB_Power(); break;
 }
}

static const MDFNSetting_EnumList V810Mode_List[] =
{
 { "fast", (int)V810_EMU_MODE_FAST, "Fast Mode", "Fast mode trades timing accuracy, cache emulation, and executing from hardware registers and RAM not intended for code use for performance."},
 { "accurate", (int)V810_EMU_MODE_ACCURATE, "Accurate Mode", "Increased timing accuracy, though not perfect, along with cache emulation, at the cost of decreased performance.  Additionally, even the pipeline isn't correctly and fully emulated in this mode." },
 { NULL, 0 },
};

static const MDFNSetting_EnumList VB3DMode_List[] =
{
 { "anaglyph", VB3DMODE_ANAGLYPH, "Anaglyph", "Used in conjunction with classic dual-lens-color glasses." },
 { "cscope",  VB3DMODE_CSCOPE, "CyberScope", "Intended for use with the CyberScope 3D device." },
 { "sidebyside", VB3DMODE_SIDEBYSIDE, "Side-by-Side", "The left-eye image is displayed on the left, and the right-eye image is displayed on the right." },
// { "overunder", VB3DMODE_OVERUNDER },
 { "vli", VB3DMODE_VLI, "Vertical Line Interlaced", "Vertical lines alternate between left view and right view." },
 { "hli", VB3DMODE_HLI, "Horizontal Line Interlaced", "Horizontal lines alternate between left view and right view." },
 { NULL, 0 },
};

static const MDFNSetting_EnumList AnaglyphPreset_List[] =
{
 { "disabled", ANAGLYPH_PRESET_DISABLED, "Disabled", "Forces usage of custom anaglyph colors." },
 { "0", ANAGLYPH_PRESET_DISABLED },

 { "red_blue", ANAGLYPH_PRESET_RED_BLUE, "Red/Blue", "Classic red/blue anaglyph." },
 { "red_cyan", ANAGLYPH_PRESET_RED_CYAN, "Red/Cyan", "Improved quality red/cyan anaglyph." },
 { "red_electriccyan", ANAGLYPH_PRESET_RED_ELECTRICCYAN, "Red/Electric Cyan", "Alternate version of red/cyan" },
 { "red_green", ANAGLYPH_PRESET_RED_GREEN, "Red/Green" },
 { "green_magenta", ANAGLYPH_PRESET_GREEN_MAGENTA, "Green/Magenta" },
 { "yellow_blue", ANAGLYPH_PRESET_YELLOW_BLUE, "Yellow/Blue" },

 { NULL, 0 },
};

static MDFNSetting VBSettings[] =
{
 { "vb.cpu_emulation", MDFNSF_EMU_STATE | MDFNSF_UNTRUSTED_SAFE, "CPU emulation mode.", NULL, MDFNST_ENUM, "fast", NULL, NULL, NULL, NULL, V810Mode_List },
 { "vb.input.instant_read_hack", MDFNSF_EMU_STATE | MDFNSF_UNTRUSTED_SAFE, "Input latency reduction hack.", "Reduces latency in some games by 20ms by returning the current pad state, rather than latched state, on serial port data reads.  This hack may cause some homebrew software to malfunction, but it should be relatively safe for commercial official games.", MDFNST_BOOL, "1", NULL, NULL, NULL, SettingChanged },
 
 { "vb.instant_display_hack", MDFNSF_NOFLAGS, "Display latency reduction hack.", "Reduces latency in games by displaying the framebuffer 20ms earlier.  This hack has some potential of causing graphical glitches, so it is disabled by default.", MDFNST_BOOL, "0", NULL, NULL, NULL, SettingChanged },
 { "vb.allow_draw_skip", MDFNSF_EMU_STATE | MDFNSF_UNTRUSTED_SAFE, "Allow draw skipping.", "If vb.instant_display_hack is set to \"1\", and this setting is set to \"1\", then frame-skipping the drawing to the emulated framebuffer will be allowed.  THIS WILL CAUSE GRAPHICAL GLITCHES, AND THEORETICALLY(but unlikely) GAME CRASHES, ESPECIALLY WITH DIRECT FRAMEBUFFER DRAWING GAMES.", MDFNST_BOOL, "0", NULL, NULL, NULL, SettingChanged },

 // FIXME: We're going to have to set up some kind of video mode change notification for changing vb.3dmode while the game is running to work properly.
 { "vb.3dmode", MDFNSF_NOFLAGS, "3D mode.", NULL, MDFNST_ENUM, "anaglyph", NULL, NULL, NULL, /*SettingChanged*/NULL, VB3DMode_List },
 { "vb.liprescale", MDFNSF_NOFLAGS, "Line Interlaced prescale.", NULL, MDFNST_UINT, "2", "1", "10", NULL, NULL },

 { "vb.disable_parallax", MDFNSF_EMU_STATE | MDFNSF_UNTRUSTED_SAFE, "Disable parallax for BG and OBJ rendering.", NULL, MDFNST_BOOL, "0", NULL, NULL, NULL, SettingChanged },
 { "vb.default_color", MDFNSF_NOFLAGS, "Default maximum-brightness color to use in non-anaglyph 3D modes.", NULL, MDFNST_UINT, "0xF0F0F0", "0x000000", "0xFFFFFF", NULL, SettingChanged },

 { "vb.anaglyph.preset", MDFNSF_NOFLAGS, "Anaglyph preset colors.", NULL, MDFNST_ENUM, "red_blue", NULL, NULL, NULL, SettingChanged, AnaglyphPreset_List },
 { "vb.anaglyph.lcolor", MDFNSF_NOFLAGS, "Anaglyph maximum-brightness color for left view.", NULL, MDFNST_UINT, "0xffba00", "0x000000", "0xFFFFFF", NULL, SettingChanged },
 { "vb.anaglyph.rcolor", MDFNSF_NOFLAGS, "Anaglyph maximum-brightness color for right view.", NULL, MDFNST_UINT, "0x00baff", "0x000000", "0xFFFFFF", NULL, SettingChanged },

 { "vb.sidebyside.separation", MDFNSF_NOFLAGS, "Number of pixels to separate L/R views by.", "This setting refers to pixels before vb.xscale(fs) scaling is taken into consideration.  For example, a value of \"100\" here will result in a separation of 300 screen pixels if vb.xscale(fs) is set to \"3\".", MDFNST_UINT, /*"96"*/"0", "0", "1024", NULL, NULL },

 { "vb.3dreverse", MDFNSF_NOFLAGS, "Reverse left/right 3D views.", NULL, MDFNST_BOOL, "0", NULL, NULL, NULL, SettingChanged },
 { NULL }
};


static const InputDeviceInputInfoStruct IDII[] =
{
 { "a", "A", 7, IDIT_BUTTON_CAN_RAPID,  NULL },
 { "b", "B", 6, IDIT_BUTTON_CAN_RAPID, NULL },
 { "rt", "Right-Back", 13, IDIT_BUTTON, NULL },
 { "lt", "Left-Back", 12, IDIT_BUTTON, NULL },

 { "up-r", "UP ↑ (Right D-Pad)", 8, IDIT_BUTTON, "down-r" },
 { "right-r", "RIGHT → (Right D-Pad)", 11, IDIT_BUTTON, "left-r" },

 { "right-l", "RIGHT → (Left D-Pad)", 3, IDIT_BUTTON, "left-l" },
 { "left-l", "LEFT ← (Left D-Pad)", 2, IDIT_BUTTON, "right-l" },
 { "down-l", "DOWN ↓ (Left D-Pad)", 1, IDIT_BUTTON, "up-l" },
 { "up-l", "UP ↑ (Left D-Pad)", 0, IDIT_BUTTON, "down-l" },

 { "start", "Start", 5, IDIT_BUTTON, NULL },
 { "select", "Select", 4, IDIT_BUTTON, NULL },

 { "left-r", "LEFT ← (Right D-Pad)", 10, IDIT_BUTTON, "right-r" },
 { "down-r", "DOWN ↓ (Right D-Pad)", 9, IDIT_BUTTON, "up-r" },
};

static InputDeviceInfoStruct InputDeviceInfo[] =
{
 {
  "gamepad",
  "Gamepad",
  NULL,
  NULL,
  sizeof(IDII) / sizeof(InputDeviceInputInfoStruct),
  IDII,
 }
};

static const InputPortInfoStruct PortInfo[] =
{
 { "builtin", "Built-In", sizeof(InputDeviceInfo) / sizeof(InputDeviceInfoStruct), InputDeviceInfo, "gamepad" }
};

static InputInfoStruct InputInfo =
{
 sizeof(PortInfo) / sizeof(InputPortInfoStruct),
 PortInfo
};


static const FileExtensionSpecStruct KnownExtensions[] =
{
 { ".vb", "Nintendo Virtual Boy" },
 { ".vboy", "Nintendo Virtual Boy" },
 { NULL, NULL }
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

 384,	// Nominal width
 224,	// Nominal height

 384,	// Framebuffer width
 256,	// Framebuffer height

 2,     // Number of output sound channels
};

MDFNGI *MDFNGameInfo = &EmulatedVB;

/* forward declarations */
extern void MDFND_DispMessage(unsigned char *str);

void MDFN_ResetMessages(void)
{
 MDFND_DispMessage(NULL);
}


static MDFNGI *MDFNI_LoadGame(const uint8_t *data, size_t size)
{
   MDFNGameInfo = &EmulatedVB;

	// Load per-game settings
	// Maybe we should make a "pgcfg" subdir, and automatically load all files in it?
	// End load per-game settings
	//

   if(Load(data, size) <= 0)
      goto error;

	MDFN_LoadGameCheats(NULL);
	MDFNMP_InstallReadPatches();

	MDFN_ResetMessages();	// Save state, status messages, etc.

   return(MDFNGameInfo);

error:
   MDFNGameInfo = NULL;
   return NULL;
}

static void MDFNI_CloseGame(void)
{
   if(!MDFNGameInfo)
      return;

   MDFN_FlushGameCheats(0);

   CloseGame();

   MDFNMP_Kill();

   MDFNGameInfo = NULL;
}

bool MDFNI_InitializeModule(void)
{
 return(1);
}

int MDFNI_Initialize(const char *basedir)
{
	return(1);
}

static void hookup_ports(bool force);

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
#define FB_WIDTH 384
#define FB_HEIGHT 224


#define FB_MAX_HEIGHT FB_HEIGHT

const char *mednafen_core_str = MEDNAFEN_CORE_NAME;

static void check_system_specs(void)
{
   unsigned level = 0;
   environ_cb(RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL, &level);
}

void retro_init(void)
{
   struct retro_log_callback log;
#if defined(WANT_16BPP) && defined(FRONTEND_SUPPORTS_RGB565)
   enum retro_pixel_format rgb565 = RETRO_PIXEL_FORMAT_RGB565;
#endif
   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;
   else 
      log_cb = NULL;

   MDFNI_InitializeModule();

#if defined(WANT_16BPP) && defined(FRONTEND_SUPPORTS_RGB565)
   if (environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &rgb565) && log_cb)
      log_cb(RETRO_LOG_INFO, "Frontend supports RGB565 - will use that instead of XRGB1555.\n");
#endif

   if (environ_cb(RETRO_ENVIRONMENT_GET_PERF_INTERFACE, &perf_cb))
      perf_get_cpu_features_cb = perf_cb.get_cpu_features;
   else
      perf_get_cpu_features_cb = NULL;

   check_system_specs();
}

void retro_reset(void)
{
   DoSimpleCommand(MDFN_MSC_RESET);
}

bool retro_load_game_special(unsigned, const struct retro_game_info *, size_t)
{
   return false;
}

static void set_volume (uint32_t *ptr, unsigned number)
{
   switch(number)
   {
      default:
         *ptr = number;
         break;
   }
}

static void check_variables(void)
{
   struct retro_variable var = {0};

   var.key = "vb_color_mode";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "black & red") == 0)
	  {
         setting_vb_lcolor = 0xFF0000;
		 setting_vb_rcolor = 0x000000;
	  }
      else if (strcmp(var.value, "black & white") == 0)
	  {
         setting_vb_lcolor = 0xFFFFFF;      
		 setting_vb_rcolor = 0x000000;
	  }
      log_cb(RETRO_LOG_INFO, "[%s]: Palette changed: %s .\n", mednafen_core_str, var.value);  
   }   
   
   var.key = "vb_anaglyph_preset";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "disabled") == 0)
		setting_vb_anaglyph_preset = 0; 
      else if (strcmp(var.value, "red & blue") == 0)
         setting_vb_anaglyph_preset = 1;      
      else if (strcmp(var.value, "red & cyan") == 0)
         setting_vb_anaglyph_preset = 2;      
	  else if (strcmp(var.value, "red & electric cyan") == 0)	 
         setting_vb_anaglyph_preset = 3;      
      else if (strcmp(var.value, "red & green") == 0)
         setting_vb_anaglyph_preset = 4;      
      else if (strcmp(var.value, "green & magenta") == 0)
         setting_vb_anaglyph_preset = 5;      
      else if (strcmp(var.value, "yellow & blue") == 0)
         setting_vb_anaglyph_preset = 6;      

      log_cb(RETRO_LOG_INFO, "[%s]: Palette changed: %s .\n", mednafen_core_str, var.value);  
   }    

   var.key = "vb_right_analog_to_digital";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "disabled") == 0)
         setting_vb_right_analog_to_digital = false;
      else if (strcmp(var.value, "enabled") == 0)
      {
         setting_vb_right_analog_to_digital = true;
         setting_vb_right_invert_x = false;
         setting_vb_right_invert_y = false;
      }
      else if (strcmp(var.value, "invert x") == 0)
      {
         setting_vb_right_analog_to_digital = true;
         setting_vb_right_invert_x = true;
         setting_vb_right_invert_y = false;
      }
      else if (strcmp(var.value, "invert y") == 0)
      {
         setting_vb_right_analog_to_digital = true;
         setting_vb_right_invert_x = false;
         setting_vb_right_invert_y = true;
      }
      else if (strcmp(var.value, "invert both") == 0)
      {
         setting_vb_right_analog_to_digital = true;
         setting_vb_right_invert_x = true;
         setting_vb_right_invert_y = true;
      }
      else
         setting_vb_right_analog_to_digital = false;
   } 
}

#define MAX_PLAYERS 1
#define MAX_BUTTONS 14
static uint16_t input_buf[MAX_PLAYERS];

static void hookup_ports(bool force)
{
   if (initial_ports_hookup && !force)
      return;

   // Possible endian bug ...
   VBINPUT_SetInput(0, "gamepad", &input_buf[0]);

   initial_ports_hookup = true;
}

bool retro_load_game(const struct retro_game_info *info)
{
   if (!info)
      return false;

   struct retro_input_descriptor desc[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left D-Pad Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Left D-Pad Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Left D-Pad Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Left D-Pad Right" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "L" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "R" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "Right D-Pad Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "Right D-Pad Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3, "Right D-Pad Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, "Right D-Pad Right" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },

      { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X, "Right D-Pad X" },
      { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y, "Right D-Pad Y" },
      { 0 },
   };

   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

#ifdef WANT_32BPP
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      if (log_cb)
         log_cb(RETRO_LOG_ERROR, "Pixel format XRGB8888 not supported by platform, cannot use %s.\n", MEDNAFEN_CORE_NAME);
      return false;
   }
#endif

   overscan = false;
   environ_cb(RETRO_ENVIRONMENT_GET_OVERSCAN, &overscan);

   check_variables();

   game = MDFNI_LoadGame((const uint8_t*)info->data, info->size);
   if (!game)
      return false;

   MDFN_PixelFormat pix_fmt(MDFN_COLORSPACE_RGB, 16, 8, 0, 24);
   memset(&last_pixel_format, 0, sizeof(MDFN_PixelFormat));
   
   surf = new MDFN_Surface(NULL, FB_WIDTH, FB_HEIGHT, FB_WIDTH, pix_fmt);

   hookup_ports(true);

   check_variables();

   return game;
}

void retro_unload_game()
{
   if (!game)
      return;

   MDFNI_CloseGame();
}



static void update_input(void)
{
   input_buf[0] = 0;

   static unsigned map[] = {
      RETRO_DEVICE_ID_JOYPAD_A,
      RETRO_DEVICE_ID_JOYPAD_B,
      RETRO_DEVICE_ID_JOYPAD_R,
      RETRO_DEVICE_ID_JOYPAD_L,
      RETRO_DEVICE_ID_JOYPAD_L2, //right d-pad UP
      RETRO_DEVICE_ID_JOYPAD_R3, //right d-pad RIGHT
      RETRO_DEVICE_ID_JOYPAD_RIGHT, //left d-pad
      RETRO_DEVICE_ID_JOYPAD_LEFT, //left d-pad
      RETRO_DEVICE_ID_JOYPAD_DOWN, //left d-pad
      RETRO_DEVICE_ID_JOYPAD_UP, //left d-pad
      RETRO_DEVICE_ID_JOYPAD_START,
      RETRO_DEVICE_ID_JOYPAD_SELECT,
      RETRO_DEVICE_ID_JOYPAD_R2, //right d-pad LEFT
      RETRO_DEVICE_ID_JOYPAD_L3, //right d-pad DOWN
   };

   for (unsigned j = 0; j < MAX_PLAYERS; j++)
   {
      for (unsigned i = 0; i < MAX_BUTTONS; i++)
         input_buf[j] |= map[i] != -1u &&
            input_state_cb(j, RETRO_DEVICE_JOYPAD, 0, map[i]) ? (1 << i) : 0;
            
      if (setting_vb_right_analog_to_digital)
      {
         int16_t analog_x = input_state_cb(j, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X);
         int16_t analog_y = input_state_cb(j, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);

         if (abs(analog_x) > STICK_DEADZONE)
            input_buf[j] |= (analog_x < 0) ^ !setting_vb_right_invert_x ? RIGHT_DPAD_RIGHT : RIGHT_DPAD_LEFT;
         if (abs(analog_y) > STICK_DEADZONE)
            input_buf[j] |= (analog_y < 0) ^ !setting_vb_right_invert_y ? RIGHT_DPAD_DOWN : RIGHT_DPAD_UP;
      }

#ifdef MSB_FIRST
      union {
         uint8_t b[2];
         uint16_t s;
      } u;
      u.s = input_buf[j];
      input_buf[j] = u.b[0] | u.b[1] << 8;
#endif
   }
}

static uint64_t video_frames, audio_frames;

void retro_run(void)
{
   MDFNGI *curgame = game;

   input_poll_cb();

   update_input();

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

   if (memcmp(&last_pixel_format, &spec.surface->format, sizeof(MDFN_PixelFormat)))
   {
      spec.VideoFormatChanged = true;

      last_pixel_format = spec.surface->format;
   }

   if (spec.SoundRate != last_sound_rate)
   {
      spec.SoundFormatChanged = true;
      last_sound_rate = spec.SoundRate;
   }

   Emulate(&spec);

   int16 *const SoundBuf = spec.SoundBuf + spec.SoundBufSizeALMS * curgame->soundchan;
   int32 SoundBufSize = spec.SoundBufSize - spec.SoundBufSizeALMS;
   const int32 SoundBufMaxSize = spec.SoundBufMaxSize - spec.SoundBufSizeALMS;

   spec.SoundBufSize = spec.SoundBufSizeALMS + SoundBufSize;

   unsigned width  = spec.DisplayRect.w;
   unsigned height = spec.DisplayRect.h;

#if defined(WANT_32BPP)
   const uint32_t *pix = surf->pixels;
   video_cb(pix, width, height, FB_WIDTH << 2);
#elif defined(WANT_16BPP)
   const uint16_t *pix = surf->pixels16;
   video_cb(pix, width, height, FB_WIDTH << 1);
#endif

   video_frames++;
   audio_frames += spec.SoundBufSize;

   audio_batch_cb(spec.SoundBuf, spec.SoundBufSize);

   bool updated = false;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      check_variables();
}

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = MEDNAFEN_CORE_NAME;
#ifndef GIT_VERSION
#define GIT_VERSION ""
#endif
   info->library_version  = MEDNAFEN_CORE_VERSION GIT_VERSION;
   info->need_fullpath    = false;
   info->valid_extensions = MEDNAFEN_CORE_EXTENSIONS;
   info->block_extract    = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   memset(info, 0, sizeof(*info));
   info->timing.fps            = MEDNAFEN_CORE_TIMING_FPS;
   info->timing.sample_rate    = 44100;
   info->geometry.base_width   = MEDNAFEN_CORE_GEOMETRY_BASE_W;
   info->geometry.base_height  = MEDNAFEN_CORE_GEOMETRY_BASE_H;
   info->geometry.max_width    = MEDNAFEN_CORE_GEOMETRY_MAX_W;
   info->geometry.max_height   = MEDNAFEN_CORE_GEOMETRY_MAX_H;
   info->geometry.aspect_ratio = MEDNAFEN_CORE_GEOMETRY_ASPECT_RATIO;
}

void retro_deinit()
{
   delete surf;
   surf = NULL;

   if (log_cb)
   {
      log_cb(RETRO_LOG_INFO, "[%s]: Samples / Frame: %.5f\n",
            mednafen_core_str, (double)audio_frames / video_frames);
      log_cb(RETRO_LOG_INFO, "[%s]: Estimated FPS: %.5f\n",
            mednafen_core_str, (double)video_frames * 44100 / audio_frames);
   }
}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC; // FIXME: Regions for other cores.
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned in_port, unsigned device)
{
}

void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;

   static const struct retro_variable vars[] = {
      { "vb_anaglyph_preset", "Anaglyph preset (restart); disabled|red & blue|red & cyan|red & electric cyan|red & green|green & magenta|yellow & blue" },
      { "vb_color_mode", "Palette (restart); black & red|black & white" },
      { "vb_right_analog_to_digital", "Right analog to digital; disabled|enabled|invert x|invert y|invert both" },
      { NULL, NULL },
   };
   cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)vars);
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
   audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
   input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
   input_state_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}

static size_t serialize_size;

size_t retro_serialize_size(void)
{
   StateMem st;

   st.data           = NULL;
   st.loc            = 0;
   st.len            = 0;
   st.malloced       = 0;
   st.initial_malloc = 0;

   if (!MDFNSS_SaveSM(&st, 0, 0, NULL, NULL, NULL))
      return 0;

   free(st.data);
   return serialize_size = st.len;
}

bool retro_serialize(void *data, size_t size)
{
   StateMem st;
   bool ret          = false;
   uint8_t *_dat     = (uint8_t*)malloc(size);

   if (!_dat)
      return false;

   /* Mednafen can realloc the buffer so we need to ensure this is safe. */
   st.data           = _dat;
   st.loc            = 0;
   st.len            = 0;
   st.malloced       = size;
   st.initial_malloc = 0;

   ret = MDFNSS_SaveSM(&st, 0, 0, NULL, NULL, NULL);

   memcpy(data, st.data, size);
   free(st.data);

   return ret;
}

bool retro_unserialize(const void *data, size_t size)
{
   StateMem st;

   st.data           = (uint8_t*)data;
   st.loc            = 0;
   st.len            = size;
   st.malloced       = 0;
   st.initial_malloc = 0;

   return MDFNSS_LoadSM(&st, 0, 0);
}

void *retro_get_memory_data(unsigned type)
{
   switch(type)
   {
      case RETRO_MEMORY_SYSTEM_RAM:
         return WRAM;
      case RETRO_MEMORY_SAVE_RAM:
         return GPRAM;
      default:
         return NULL;
   }
}

size_t retro_get_memory_size(unsigned type)
{
   switch(type)
   {
      case RETRO_MEMORY_SYSTEM_RAM:
         return 0x10000;
      case RETRO_MEMORY_SAVE_RAM:
         return GPRAM_Mask + 1;
      default:
         return 0;
   }
}

void retro_cheat_reset(void)
{}

void retro_cheat_set(unsigned, bool, const char *)
{}

void MDFND_DispMessage(unsigned char *str)
{
   if (log_cb)
      log_cb(RETRO_LOG_INFO, "%s\n", str);
}

void MDFND_Message(const char *str)
{
   if (log_cb)
      log_cb(RETRO_LOG_INFO, "%s\n", str);
}

void MDFND_MidSync(const EmulateSpecStruct *)
{}

void MDFN_MidLineUpdate(EmulateSpecStruct *espec, int y)
{
 //MDFND_MidLineUpdate(espec, y);
}

void MDFND_PrintError(const char* err)
{
   if (log_cb)
      log_cb(RETRO_LOG_ERROR, "%s\n", err);
}
