/* Attract intro, title, options, character select, VS, boss intro, win quotes, continue, endings and
 * the attract demo. Screen images keep their palettes in ROM and are loaded into palette slots per
 * screen (screen_load), because all of them together need far more palettes than the Neo Geo has. */
#include <ngdevkit/neogeo.h>
#include <ngdevkit/bios-ram.h>
#include <ngdevkit/ng-fix.h>
#include "screens.h"
#include "video.h"
#include "fighter.h"
#include "stage.h"
#include "hud.h"
#include "hud_tiles.h"
#include "sound.h"
#include "story.h"
#include "screens_data.h"

#define USER_MODE_GAME 2
/* sprite blocks inside SPR_SCREEN (121 sprites) */
#define BLOCK_BG SPR_SCREEN              /* 20 columns */
#define BLOCK_A (SPR_SCREEN + 20)        /* 14: logo, left portrait */
#define BLOCK_B (SPR_SCREEN + 34)        /* 14: right portrait */
#define BLOCK_ICONS (SPR_SCREEN + 48)    /* 8 x 3 */
#define BLOCK_NAME (SPR_SCREEN + 72)     /* 8 */
#define BLOCK_NAME2 (SPR_SCREEN + 80)    /* 8 */
#define BLOCK_CURSOR (SPR_SCREEN + 88)   /* 4 */
#define BLOCK_WORD (SPR_SCREEN + 92)     /* 16 */

/* palette slots used by the screens */
#define SLOT_BG 96
#define SLOT_A 112
#define SLOT_B 124
#define SLOT_BUST 144
#define SLOT_NAMES 156
#define SLOT_UI 160
#define SLOT_WORDS 184

extern u16 demo_frames;
static u8 slot_of[IMG_COUNT];

/* waits for the vblank; screens draw right after it (see frame_start() in main.c for why) */
static void frame(void) {
    wait_frame();
    sound_frame();
}

/* start, a game already started, or a coin going in (REG_STATUS_A bits 0-1, active low): the BIOS only
 * shows the title with credits once the attract sequence returns, so the long intro must end at once */
static u8 skip_pressed(void) {
    return (bios_statchange & 5) || bios_user_mode == USER_MODE_GAME || (*REG_STATUS_A & 3) != 3;
}

void screen_load(u8 img, u8 slot) {
    slot_of[img] = slot;
    set_palettes(slot, screen_palettes + screen_image[img].palette * 16, screen_image[img].palettes);
}

void screen_show(u16 block, u8 max_cols, u8 img, u8 k, s16 x, s16 y, u8 flip) {
    draw_frame(block, max_cols, &screen_images, screen_image[img].frame + k, x, y, flip, slot_of[img]);
}

static void dim_image(u8 img, u8 level) {
    fade_palettes(slot_of[img], screen_palettes + screen_image[img].palette * 16, screen_image[img].palettes, level);
}

static void flash_image(u8 img, u8 level) {
    flash_palettes(slot_of[img], screen_palettes + screen_image[img].palette * 16, screen_image[img].palettes, level);
}

static s16 frame_width(u8 img, u8 k) {
    return screen_frames[screen_image[img].frame + k].hurt[2];
}

void screens_load_palettes(void) {
    screen_load(IMG_WORDS, SLOT_WORDS);
    screen_load(IMG_NAMES, SLOT_NAMES);
}

void word_show(u8 word, s16 y) {
    unscale_sprites(BLOCK_WORD, 16);
    screen_show(BLOCK_WORD, 16, IMG_WORDS, word, SCREEN_W / 2, y, 0);
}

/* lettering growing from small to full size with the hardware zoom (t = frames since it appeared) */
void word_zoom(u8 word, s16 x, s16 y, u8 t, u8 final_scale) {
    u8 scale = (u8)(3 + t * 2);
    if (scale > final_scale) scale = final_scale;
    draw_frame_scaled(BLOCK_WORD, 16, &screen_images, screen_image[IMG_WORDS].frame + word, x, y, SLOT_WORDS, scale);
}

void word_hide(void) {
    hide_sprites(BLOCK_WORD, 16);
    unscale_sprites(BLOCK_WORD, 16);
}

