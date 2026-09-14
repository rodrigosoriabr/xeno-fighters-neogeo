/* Attract intro, title, options, select, VS, boss intro, win quotes, continue, endings, attract demo. */
#ifndef SCREENS_H
#define SCREENS_H
#include <ngdevkit/types.h>

u8 intro_story(void);                 /* 1 = cut short by coin/start */
void title_screen(void);
void options_screen(u8 player);       /* difficulty and game speed */
u8 select_screen(u8 player);          /* returns the character picked */
void vs_screen(u8 left, u8 right);
void boss_intro(void);
void win_screen(u8 winner, u8 loser);
u8 continue_screen(u8 character);     /* 1 = continue */
void ending_screen(u8 character);
void challenger_screen(void);
void demo_match(void);

/* screen images: load an image's palettes into a slot, then draw its frame k */
void screen_load(u8 img, u8 slot);
void screen_show(u16 block, u8 max_cols, u8 img, u8 k, s16 x, s16 y, u8 flip);

/* big arcade lettering (WORD_* in screens_data.h), centered on (x, y); used during fights too */
void word_show(u8 word, s16 y);
void word_zoom(u8 word, s16 x, s16 y, u8 t, u8 final_scale);
void word_hide(void);
void screens_load_palettes(void);

/* main.c */
u8 play_match(u8 char1, u8 char2, u8 cpu1, u8 cpu2);
extern u8 demo;
#endif
