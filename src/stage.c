#include <ngdevkit/neogeo.h>
#include "stage.h"
#include "fighter.h"
#include "fx_anims.h"
#include "fx.h"
#include "crowd_data.h"
#include "tile_bases.h"

#define COLUMNS 21
#define SPECTATORS 12
#define PARTICLES SPR_PARTICLE_COUNT

s16 stage_width;
s16 camera_x, camera_y;

static const Stage *stage;
static ImageSet images;
static const ImageSet crowd_images = {crowd_frames, crowd_cells, crowd_attrs, TILE_BASE_CROWD};
static s16 first_column[3];
static s16 offset_y;       /* stage row shown at screen line 0 when camera_y = 0 */
static u8 crowd_bob;
static u8 excite;

typedef struct { s16 x; u8 kind, flip, phase; } Spectator;
static Spectator spectators[SPECTATORS];

typedef struct { s16 x, y; u8 phase; } Particle;
static Particle particles[PARTICLES];
static u16 seed;

static u16 rnd(void) {
    seed = seed * 25173 + 13849;
    return seed >> 4;
}

void stage_load(const Stage *s, u8 unused) {
    (void)unused;
    stage = s;
    images = s->images;
    stage_width = s->width;
    offset_y = s->ground - GROUND_Y;
    set_palettes(PAL_STAGE, s->palettes, s->palette_count);
    if (s->crowd != NO_CROWD) set_palettes(PAL_CROWD, crowd_palettes + s->crowd * CROWD_PALETTES * 16, CROWD_PALETTES);
    set_backdrop(s->backdrop);
    for (u8 b = 0; b < 3; b++) first_column[b] = -1;
    camera_x = (s->width - SCREEN_W) / 2;
    camera_y = 0;
    excite = 0;
    seed = 0x5eed + frame_count;
    /* spectators in small groups with gaps, random kinds and facing */
    s16 x = 20;
    for (u8 i = 0; i < SPECTATORS; i++) {
        spectators[i].x = x;
        spectators[i].kind = (u8)(rnd() % 4);
        spectators[i].flip = (u8)(rnd() & 1);
        spectators[i].phase = (u8)rnd();
        x += 28 + (s16)(rnd() % 14) + ((i % 3) == 2 ? 26 : 0);
    }
    for (u8 i = 0; i < PARTICLES; i++) {
        particles[i].x = (s16)(rnd() % SCREEN_W);
        particles[i].y = (s16)(rnd() % SCREEN_H);
        particles[i].phase = (u8)rnd();
    }
}

void stage_hide(void) {
    hide_sprites(SPR_STAGE, COLUMNS * 3);
    hide_sprites(SPR_CROWD, SPR_CROWD_COUNT);
    hide_sprites(SPR_PARTICLE, SPR_PARTICLE_COUNT);
    stage = 0;
}

static void write_band_tiles(u8 b, s16 first) {
    const Frame *fr = &images.frames[stage->bands[b].frame];
    const u16 *cells = images.cells + fr->cell;
    const u8 *attrs = images.attrs + fr->cell;
    for (u8 k = 0; k < COLUMNS; k++) {
        s16 col = first + k;
        if (col >= fr->cols) col = fr->cols - 1;
        *REG_VRAMADDR = ADDR_SCB1 + (SPR_STAGE + b * COLUMNS + k) * 64;
        *REG_VRAMMOD = 1;
        for (u8 r = 0; r < fr->rows; r++) {
            u16 cell = cells[col * fr->rows + r];
            u8 attr = attrs[col * fr->rows + r];
            u32 tile = cell ? images.tile_base + cell : TILE_EMPTY;
            *REG_VRAMRW = (u16)tile;
            *REG_VRAMRW = (u16)(PAL_STAGE + (attr & 15)) << 8 | ((tile >> 16) & 15) << 4 | ((attr >> 4) & 1);
        }
    }
}

void stage_draw(void) {
    if (!stage) return;
    for (u8 b = 0; b < 3; b++) {
        const Band *band = &stage->bands[b];
        const Frame *fr = &images.frames[band->frame];
        s16 scroll = (s16)((s32)camera_x * band->speed / 16);
        s16 first = scroll >> 4;
        if (first != first_column[b]) {
            write_band_tiles(b, first);
            first_column[b] = first;
        }
        s16 y = band->y - offset_y - camera_y;
        if (b == 1 && crowd_bob) y -= (frame_count >> 3) & 1;
        for (u8 k = 0; k < COLUMNS; k++) {
            *REG_VRAMADDR = ADDR_SCB3 + SPR_STAGE + b * COLUMNS + k;
            *REG_VRAMMOD = 0x200;
            *REG_VRAMRW = SCB3_Y(y) | fr->rows;
            *REG_VRAMRW = (u16)((k * 16 - (scroll & 15)) & 0x1ff) << 7;
        }
    }
}