static void clear_screen(void) {
    video_init();
    fix_clear();
    set_palettes(0, fix_palettes, FIX_PALETTES);
    screens_load_palettes();
}

static void hide_screen(void) {
    hide_sprites(SPR_SCREEN, SPR_SCREEN_COUNT);
    unscale_sprites(SPR_SCREEN, SPR_SCREEN_COUNT);
    fix_clear();
}

static u8 blink(u16 t) {
    return (t / 20) & 1;
}

static u8 len(const char *s) {
    u8 n = 0;
    while (s[n]) n++;
    return n;
}

/* letterbox band on the fix layer, rows r0..r1 */
static void band(u8 r0, u8 r1) {
    for (u8 y = r0; y <= r1; y++) {
        for (u8 x = 0; x < 40; x++) fix_put(x, y, 0, FIX_SOLID);
    }
}

static void clear_rows(u8 r0, u8 r1) {
    for (u8 y = r0; y <= r1; y++) fix_text(0, y, 0, "                                        ");
}

/* types `text` centered on row y, one character every 2 frames from frame `start` */
static void type_line(u8 y, u8 pal, const char *text, u16 t, u16 start) {
    if (t < start || (t - start) & 1) return;
    u16 n = (t - start) / 2;
    u8 l = len(text);
    if (n >= l) return;
    u8 x = (u8)(20 - l / 2);
    char ch = text[n];
    fix_put((u8)(x + n), y, pal, ch >= 32 && ch < 96 ? FIX_OPAQUE_FONT + ch - 32 : FIX_SOLID);
    if (ch != ' ' && (n & 3) == 0) sound_play(SFX_TYPE);
}

/* --------------------------------------------------------------------------------- intro story */

/* 4 illustrations with narration typed in a letterbox band. Returns 1 if a coin/start cut it short. */
u8 intro_story(void) {
    clear_screen();
    set_backdrop(0x0000);
    sound_music(MUSIC_TITLE_TRACK);
    for (u8 page = 0; page < INTRO_PAGES; page++) {
        fix_clear();
        screen_load(IMG_STORY(page), SLOT_BG);
        dim_image(IMG_STORY(page), 0);
        band(23, 29);
        sound_play(voice_story[page]);
        for (u16 t = 0; t < 420; t++) {
            frame();
            /* slow vertical drift: the painting settles into place */
            s16 drift = t < 64 ? (64 - (s16)t) / 8 : 0;
            screen_show(BLOCK_BG, 20, IMG_STORY(page), 0, 0, -drift, 0);
            if (t < 32) dim_image(IMG_STORY(page), (u8)(t / 2));
            if (t == 32) dim_image(IMG_STORY(page), 16);
            if (t >= 388) dim_image(IMG_STORY(page), (u8)((420 - t) / 2));
            for (u8 l = 0; l < 3; l++) type_line((u8)(24 + l * 2), 0, intro_text[page][l], t, (u16)(40 + l * 70));
            if (skip_pressed()) {
                hide_screen();
                return 1;
            }
        }
    }
    hide_screen();
    return 0;
}

/* ------------------------------------------------------------------------------------ title */

void title_screen(void) {
    clear_screen();
    set_backdrop(0x0000);
    screen_load(IMG_TITLE_BG, SLOT_BG);
    screen_load(IMG_LOGO, SLOT_A);
    sound_music(MUSIC_TITLE_TRACK);
    u16 t = 0;
    while (bios_user_mode != USER_MODE_GAME) {
        frame();
        screen_show(BLOCK_BG, 20, IMG_TITLE_BG, 0, 0, 0, 0);
        /* the logo zooms in with the hardware scaler, then a white flash, then it bobs */
        if (t < 28) {
            draw_frame_scaled(BLOCK_A, 20, &screen_images, screen_image[IMG_LOGO].frame, SCREEN_W / 2, 72, SLOT_A, (u8)(1 + t * 15 / 27));
        } else {
            if (t == 28) {
                unscale_sprites(BLOCK_A, 20);
                sound_play(SFX_SUPER);
                sound_play(VOICE_TITLE);
            }
            screen_show(BLOCK_A, 20, IMG_LOGO, 0, SCREEN_W / 2, 72 + (s16)((t >> 5) & 1), 0);
        }
        if (t >= 28 && t < 44) {
            flash_image(IMG_TITLE_BG, (u8)(16 - (t - 28)));
            flash_image(IMG_LOGO, (u8)(16 - (t - 28)));
        }
        if (t == 44) {
            dim_image(IMG_TITLE_BG, 16);
            dim_image(IMG_LOGO, 16);
        }
        if (t > 50) {
            ng_center_text(22, 0, blink(t) ? "                 " : (bios_mvs_flag ? "   INSERT COIN   " : "   PRESS START   "));
        }
        ng_center_text(27, 6, "(C) 2026 RODRIGO SORIA");
        t++;
        if (bios_mvs_flag == 0 && (bios_statchange & 1)) {
            bios_user_mode = USER_MODE_GAME;
            bios_player_mod1 = 1;
        }
    }
    hide_screen();
}

