/* Fight HUD. Sprites (images drawn by tools/ui_art.py) give the frames, face icons and the name plates;
 * the fix layer on top fills the lifebars (with a red trail of recent damage, flashing red under 25%),
 * the timer, win marks, super stocks and the combo counter. Only cells whose value changed are rewritten. */
#include <ngdevkit/neogeo.h>
#include <ngdevkit/ng-fix.h>
#include "hud.h"
#include "hud_tiles.h"
#include "screens.h"
#include "screens_data.h"

#define BAR_CELLS 13          /* 104 px per bar */
#define BAR_ROW 3
#define POW_CELLS 10
#define POW_ROW 27

/* sprite blocks inside SPR_HUD */
#define H_FRAME SPR_HUD             /* 10 + 10 */
#define H_TIMER (SPR_HUD + 20)      /* 3 */
#define H_ICON (SPR_HUD + 23)       /* 2 + 2 */
#define H_NAME (SPR_HUD + 27)       /* 7 + 7 */
#define H_POWER (SPR_HUD + 41)      /* 8 + 8 */

/* HUD palettes, from PAL_HUD */
#define HP_FRAMES PAL_HUD
#define HP_NAMES (PAL_HUD + 4)
#define HP_ICON1 (PAL_HUD + 8)
#define HP_ICON2 (PAL_HUD + 12)

static s16 last_life[2], last_trail[2];
static u16 last_power[2];
static u8 last_combo[2], combo_time[2], last_bar_pal[2];
static s16 last_timer;

static u8 text_len(const char *s) {
    u8 n = 0;
    while (s[n]) n++;
    return n;
}

void hud_init(u8 char1, u8 char2) {
    set_palettes(0, fix_palettes, FIX_PALETTES);
    fix_clear();
    last_life[0] = last_life[1] = -1;
    last_trail[0] = last_trail[1] = -1;
    last_power[0] = last_power[1] = 0xffff;
    last_combo[0] = last_combo[1] = 0;
    last_bar_pal[0] = last_bar_pal[1] = 0;
    last_timer = -1;
    screen_load(IMG_HUD, HP_FRAMES);
    screen_load(IMG_NAMES, HP_NAMES);
    screen_load(IMG_ICON(char1), HP_ICON1);
    screen_load(IMG_ICON(char2), HP_ICON2);
    screen_show(H_FRAME, 10, IMG_HUD, HUD_LIFEBAR, 0, 0, 0);
    screen_show(H_FRAME + 10, 10, IMG_HUD, HUD_LIFEBAR, SCREEN_W, 0, 1);
    screen_show(H_TIMER, 3, IMG_HUD, HUD_TIMER, 136, 0, 0);
    /* the second icon uses its own palette slot: draw_frame takes the palette from slot_of[], so
     * the p2 icon is drawn with an explicit palette base */
    screen_show(H_ICON, 2, IMG_ICON(char1), 1, 4, 4, 0);
    draw_frame(H_ICON + 2, 2, &screen_images, screen_image[IMG_ICON(char2)].frame + 1, SCREEN_W - 4, 4, 1, HP_ICON2);
    screen_show(H_NAME, 7, IMG_NAMES, NAME_SMALL + char1, 42, 27, 0);
    const Frame *n2 = &screen_frames[screen_image[IMG_NAMES].frame + NAME_SMALL + char2];
    screen_show(H_NAME + 7, 7, IMG_NAMES, NAME_SMALL + char2, SCREEN_W - 42 - n2->hurt[2], 27, 0);
    screen_show(H_POWER, 8, IMG_HUD, HUD_POWER, 0, 180, 0);
    screen_show(H_POWER + 8, 8, IMG_HUD, HUD_POWER, SCREEN_W, 180, 1);
}

void hud_hide(void) {
    hide_sprites(SPR_HUD, SPR_HUD_COUNT);
}

static void draw_bar(u8 side, s16 life, s16 trail, s16 max, u8 pal) {
    s16 life_px = (s16)((s32)life * BAR_CELLS * 8 / max);
    s16 trail_px = (s16)((s32)trail * BAR_CELLS * 8 / max) - life_px;
    if (life > 0 && life_px == 0) life_px = 1;
    if (trail_px < 0) trail_px = 0;
    for (u8 c = 0; c < BAR_CELLS; c++) {
        /* distance of this cell from the inner (timer) side */
        s16 from_inner = (s16)c * 8;
        s16 l = life_px - from_inner;
        if (l < 0) l = 0;
        if (l > 8) l = 8;
        s16 t = life_px + trail_px - from_inner - l;
        if (t < 0) t = 0;
        if (t > 8 - l) t = 8 - l;
        u16 idx = bar_index[l] + t;
        u8 x = side == 0 ? (u8)(5 + BAR_CELLS - 1 - c) : (u8)(22 + c);
        fix_put(x, BAR_ROW, pal, (side == 0 ? FIX_BAR_L0 : FIX_BAR_R0) + idx);
        fix_put(x, BAR_ROW + 1, pal, (side == 0 ? FIX_BAR_L1 : FIX_BAR_R1) + idx);
    }
}

