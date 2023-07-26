#ifndef MDFN_SETTINGS_H
#define MDFN_SETTINGS_H

#include <string.h>

extern uint32_t setting_vb_lcolor;
extern uint32_t setting_vb_rcolor;
extern uint32_t setting_vb_anaglyph_preset;
extern bool setting_vb_right_analog_to_digital;
extern bool setting_vb_right_invert_x;
extern bool setting_vb_right_invert_y;

// This should assert() or something if the setting isn't found, since it would
// be a totally tubular error!
uint64 MDFN_GetSettingUI(const char *name);
int64 MDFN_GetSettingI(const char *name);
double MDFN_GetSettingF(const char *name);
bool MDFN_GetSettingB(const char *name);
#endif