/* ---------------------------------------------------------------------------------- options */

void options_screen(u8 player) {
    static const char *const levels[3] = {"  EASY  ", " NORMAL ", "  HARD  "};
    static const char *const speeds[2] = {"STANDARD", " TURBO  "};
    clear_screen();
    set_backdrop(0x0000);
    screen_load(IMG_TITLE_BG, SLOT_BG);
    dim_image(IMG_TITLE_BG, 4);
    u8 row = 0, done = 0;
    u16 t = 0;
    while (!done) {
        frame();
        screen_show(BLOCK_BG, 20, IMG_TITLE_BG, 0, 0, 0, 0);
        screen_show(BLOCK_WORD, 16, IMG_NAMES, NAME_TITLE_OPTIONS, (SCREEN_W - frame_width(IMG_NAMES, NAME_TITLE_OPTIONS)) / 2, 22, 0);
        u8 change = player ? bios_p2change : bios_p1change;
        if (change & (CNT_UP | CNT_DOWN)) {
            row = (u8)((row + ((change & CNT_DOWN) ? 1 : 2)) % 3);
            sound_play(SFX_CURSOR);
        }
        if (change & (CNT_LEFT | CNT_RIGHT)) {
            if (row == 0) difficulty = (u8)((difficulty + ((change & CNT_RIGHT) ? 1 : 2)) % 3);
            if (row == 1) turbo ^= 1;
            sound_play(SFX_CURSOR);
        }
        if ((change & (CNT_A | CNT_B | CNT_C | CNT_D)) || (bios_statchange & 5) || t > 20 * 60) {
            if (row == 2 || t > 20 * 60 || (bios_statchange & 5)) done = 1;
            else row++;
            sound_play(SFX_SELECT);
        }
        u8 pulse = (t >> 3) & 1;
        fix_text(9, 11, row == 0 ? 5 : 0, "DIFFICULTY");
        fix_text(22, 11, row == 0 && pulse ? 5 : 6, levels[difficulty]);
        fix_text(9, 14, row == 1 ? 5 : 0, "GAME SPEED");
        fix_text(22, 14, row == 1 && pulse ? 5 : 6, speeds[turbo]);
        fix_text(21, 11, 0, row == 0 ? "<" : " ");
        fix_text(30, 11, 0, row == 0 ? ">" : " ");
        fix_text(21, 14, 0, row == 1 ? "<" : " ");
        fix_text(30, 14, 0, row == 1 ? ">" : " ");
        fix_text(16, 18, row == 2 && pulse ? 5 : 0, row == 2 ? "> START <" : "  START  ");
        fix_text(5, 22, 0, turbo ? "  TURBO: FAST ARCADE PACE     " : "STANDARD: CLASSIC, SLOWER PACE");
        t++;
    }
    hide_screen();
}

/* ----------------------------------------------------------------------------------- select */

/* 4x2 icon grid at the bottom; the bust, name, epithet and bio of the fighter under the cursor on top */
#define ICON_X0 56
#define ICON_PITCH 52
#define ICON_Y0 122
#define ICON_SLOT(i) ((u8)(112 + (i) * 4))

static s16 icon_x(u8 i) { return ICON_X0 + (i & 3) * ICON_PITCH; }
static s16 icon_y(u8 i) { return ICON_Y0 + (i >> 2) * ICON_PITCH; }

