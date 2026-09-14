#ifndef STORY_H
#define STORY_H
#include "fighter.h"

#define INTRO_PAGES 4
#define BOSS_PAGES 2
#define ENDING_PAGES 3

extern const char *const intro_text[INTRO_PAGES][3];
extern const char *const epithet[CHARACTER_COUNT];
extern const char *const bio[CHARACTER_COUNT][3];
extern const char *const win_quote[CHARACTER_COUNT][2];
extern const char *const boss_text[BOSS_PAGES][2];
extern const char *const ending_text[PLAYABLE_COUNT][ENDING_PAGES][2];

#endif
