#ifndef HUD_H
#define HUD_H
#include "fighter.h"

void hud_init(u8 char1, u8 char2);     /* loads the frames, icons and names of both fighters */
void hud_hide(void);
void hud_wins(u8 p1, u8 p2);
void hud_update(const Fighter *p1, const Fighter *p2, s16 timer);
void hud_message(u8 row, const char *text);
void hud_clear_row(u8 row);

#endif