u8 select_screen(u8 player) {
    clear_screen();
    set_backdrop(0x0101);
    screen_load(IMG_TITLE_BG, SLOT_BG);
    dim_image(IMG_TITLE_BG, 5);
    screen_load(IMG_HUD, SLOT_UI);
    for (u8 i = 0; i < PLAYABLE_COUNT; i++) screen_load(IMG_ICON(i), ICON_SLOT(i));
    sound_play(VOICE_CHOOSE);
    sound_music(MUSIC_SELECT_TRACK);

    u8 cursor = player ? 3 : 0, chosen = 255, shown = 255;
    u16 t = 0, timeout = 20 * 60, chosen_time = 0, moved_at = 0;
    for (;;) {
        frame();
        u8 change = player ? bios_p2change : bios_p1change;
        if (chosen == 255) {
            u8 before = cursor;
            if (change & CNT_LEFT) cursor = (u8)((cursor & 4) | ((cursor + 3) & 3));
            if (change & CNT_RIGHT) cursor = (u8)((cursor & 4) | ((cursor + 1) & 3));
            if (change & (CNT_UP | CNT_DOWN)) cursor ^= 4;
            if (before != cursor) {
                sound_play(SFX_CURSOR);
                moved_at = t;
            }
            if ((change & (CNT_A | CNT_B | CNT_C | CNT_D)) || t >= timeout) {
                chosen = cursor;
                chosen_time = 0;
                sound_play(SFX_SELECT);
                sound_play(voice_name[chosen]);
            }
        } else if (++chosen_time > 90) {
            break;
        }
        if (shown != cursor) {
            screen_load(IMG_PORTRAIT(cursor), SLOT_BUST);
            for (u8 i = 0; i < PLAYABLE_COUNT; i++) {
                if (i != cursor) fade_palettes(ICON_SLOT(i), screen_palettes + screen_image[IMG_ICON(i)].palette * 16, 4, 8);
                else set_palettes(ICON_SLOT(i), screen_palettes + screen_image[IMG_ICON(i)].palette * 16, 4);
            }
            clear_rows(10, 16);
            fix_text(20, 11, 5, epithet[cursor]);
            for (u8 l = 0; l < 3; l++) fix_text(20, (u8)(13 + l), 0, bio[cursor][l]);
            ng_text_tall(1, 26, 6, player ? "2P" : "1P");
            shown = cursor;
        }
        screen_show(BLOCK_BG, 20, IMG_TITLE_BG, 0, 0, 0, 0);
        s16 slide = t - moved_at < 8 ? (8 - (s16)(t - moved_at)) * 12 : 0;
        draw_frame(BLOCK_A, 12, &screen_images, screen_image[IMG_PORTRAIT(cursor)].frame + 1, 2 - slide, -8, 0, SLOT_BUST);
        screen_show(BLOCK_NAME, 8, IMG_NAMES, NAME_BIG + cursor, 158 + slide / 2, 34, 0);
        screen_show(BLOCK_WORD, 16, IMG_NAMES, NAME_TITLE_SELECT, (SCREEN_W - frame_width(IMG_NAMES, NAME_TITLE_SELECT)) / 2, 4, 0);
        for (u8 i = 0; i < PLAYABLE_COUNT; i++) {
            s16 bump = (i == cursor && chosen == 255) ? (s16)((t >> 4) & 1) * 2 : 0;
            draw_frame(BLOCK_ICONS + i * 3, 3, &screen_images, screen_image[IMG_ICON(i)].frame, icon_x(i), icon_y(i) - bump, 0, ICON_SLOT(i));
        }
        /* cursor brackets bob; they flash once the fighter is chosen */
        if (chosen == 255 || (chosen_time & 4)) {
            screen_show(BLOCK_CURSOR, 4, IMG_HUD, HUD_CURSOR, icon_x(cursor) - 4, icon_y(cursor) - 4 - (s16)((t >> 4) & 1), 0);
        } else {
            hide_sprites(BLOCK_CURSOR, 4);
        }
        fix_text(20, 17, 5, chosen != 255 && (chosen_time & 8) ? "READY!" : "      ");
        if (chosen == 255) {
            char secs[3] = {(char)('0' + (timeout - t) / 600 % 10), (char)('0' + (timeout - t) / 60 % 10), 0};
            ng_text_tall(36, 5, 5, secs);
        }
        t++;
    }
    hide_screen();
    return chosen;
}