static void draw_digit(u8 x, u8 y, u8 pal, u8 d) {
    for (u8 ty = 0; ty < 3; ty++) {
        for (u8 tx = 0; tx < 2; tx++) {
            fix_put(x + tx, y + ty, pal, FIX_DIGIT + d * 6 + tx + ty * 2);
        }
    }
}

void hud_wins(u8 p1, u8 p2) {
    for (u8 i = 0; i < 2; i++) {
        fix_put((u8)(19 - i), BAR_ROW + 2, 2, i < p1 ? FIX_WIN : FIX_WIN_EMPTY);
        fix_put((u8)(20 + i), BAR_ROW + 2, 2, i < p2 ? FIX_WIN : FIX_WIN_EMPTY);
    }
}

void hud_update(const Fighter *p1, const Fighter *p2, s16 timer) {
    const Fighter *f[2] = {p1, p2};
    for (u8 s = 0; s < 2; s++) {
        u8 danger = f[s]->health * 4 < f[s]->max_health && f[s]->health > 0 && ((frame_count >> 4) & 1);
        u8 pal = danger ? 7 : 1;
        if (f[s]->shown_health != last_life[s] || f[s]->trail_health != last_trail[s] || pal != last_bar_pal[s]) {
            draw_bar(s, f[s]->shown_health, f[s]->trail_health, f[s]->max_health, pal);
            last_life[s] = f[s]->shown_health;
            last_trail[s] = f[s]->trail_health;
            last_bar_pal[s] = pal;
        }
        u16 power = f[s]->power;
        u8 full = power >= POWER_MAX;
        /* the MAX gauge shimmers: redraw every 8 frames */
        if (power != last_power[s] || (full && (frame_count & 7) == 0)) {
            u8 stocks = (u8)(power / POWER_STOCK);
            s16 px = full ? POW_CELLS * 8 : (s16)(power % POWER_STOCK) * POW_CELLS * 8 / POWER_STOCK;
            u8 gauge_pal = full ? 4 : 3;
            for (u8 c = 0; c < POW_CELLS; c++) {
                s16 fill = px - (s16)c * 8;
                if (fill < 0) fill = 0;
                if (fill > 8) fill = 8;
                if (s == 0) fix_put((u8)(4 + c), POW_ROW, gauge_pal, FIX_POW_L + fill);
                else fix_put((u8)(35 - c), POW_ROW, gauge_pal, FIX_POW_R + fill);
            }
            draw_digit(s == 0 ? 1 : 37, POW_ROW - 2, stocks ? 4 : 3, stocks);
            const char *label = full ? ((frame_count >> 4) & 1 ? "MAXIMUM" : "       ") : stocks ? "SUPER! " : "POWER  ";
            fix_text(s == 0 ? 4 : 29, POW_ROW - 2, stocks ? 5 : 6, label);
            last_power[s] = power;
        }
        /* combo counter on the attacker's side while the victim is in a combo */
        const Fighter *victim = f[1 - s];
        u8 cx = s == 0 ? 2 : 31;
        if (victim->combo >= 2 && victim->combo != last_combo[s]) {
            draw_digit(cx, 11, 2, victim->combo >= 10 ? victim->combo / 10 : 0);
            draw_digit(cx + 2, 11, 2, victim->combo % 10);
            ng_text_tall(cx + 4, 12, 5, "HITS");
            last_combo[s] = victim->combo;
            combo_time[s] = 60;
        }
        if (combo_time[s] && --combo_time[s] == 0) {
            for (u8 y = 11; y < 14; y++) fix_text(cx, y, 0, "        ");
            last_combo[s] = 0;
        }
    }
    if (timer != last_timer) {
        draw_digit(18, 2, 2, (u8)(timer / 10));
        draw_digit(20, 2, 2, (u8)(timer % 10));
        last_timer = timer;
    }
    /* MAX gauges shimmer: cycle one color of palette 4 */
    if ((frame_count & 7) == 0) {
        static const u16 shimmer[4] = {0x7fdf, 0x0f8f, 0x0f2c, 0x7fff};
        ((volatile u16 *)0x400000)[4 * 16 + 3] = shimmer[(frame_count >> 3) & 3];
    }
}

void hud_message(u8 row, const char *text) {
    fix_text((u8)(20 - text_len(text) / 2), row, 5, text);
}

void hud_clear_row(u8 row) {
    fix_text(1, row, 0, "                                      ");
}
