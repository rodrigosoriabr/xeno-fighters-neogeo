#include "fx.h"
#include "sound.h"
#include "fx_data.h"    /* generated: fx_frames, fx_cells, fx_attrs, fx_anims[], fx_palettes */
#include "tile_bases.h"

#define POOL 10
#define COLS 4

extern s16 stage_width;

static const ImageSet images = {fx_frames, fx_cells, fx_attrs, TILE_BASE_FX};

typedef struct {
    u8 active, anim, frame, time, flip;
    s16 x, y;                   /* world position of the anchor */
    Fighter *owner;             /* projectiles only */
    const Move *move;
    s8 vx;
} Effect;

static Effect pool[POOL];

void fx_reset(void) {
    for (u8 i = 0; i < POOL; i++) pool[i].active = 0;
    set_palettes(PAL_FX, fx_palettes, FX_PALETTES);
    hide_sprites(SPR_FX, SPR_FX_COUNT);
}

static Effect *spawn(u8 anim, s16 x, s16 y) {
    for (u8 i = 0; i < POOL; i++) {
        if (!pool[i].active) {
            Effect *e = &pool[i];
            e->active = 1;
            e->anim = anim;
            e->frame = e->time = e->flip = 0;
            e->x = x;
            e->y = y;
            e->owner = 0;
            e->move = 0;
            e->vx = 0;
            return e;
        }
    }
    return 0;
}

void fx_spark(s16 x, s16 y, u8 kind) {
    spawn(kind, x, y);
}

void fx_projectile(Fighter *owner, const Move *m) {
    s16 x = (s16)(owner->x >> 8) + (owner->facing_right ? 44 : -44);
    Effect *e = spawn(m->projectile, x, GROUND_Y - (m->kind == MOVE_BEAM ? 64 : 72));
    if (!e) return;
    e->owner = owner;
    e->move = m;
    e->flip = !owner->facing_right;
    e->vx = owner->facing_right ? m->speed_x : -m->speed_x;
    sound_play(SFX_PROJECTILE);
}

u8 fx_threatens(const Fighter *f) {
    for (u8 i = 0; i < POOL; i++) {
        const Effect *e = &pool[i];
        if (e->active && e->owner && e->owner != f) {
            s16 d = e->x - (s16)(f->x >> 8);
            if ((e->vx > 0 && d < 0 && d > -140) || (e->vx < 0 && d > 0 && d < 140)) return 1;
        }
    }
    return 0;
}

static void bounds(const Effect *e, s16 *box) {
    const Frame *fr = &fx_frames[fx_anims[e->anim].first + e->frame];
    box[0] = e->x + fr->x;
    box[2] = e->x + fr->x + fr->cols * 16;
    box[1] = e->y + fr->y;
    box[3] = e->y;
}

void fx_update(Fighter *p1, Fighter *p2) {
    for (u8 i = 0; i < POOL; i++) {
        Effect *e = &pool[i];
        if (!e->active) continue;
        const FxAnim *a = &fx_anims[e->anim];
        if (++e->time >= a->duration) {
            e->time = 0;
            if (++e->frame >= a->count) {
                if (e->owner) {
                    e->frame = 0;           /* projectiles loop */
                } else {
                    e->active = 0;
                    continue;
                }
            }
        }
        if (!e->owner) continue;

        e->x += e->vx;
        if (e->x < -64 || e->x > stage_width + 64) {
            e->active = 0;
            continue;
        }
        s16 box[4];
        bounds(e, box);
        /* two projectiles cancel each other */
        for (u8 j = 0; j < POOL; j++) {
            Effect *o = &pool[j];
            if (j != i && o->active && o->owner && o->owner != e->owner) {
                s16 ob[4];
                bounds(o, ob);
                if (box[0] < ob[2] && box[2] > ob[0]) {
                    e->active = o->active = 0;
                    fx_spark((e->x + o->x) / 2, e->y, FXA_HEAVY);
                    sound_play(SFX_HIT_HEAVY);
                }
            }
        }
        if (!e->active) continue;
        Fighter *target = e->owner == p1 ? p2 : p1;
        const Frame *tf = &target->ch->images.frames[target->ch->steps[target->ch->anims[target->anim].first + target->step].frame];
        s16 tx = (s16)(target->x >> 8), ty = GROUND_Y - (s16)(target->y >> 8);
        s16 hurt0 = target->facing_right ? tx + tf->hurt[0] : tx - tf->hurt[2];
        s16 hurt2 = target->facing_right ? tx + tf->hurt[2] : tx - tf->hurt[0];
        if (box[0] < hurt2 && box[2] > hurt0 && box[1] < ty && box[3] > ty + tf->hurt[1]) {
            fighter_take_projectile(target, e->move, e->x);
            fx_spark(e->vx > 0 ? box[2] - 16 : box[0] + 16, e->y - 8, FXA_HEAVY);
            if (e->move->kind != MOVE_BEAM) e->active = 0;
            else e->owner = 0;              /* a beam finishes its animation without hitting again */
        }
    }
}

/* one frame of an effect animation outside the pool (stage particles) */
void fx_draw_single(u16 sprite, u8 anim, s16 x, s16 y) {
    draw_frame(sprite, 1, &images, fx_anims[anim].first, x, y, 0, PAL_FX);
}

void fx_draw(s16 camera_x, s16 camera_y) {
    for (u8 i = 0; i < POOL; i++) {
        Effect *e = &pool[i];
        u16 first = SPR_FX + i * COLS;
        if (!e->active) {
            hide_sprites(first, COLS);
            continue;
        }
        draw_frame(first, COLS, &images, fx_anims[e->anim].first + e->frame, e->x - camera_x, e->y - camera_y, e->flip, PAL_FX);
    }
}