/* ------------------------------------------------------------------------------------ versus */

void vs_screen(u8 left, u8 right) {
    clear_screen();
    set_backdrop(0x0300);
    screen_load(IMG_TITLE_BG, SLOT_BG);
    dim_image(IMG_TITLE_BG, 3);
    screen_load(IMG_PORTRAIT(left), SLOT_A);
    screen_load(IMG_PORTRAIT(right), SLOT_B);
    sound_play(SFX_VS);
    sound_music(MUSIC_STOP);
    for (u16 t = 0; t < 170; t++) {
        frame();
        screen_show(BLOCK_BG, 20, IMG_TITLE_BG, 0, 0, 0, 0);
        s16 slide = t < 16 ? (16 - (s16)t) * 12 : 0;
        screen_show(BLOCK_A, 14, IMG_PORTRAIT(left), 0, -slide, 0, 0);
        screen_show(BLOCK_B, 14, IMG_PORTRAIT(right), 0, SCREEN_W + slide, 0, 1);
        /* VS slams in with the hardware zoom */
        if (t >= 18) {
            u8 scale = t < 26 ? (u8)(4 + (t - 18) * 3 / 2) : 16;
            s16 w = frame_width(IMG_NAMES, NAME_VS);
            draw_frame_scaled(BLOCK_WORD, 8, &screen_images, screen_image[IMG_NAMES].frame + NAME_VS,
                              SCREEN_W / 2 - (w * scale >> 5), 80, SLOT_NAMES, scale);
            if (t == 26) {
                flash_image(IMG_TITLE_BG, 12);
                sound_play(SFX_KO_BOOM);
            }
            if (t == 34) dim_image(IMG_TITLE_BG, 3);
        }
        if (t == 30) sound_play(voice_name[left]);
        if (t == 80) sound_play(voice_name[right]);
        s16 name_slide = t < 30 ? (30 - (s16)t) * 8 : 0;
        if (t >= 14) {
            screen_show(BLOCK_NAME, 8, IMG_NAMES, NAME_BIG + left, 8 - name_slide, 184, 0);
            screen_show(BLOCK_NAME2, 8, IMG_NAMES, NAME_BIG + right, SCREEN_W - 8 - frame_width(IMG_NAMES, NAME_BIG + right) + name_slide, 184, 0);
        }
    }
    hide_screen();
}

/* WARNING before the boss: red pulses and a siren, then the Overmind speaks. */
void boss_intro(void) {
    clear_screen();
    set_backdrop(0x0000);
    sound_music(MUSIC_STOP);
    for (u16 t = 0; t < 150; t++) {
        frame();
        u8 pulse = (t >> 3) & 3;
        set_backdrop(pulse == 0 ? 0x0800 : pulse == 1 ? 0x0400 : 0x0000);
        if ((t & 63) == 0) sound_play(SFX_WARNING);
        if ((t >> 4) & 1) word_show(WORD_WARNING, 90);
        else word_hide();
        if (t == 20) ng_center_text(16, 5, "A MIGHTY FOE APPROACHES");
    }
    word_hide();
    set_backdrop(0x0000);
    fix_clear();
    screen_load(IMG_TITLE_BG, SLOT_BG);
    dim_image(IMG_TITLE_BG, 2);
    screen_load(IMG_PORTRAIT(CHAR_XAL), SLOT_B);
    band(23, 29);
    sound_music(MUSIC_BOSS_TRACK);
    sound_play(VOICE_XAL_INTRO);
    for (u8 page = 0; page < BOSS_PAGES; page++) {
        for (u16 t = 0; t < 200; t++) {
            frame();
            screen_show(BLOCK_BG, 20, IMG_TITLE_BG, 0, 0, 0, 0);
            s16 slide = page == 0 && t < 24 ? (24 - (s16)t) * 8 : 0;
            screen_show(BLOCK_B, 14, IMG_PORTRAIT(CHAR_XAL), 0, SCREEN_W + slide, 0, 1);
            screen_show(BLOCK_NAME, 8, IMG_NAMES, NAME_BIG + CHAR_XAL, 16, 32, 0);
            if (t == 20 && page == 0) fix_text(2, 9, 5, epithet[CHAR_XAL]);
            for (u8 l = 0; l < 2; l++) type_line((u8)(24 + l * 3), 0, boss_text[page][l], t, (u16)(30 + l * 60));
            if (t > 60 && ((bios_p1change | bios_p2change) & (CNT_A | CNT_B | CNT_C | CNT_D))) break;
        }
        clear_rows(23, 29);
        band(23, 29);
    }
    hide_screen();
}

