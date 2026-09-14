/* CPU opponent. Every frame it reads the situation (distance, what the opponent is doing, its own
 * state), keeps a short plan and "presses" joystick bits like a player, so it goes through the same
 * input rules (motions, buffers, cancels). `cpu` level 1..8 scales reaction time, how often it blocks,
 * punishes and uses specials.
 *
 * Reactions wait `9 - level` frames after the opponent commits (a level 1 cpu sees a jab only after it
 * already landed), so EASY is beatable and HARD still reacts like a strong player.
 *
 * Soak test before this version (random button mashing vs cpu level 2-3): the cpu lost 12 of 13 rounds;
 * it walked into attacks and only blocked after being hit. */
#include "fighter.h"

#define RIGHT 8
#define LEFT 4
#define UP 1
#define DOWN 2
#define BA (1 << 4)
#define BB (1 << 5)
#define BC (1 << 6)
#define BD (1 << 7)

enum { PLAN_IDLE, PLAN_APPROACH, PLAN_RETREAT, PLAN_POKE, PLAN_JUMP_IN, PLAN_FIREBALL, PLAN_ANTI_AIR,
       PLAN_BLOCK, PLAN_SWEEP, PLAN_COMBO, PLAN_SUPER, PLAN_THROW, PLAN_WAIT };

typedef struct {
    u8 plan;
    u8 time;
    u8 block_low;
    u16 seed;
} Brain;

static Brain brains[2];

static u8 rnd(Brain *b) {
    b->seed = b->seed * 25173 + 13849;
    return (u8)(b->seed >> 8);
}

/* percent chance scaled by level: base at level 1, +step per level */
static u8 chance(Brain *b, const Fighter *f, u8 base, u8 step) {
    u16 p = base + step * (f->cpu - 1);
    if (p > 95) p = 95;
    return (u16)rnd(b) * 100 / 256 < p;
}

/* joystick bits for a numpad direction relative to facing */
static u8 stick(const Fighter *f, u8 dir) {
    u8 bits = 0;
    u8 h = (dir - 1) % 3;         /* 0 back, 1 neutral, 2 forward */
    u8 v = (dir - 1) / 3;         /* 0 down, 1 neutral, 2 up */
    if (v == 0) bits |= DOWN;
    if (v == 2) bits |= UP;
    if (h == 2) bits |= f->facing_right ? RIGHT : LEFT;
    if (h == 0) bits |= f->facing_right ? LEFT : RIGHT;
    return bits;
}

/* plays a motion one digit per frame, then holds the last direction with the button */
static u8 motion(const Fighter *f, const char *m, u8 t, u8 button) {
    u8 len = 0;
    while (m[len]) len++;
    if (t < len) return stick(f, (u8)(m[t] - '0'));
    return stick(f, (u8)(m[len - 1] - '0')) | button;
}

static u8 button_bits(u8 btn) {
    /* the light version of a move is enough for the cpu */
    if (btn & 1) return BA;
    if (btn & 2) return BB;
    if (btn & 4) return BC;
    return BD;
}

static void plan(Brain *b, u8 p) {
    b->plan = p;
    b->time = 0;
}

static u8 attacking(const Fighter *o) {
    return o->state == ST_ATTACK || o->state == ST_AIR_ATTACK || o->state == ST_THROW;
}