/* spectators (behind the fighters: SPR_CROWD is a lower block) and particles (in front) */
void stage_draw_front(s16 cx, s16 cy) {
    if (!stage) return;
    u16 block = SPR_CROWD;
    /* spectators stand at the painted barrier and scroll with the middle band, so they stay glued to it
     * (on the fight floor they looked like toys next to the fighters) */
    s16 feet = stage->crowd_feet - offset_y - cy;
    u8 speed = stage->bands[1].speed;
    for (u8 i = 0; stage->crowd != NO_CROWD && i < SPECTATORS && block + 3 <= SPR_CROWD + SPR_CROWD_COUNT; i++) {
        Spectator *s = &spectators[i];
        s16 x = s->x - (s16)((s32)cx * speed / 16);
        if (x < -24 || x > SCREEN_W + 24) continue;
        u8 t = (u8)(frame_count + s->phase);
        u8 cheer = excite && ((t >> 3) & 1);
        s16 hop = excite ? -(s16)(((t >> 2) & 3) == 1 ? 3 : ((t >> 2) & 3) == 2 ? 1 : 0) : -(s16)(((t >> 5) & 1));
        draw_frame(block, 3, &crowd_images, (u16)(stage->crowd * CROWD_FRAMES_PER_STAGE + s->kind + (cheer ? 4 : 0)),
                   x, feet + hop, s->flip, PAL_CROWD);
        block += 3;
    }
    if (block < SPR_CROWD + SPR_CROWD_COUNT) hide_sprites(block, SPR_CROWD + SPR_CROWD_COUNT - block);

    for (u8 i = 0; i < PARTICLES; i++) {
        Particle *p = &particles[i];
        u8 show = 1;
        if (stage->motion == MOTION_TWINKLE) show = ((frame_count + p->phase) & 63) < 40;
        if (!show) {
            hide_sprites(SPR_PARTICLE + i, 1);
            continue;
        }
        fx_draw_single(SPR_PARTICLE + i, stage->particle, p->x, p->y);
    }
}

void stage_update(void) {
    if (!stage) return;
    if (excite) excite--;
    /* glow: the brightest saturated colors breathe over ~1 second */
    if (stage->glow_count && (frame_count & 3) == 0) {
        u8 k = (u8)(frame_count >> 2) & 15;
        u8 level = (u8)(k < 8 ? 10 + k * 3 / 4 : 16 - (k - 8) * 3 / 4);
        for (u8 i = 0; i < stage->glow_count; i++) {
            u16 idx = stage->glow[i];
            u16 c = stage->palettes[idx];
            u16 r = ((c >> 8) & 15) * level >> 4, g = ((c >> 4) & 15) * level >> 4, b = (c & 15) * level >> 4;
            ((volatile u16 *)0x400000)[PAL_STAGE * 16 + idx] = r << 8 | g << 4 | b;
        }
    }
    for (u8 i = 0; i < PARTICLES; i++) {
        Particle *p = &particles[i];
        u8 t = (u8)(frame_count + p->phase);
        switch (stage->motion) {
        case MOTION_RISE:
            if ((t & 1) == 0) p->y--;
            if ((t & 31) == 0) p->x += (t & 32) ? 1 : -1;
            break;
        case MOTION_FALL:
            p->y += stage->particle == FXA_P_RAIN ? 6 : 1;
            if (stage->particle == FXA_P_RAIN) p->x -= 2;
            else if ((t & 7) == 0) p->x += (t & 64) ? 1 : -1;
            break;
        case MOTION_DRIFT:
            if ((t & 3) == 0) p->x += (t & 128) ? 1 : -1;
            if ((t & 7) == 0) p->y += (t & 64) ? 1 : -1;
            break;
        default:
            break;
        }
        if (p->y < -8) { p->y = SCREEN_H + 4; p->x = (s16)(rnd() % SCREEN_W); }
        if (p->y > SCREEN_H + 8) { p->y = -4; p->x = (s16)(rnd() % SCREEN_W); }
        if (p->x < -8) p->x = SCREEN_W + 4;
        if (p->x > SCREEN_W + 8) p->x = -4;
    }
}

void stage_camera(s16 x1, s16 x2, s16 height) {
    s16 target = (x1 + x2) / 2 - SCREEN_W / 2;
    if (target < 0) target = 0;
    if (target > stage_width - SCREEN_W) target = stage_width - SCREEN_W;
    camera_x += (target - camera_x) / 4;
    s16 up = height > 60 ? -(height - 60) / 2 : 0;
    if (up < -offset_y) up = -offset_y;
    camera_y += (up - camera_y) / 4;
    crowd_bob = shake > 0 || hitstop > 0;
}

void crowd_cheer(u8 frames) {
    if (frames > excite) excite = frames;
}