/* the winner's portrait and win quote */
void win_screen(u8 winner, u8 loser) {
    (void)loser;
    clear_screen();
    set_backdrop(0x0000);
    screen_load(IMG_TITLE_BG, SLOT_BG);
    dim_image(IMG_TITLE_BG, 3);
    screen_load(IMG_PORTRAIT(winner), SLOT_A);
    band(23, 29);
    sound_play(VOICE_YOUWIN);
    for (u16 t = 0; t < 240; t++) {
        frame();
        screen_show(BLOCK_BG, 20, IMG_TITLE_BG, 0, 0, 0, 0);
        s16 slide = t < 16 ? (16 - (s16)t) * 10 : 0;
        screen_show(BLOCK_A, 14, IMG_PORTRAIT(winner), 0, 8 - slide, 0, 0);
        word_zoom(WORD_YOU_WIN, 226, 56, (u8)(t > 12 ? 12 : t), 16);
        screen_show(BLOCK_NAME, 8, IMG_NAMES, NAME_BIG + winner, 170, 104, 0);
        for (u8 l = 0; l < 2; l++) type_line((u8)(24 + l * 3), 0, win_quote[winner][l], t, (u16)(30 + l * 50));
        if (t > 120 && ((bios_p1change | bios_p2change) & (CNT_A | CNT_B | CNT_C | CNT_D))) break;
    }
    word_hide();
    hide_screen();
}

u8 continue_screen(u8 character) {
    clear_screen();
    set_backdrop(0x0000);
    screen_load(IMG_PORTRAIT(character), SLOT_A);
    sound_play(VOICE_YOULOSE);
    sound_music(MUSIC_STOP);
    dim_image(IMG_PORTRAIT(character), 7);
    for (u16 t = 0; t < 60; t++) {
        frame();
        screen_show(BLOCK_A, 14, IMG_PORTRAIT(character), 0, 85, 0, 0);
        word_zoom(WORD_YOU_LOSE, SCREEN_W / 2, 40, (u8)t, 16);
    }
    word_hide();
    sound_play(VOICE_CONTINUE);
    for (s16 count = 9; count >= 0; count--) {
        char digit[2] = {(char)('0' + count), 0};
        ng_center_text_tall(16, 5, digit);
        sound_play(SFX_COUNT);
        /* the portrait darkens as the count runs out */
        dim_image(IMG_PORTRAIT(character), (u8)(1 + count * 2 / 3));
        for (u8 t = 0; t < 60; t++) {
            frame();
            screen_show(BLOCK_A, 14, IMG_PORTRAIT(character), 0, 85, 0, 0);
            word_show(WORD_CONTINUE, 80);
            u8 pressed = bios_statchange & 5;
            u8 buttons = (bios_p1change | bios_p2change) & (CNT_A | CNT_B | CNT_C | CNT_D);
            if (pressed || (buttons && bios_mvs_flag == 0)) {
                hide_screen();
                return 1;
            }
            if (buttons && t > 10) break;      /* buttons speed up the count */
        }
    }
    fix_clear();
    hide_sprites(BLOCK_A, 14);
    sound_play(VOICE_GAMEOVER);
    for (u16 t = 0; t < 180; t++) {
        frame();
        word_zoom(WORD_GAME_OVER, SCREEN_W / 2, 100, (u8)(t > 20 ? 20 : t), 16);
    }
    hide_screen();
    return 0;
}