u8 cpu_think(Fighter *f) {
    Brain *b = &brains[f->id];
    Fighter *o = f->foe;
    s16 dist = (s16)((f->x - o->x) >> 8);
    if (dist < 0) dist = -dist;
    if (!b->seed) b->seed = 0x1234 + f->id * 777 + frame_count;
    const Character *ch = f->ch;

    /* busy: attacks, hits, air. Only a combo plan keeps feeding inputs (to cancel into the special). */
    if (!fighter_is_idle(f) && f->state != ST_RUN && f->state != ST_BLOCK) {
        if (b->plan == PLAN_COMBO || b->plan == PLAN_JUMP_IN || b->plan == PLAN_ANTI_AIR || b->plan == PLAN_SUPER || b->plan == PLAN_FIREBALL) {
            /* fall through to keep the plan's inputs flowing */
        } else {
            return 0;
        }
    }

    /* reactions override the plan, after a reaction time that shrinks with the level */
    u8 react = f->cpu < 8 ? 9 - f->cpu : 1;
    if (b->plan != PLAN_BLOCK && b->plan != PLAN_ANTI_AIR && fighter_is_idle(f)) {
        if (o->state == ST_AIR && dist < 120 && o->vy < FIX_ONE * 2 && o->state_time > react + 4 && chance(b, f, 8, 9)) {
            plan(b, PLAN_ANTI_AIR);
        } else if ((attacking(o) && dist < 120 && o->state_time >= react) || fx_threatens(f)) {
            if (chance(b, f, 25, 9)) {
                plan(b, PLAN_BLOCK);
                b->block_low = o->anim == AN_SWEEP || o->anim == AN_CLK;
            }
        }
    }
    /* punish: the opponent is recovering from a blocked or whiffed heavy attack close by */
    if (f->state == ST_BLOCK && f->stun <= 2 && dist < 80 && chance(b, f, 8, 10)) {
        plan(b, PLAN_COMBO);
    }

    if (b->plan == PLAN_IDLE || (b->plan == PLAN_WAIT && b->time > 12)) {
        u8 r = rnd(b) & 15;
        /* low levels hesitate: a pause between plans */
        if (f->cpu <= 2 && b->plan == PLAN_IDLE && (rnd(b) & 3) == 0) { plan(b, PLAN_WAIT); return 0; }
        if (f->power >= POWER_STOCK && dist < 150 && r < 7) plan(b, PLAN_SUPER);
        else if (dist > 170) plan(b, r < 6 ? PLAN_FIREBALL : r < 12 ? PLAN_APPROACH : PLAN_JUMP_IN);
        else if (dist > 95) plan(b, r < 4 ? PLAN_FIREBALL : r < 9 ? PLAN_APPROACH : r < 12 ? PLAN_JUMP_IN : PLAN_WAIT);
        else if (dist > 55) plan(b, r < 6 ? PLAN_POKE : r < 10 ? PLAN_SWEEP : r < 13 ? PLAN_COMBO : PLAN_WAIT);
        else plan(b, r < 4 ? PLAN_THROW : r < 11 ? PLAN_COMBO : r < 13 ? PLAN_POKE : PLAN_RETREAT);
    }
    u8 t = b->time++;

    switch (b->plan) {
    case PLAN_WAIT:
        return (t & 8) ? stick(f, 4) : 0;
    case PLAN_APPROACH:
        if (dist < 80 || t > 36) plan(b, PLAN_IDLE);
        return f->cpu >= 5 && t < 6 ? stick(f, (t == 0 || t >= 3) ? 6 : 5) : stick(f, 6);   /* dash at high levels */
    case PLAN_RETREAT:
        if (t > 16) plan(b, PLAN_IDLE);
        return stick(f, 4);
    case PLAN_BLOCK:
        if (t > 14 && !attacking(o) && !fx_threatens(f)) plan(b, PLAN_IDLE);
        return stick(f, b->block_low || (o->state == ST_ATTACK && (o->anim == AN_SWEEP || o->anim == AN_CLK)) ? 1 : 4);
    case PLAN_POKE:
        if (t > 10) { plan(b, PLAN_IDLE); return 0; }
        return t == 2 ? ((rnd(b) & 1) ? BD : BB) : 0;
    case PLAN_SWEEP:
        if (t > 14) { plan(b, PLAN_IDLE); return stick(f, 2); }
        return stick(f, 2) | (t == 3 ? BD : 0);
    case PLAN_JUMP_IN:
        if (t < 3) return stick(f, 9);
        if (f->y && o->y == 0 && dist < 80 && f->vy < 0) return (rnd(b) & 1) ? BD : BC;
        if (t > 45 || (t > 6 && f->y == 0)) {
            plan(b, f->y == 0 && dist < 70 ? PLAN_COMBO : PLAN_IDLE);
        }
        return 0;
    case PLAN_FIREBALL:
        if (t > 14) { plan(b, PLAN_IDLE); return 0; }
        return motion(f, ch->special1.motion, t, button_bits(ch->special1.buttons));
    case PLAN_ANTI_AIR:
        if (t > 16) { plan(b, PLAN_IDLE); return 0; }
        return motion(f, ch->special2.motion, t, button_bits(ch->special2.buttons));
    case PLAN_COMBO:
        /* crouching light kick, standing heavy punch, cancelled into special 1 */
        if (t == 0) return stick(f, 2) | BB;
        if (t < 9) return stick(f, 2);
        if (t == 9) return BC;
        if (t < 13) return 0;
        if (t < 13 + 4) return motion(f, ch->special1.motion, t - 13, button_bits(ch->special1.buttons));
        plan(b, PLAN_IDLE);
        return 0;
    case PLAN_SUPER:
        if (t > 12) { plan(b, PLAN_IDLE); return 0; }
        return motion(f, ch->super.motion, t, button_bits(ch->super.buttons));
    case PLAN_THROW:
        if (t > 5) { plan(b, PLAN_IDLE); return 0; }
        return stick(f, 6) | (t == 2 ? BC : 0);
    }
    plan(b, PLAN_IDLE);
    return 0;
}