/* Ending illustration with 3 pages of text, then the cast roll and THE END. */
void ending_screen(u8 character) {
    clear_screen();
    set_backdrop(0x0000);
    sound_music(MUSIC_ENDING_TRACK);
    screen_load(IMG_END(character), SLOT_BG);
    dim_image(IMG_END(character), 0);
    band(23, 29);
    for (u8 page = 0; page < ENDING_PAGES; page++) {
        for (u16 t = 0; t < 360; t++) {
            frame();
            screen_show(BLOCK_BG, 20, IMG_END(character), 0, 0, 0, 0);
            if (page == 0 && t < 48) dim_image(IMG_END(character), (u8)(t / 3));
            for (u8 l = 0; l < 2; l++) type_line((u8)(24 + l * 3), 0, ending_text[character][page][l], t, (u16)(20 + l * 70));
            if (t > 200 && ((bios_p1change | bios_p2change) & (CNT_A | CNT_B | CNT_C | CNT_D))) break;
        }
        clear_rows(23, 29);
        band(23, 29);
    }
    for (u8 t = 0; t < 48; t++) {
        frame();
        dim_image(IMG_END(character), (u8)(16 - t / 3));
    }
    hide_screen();

    /* cast roll: every fighter slides by with name and epithet */
    clear_screen();
    set_backdrop(0x0000);
    screen_load(IMG_TITLE_BG, SLOT_BG);
    dim_image(IMG_TITLE_BG, 3);
    for (u8 c = 0; c < CHARACTER_COUNT; c++) {
        screen_load(IMG_PORTRAIT(c), SLOT_A);
        u8 right = c & 1;
        fix_clear();
        for (u16 t = 0; t < 150; t++) {
            frame();
            screen_show(BLOCK_BG, 20, IMG_TITLE_BG, 0, 0, 0, 0);
            s16 slide = t < 20 ? (20 - (s16)t) * 10 : t > 130 ? ((s16)t - 130) * 10 : 0;
            if (right) screen_show(BLOCK_A, 14, IMG_PORTRAIT(c), 0, SCREEN_W + slide, 0, 1);
            else screen_show(BLOCK_A, 14, IMG_PORTRAIT(c), 0, -slide, 0, 0);
            s16 nx = right ? 16 : SCREEN_W - 16 - frame_width(IMG_NAMES, NAME_BIG + c);
            screen_show(BLOCK_NAME, 8, IMG_NAMES, NAME_BIG + c, nx, 96, 0);
            if (t == 24) fix_text(right ? 2 : (u8)(38 - len(epithet[c])), 17, 5, epithet[c]);
        }
    }
    fix_clear();
    hide_sprites(BLOCK_A, 14);
    hide_sprites(BLOCK_NAME, 8);
    for (u16 t = 0; t < 600; t++) {
        frame();
        screen_show(BLOCK_BG, 20, IMG_TITLE_BG, 0, 0, 0, 0);
        word_zoom(WORD_THE_END, SCREEN_W / 2, 90, (u8)(t > 30 ? 30 : t), 16);
        if (t == 90) ng_center_text(20, 5, "THANK YOU FOR PLAYING");
        if (t == 150) ng_center_text(23, 0, "XENO FIGHTERS (C) 2026 RODRIGO SORIA");
        if (t > 240 && ((bios_p1change | bios_p2change) & (CNT_A | CNT_B | CNT_C | CNT_D))) break;
    }
    hide_screen();
}

/* HERE COMES A NEW CHALLENGER, before the versus match */
void challenger_screen(void) {
    clear_screen();
    set_backdrop(0x0000);
    for (u16 t = 0; t < 150; t++) {
        frame();
        set_backdrop((t >> 3) & 1 ? 0x0006 : 0x0000);
        word_zoom(WORD_NEW_CHALLENGER, SCREEN_W / 2, 100, (u8)(t > 20 ? 20 : t), 16);
        if (t == 10) sound_play(SFX_WARNING);
    }
    set_backdrop(0x0000);
    hide_screen();
}

void demo_match(void) {
    demo = 1;
    demo_frames = 0;
    u8 a = (u8)(frame_count % PLAYABLE_COUNT);
    u8 b = (u8)((a + 1 + frame_count / 7 % (PLAYABLE_COUNT - 1)) % PLAYABLE_COUNT);
    play_match(a, b, 5, 5);
    demo = 0;
}
