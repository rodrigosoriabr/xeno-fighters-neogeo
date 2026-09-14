#include "fighter.h"
#include "sound.h"
#include "fx.h"

u8 hitstop;
u8 shake;
static u16 last_hurt_voice;
u8 fight_event, fight_event_side;
u8 first_hit_done;

static void event(u8 ev, u8 side) {
    if (ev >= fight_event) {           /* keep the most important one of the frame */
        fight_event = ev;
        fight_event_side = side;
    }
}

/* cpu damage by difficulty, percent */
static const u8 cpu_damage[3] = {70, 100, 115};

static u8 scaled_damage(const Fighter *f, u8 damage) {
    u16 d = (u16)damage * f->ch->damage_scale * 135 / 10000;
    if (f->cpu) d = d * cpu_damage[difficulty] / 100;
    return (u8)(d > 255 ? 255 : d);
}

#define FIX(px) ((s32)(px) << 8)
#define PX(v) ((s16)((v) >> 8))
#define STAGE_LEFT 24
#define STAGE_RIGHT (stage_width - 24)
#define GRAVITY 96              /* 0.375 px/frame^2 */
#define MAX_DISTANCE 272        /* fighters can't walk further apart than the screen allows */

extern s16 stage_width;

/* ---------------------------------------------------------------------------------- animation */

static void set_anim(Fighter *f, u8 anim) {
    f->anim = anim;
    f->step = 0;
    f->step_time = 0;
    f->attack_connected = 0;
}

static const Step *current_step(const Fighter *f) {
    const Anim *a = &f->ch->anims[f->anim];
    return &f->ch->steps[a->first + f->step];
}

/* Advances one frame. Returns 1 when a non-looping animation just ended. */
static u8 advance_anim(Fighter *f, u8 loop) {
    const Anim *a = &f->ch->anims[f->anim];
    if (++f->step_time >= f->ch->steps[a->first + f->step].duration) {
        f->step_time = 0;
        if (f->step + 1 < a->count) {
            f->step++;
            if (f->ch->steps[a->first + f->step].hit && !f->ch->steps[a->first + f->step - 1].hit) {
                f->attack_connected = 0;   /* a new active window */
            }
        } else if (loop) {
            f->step = 0;
        } else {
            return 1;
        }
    }
    return 0;
}

static void set_state(Fighter *f, u8 state, u8 anim) {
    f->state = state;
    f->state_time = 0;
    set_anim(f, anim);
}

/* --------------------------------------------------------------------------------------- setup */

void fighter_init(Fighter *f, const Character *ch, u8 id, u8 palette_variant) {
    f->ch = ch;
    f->id = id;
    f->palette = id ? PAL_P2 : PAL_P1;
    set_palettes(f->palette, palette_variant ? ch->palettes_p2 : ch->palettes_p1, 4);
    fighter_ghost_palettes(f, palette_variant ? ch->palettes_p2 : ch->palettes_p1);
    f->wins = 0;
    f->power = 0;
    input_clear(&f->in);
}

void fighter_round_start(Fighter *f, s16 x) {
    f->x = FIX(x);
    f->y = 0;
    f->vx = f->vy = 0;
    f->max_health = (s16)((s32)HEALTH * f->ch->health_scale / 100);
    f->health = f->shown_health = f->trail_health = f->max_health;
    f->stun = f->invulnerable = f->combo = 0;
    f->move = 0;
    f->cols_drawn = 0;
    set_state(f, ST_STAND, AN_IDLE);
}

u8 fighter_is_idle(const Fighter *f) {
    return f->state == ST_STAND || f->state == ST_WALK || f->state == ST_CROUCH;
}

void fighter_win(Fighter *f) {
    f->vx = 0;
    set_state(f, ST_WIN, AN_WIN);
}

/* ------------------------------------------------------------------------------------- attacks */

enum { HIT_MID, HIT_LOW, HIT_OVERHEAD };

typedef struct { u8 anim, damage, heavy, kind; } Normal;

static void start_attack(Fighter *f, u8 anim, u8 damage, u8 heavy, u8 kind) {
    set_state(f, f->y ? ST_AIR_ATTACK : ST_ATTACK, anim);
    f->move = 0;
    f->attack_damage = scaled_damage(f, damage);
    f->attack_heavy = heavy;
    f->attack_kind = kind;
    f->hits_left = 1;
    if (f->y == 0) f->vx = 0;
    sound_play(heavy ? SFX_SWING_HEAVY : SFX_SWING_LIGHT);
}

static void start_move(Fighter *f, const Move *m, u8 anim) {
    set_state(f, ST_ATTACK, anim);
    f->move = m;
    f->attack_damage = scaled_damage(f, m->damage);
    f->attack_heavy = 1;
    f->attack_kind = HIT_MID;
    f->hits_left = m->hits;
    f->invulnerable = m->invulnerable;
    f->vx = 0;
    f->reversal = (u16)(frame_count - f->recovered_at) <= 4;
    sound_play(m->voice);
}

/* Specials and the super, checked before normals so a motion + button beats the plain button. */
static u8 try_moves(Fighter *f) {
    const Character *ch = f->ch;
    if (f->power >= POWER_STOCK && input_buffered(&f->in, ch->super.buttons, 3) &&
        input_motion(&f->in, ch->super.motion, 40)) {
        f->power -= POWER_STOCK;
        start_move(f, &ch->super, AN_SUPER);
        hitstop = 40;               /* super flash + portrait cut-in: the opponent freezes meanwhile */
        fx_spark(PX(f->x), GROUND_Y - 60, FXA_SUPER);
        sound_play(SFX_SUPER);
        return 1;
    }
    if (input_buffered(&f->in, ch->special2.buttons, 3) && input_motion(&f->in, ch->special2.motion, 20)) {
        start_move(f, &ch->special2, AN_SPECIAL2);
        return 1;
    }
    if (input_buffered(&f->in, ch->special1.buttons, 3) && input_motion(&f->in, ch->special1.motion, 20)) {
        start_move(f, &ch->special1, AN_SPECIAL1);
        return 1;
    }
    return 0;
}

static s16 distance(const Fighter *f) {
    s16 d = PX(f->x) - PX(f->foe->x);
    return d < 0 ? -d : d;
}

static u8 try_normals(Fighter *f) {
    Input *in = &f->in;
    u8 crouching = in->dir <= 3;
    if (f->y) {
        if (in->pressed & (BTN_A | BTN_C)) { start_attack(f, AN_JP, (in->pressed & BTN_C) ? 22 : 14, (in->pressed & BTN_C) != 0, HIT_OVERHEAD); return 1; }
        if (in->pressed & (BTN_B | BTN_D)) { start_attack(f, AN_JK, (in->pressed & BTN_D) ? 24 : 15, (in->pressed & BTN_D) != 0, HIT_OVERHEAD); return 1; }
        return 0;
    }
    /* throw: close, holding forward or back, heavy button, opponent on the ground and not stunned */
    if ((in->pressed & (BTN_C | BTN_D)) && (in->dir == 4 || in->dir == 6) && distance(f) < 56 &&
        f->foe->y == 0 && f->foe->stun == 0 && f->foe->state != ST_THROWN && f->foe->state != ST_KNOCK &&
        f->foe->state != ST_DOWN && !f->foe->invulnerable) {
        set_state(f, ST_THROW, AN_THROW);
        f->move = 0;
        Fighter *o = f->foe;
        o->vx = o->vy = 0;
        o->y = 0;
        set_state(o, ST_THROWN, AN_HIT_HEAVY);
        if (in->dir == 4) f->facing_right = !f->facing_right;   /* back throw */
        sound_play(SFX_GRAB);
        return 1;
    }
    if (crouching) {
        if (in->pressed & BTN_C) { start_attack(f, AN_CHP, 26, 1, HIT_MID); return 1; }
        if (in->pressed & BTN_D) { start_attack(f, AN_SWEEP, 24, 1, HIT_LOW); return 1; }
        if (in->pressed & BTN_A) { start_attack(f, AN_CLP, 10, 0, HIT_MID); return 1; }
        if (in->pressed & BTN_B) { start_attack(f, AN_CLK, 11, 0, HIT_LOW); return 1; }
    } else {
        if (in->pressed & BTN_C) { start_attack(f, AN_HP, 28, 1, HIT_MID); return 1; }
        if (in->pressed & BTN_D) { start_attack(f, AN_HK, 30, 1, HIT_MID); return 1; }
        if (in->pressed & BTN_A) { start_attack(f, AN_LP, 10, 0, HIT_MID); return 1; }
        if (in->pressed & BTN_B) { start_attack(f, AN_LK, 12, 0, HIT_MID); return 1; }
    }
    return 0;
}

/* ------------------------------------------------------------------------------------ movement */

static void face_opponent(Fighter *f) {
    if (f->x != f->foe->x) f->facing_right = f->x < f->foe->x;
}

static void neutral_ground(Fighter *f) {
    Input *in = &f->in;
    s32 fwd = f->facing_right ? 1 : -1;
    face_opponent(f);
    if (try_moves(f) || try_normals(f)) return;

    if (in->dir >= 7) {
        f->short_hop = 0;
        f->vx = in->dir == 9 ? fwd * FIX(3) : in->dir == 7 ? -fwd * FIX(3) : 0;
        set_state(f, ST_PREJUMP, AN_JUMP_TAKEOFF);
        return;
    }
    if (in->dir <= 3) {
        if (f->state != ST_CROUCH) set_state(f, ST_CROUCH, AN_CROUCH_DOWN);
        f->vx = 0;
        if (f->anim == AN_CROUCH_DOWN && advance_anim(f, 0)) set_anim(f, AN_CROUCH);
        return;
    }
    if (in->dir == 6 && input_double_tap(in, 6)) {
        set_state(f, ST_RUN, AN_DASH);
        return;
    }
    if (in->dir == 4 && input_double_tap(in, 4)) {
        set_state(f, ST_BACKSTEP, AN_JUMP_TAKEOFF);
        f->vx = -fwd * FIX(6);
        f->vy = FIX(3);
        f->invulnerable = 8;
        return;
    }
    if (in->dir == 6 || in->dir == 4) {
        if (f->state != ST_WALK) set_state(f, ST_WALK, AN_WALK);
        s32 speed = (s32)f->ch->walk_speed << 6;           /* walk_speed is in 1/4 px */
        f->vx = in->dir == 6 ? fwd * speed : -fwd * speed * 3 / 4;
        advance_anim(f, 1);
        return;
    }
    if (f->state != ST_STAND) set_state(f, ST_STAND, AN_IDLE);
    f->vx = 0;
    advance_anim(f, 1);
}

static u8 holding_block(const Fighter *f, u8 kind) {
    u8 d = f->in.dir;
    if (f->foe->state != ST_ATTACK && f->foe->state != ST_AIR_ATTACK && f->foe->move == 0 && !fx_threatens(f)) {
        return 0;
    }
    if (kind == HIT_LOW) return d == 1;
    if (kind == HIT_OVERHEAD) return d == 4 || d == 7;
    return d == 1 || d == 4 || d == 7;
}

/* ------------------------------------------------------------------------------------- getting hit */

static void take_hit(Fighter *f, s16 damage, u8 heavy, u8 kind, u8 knockdown, s16 from_x) {
    Fighter *o = f;
    s32 away = (PX(o->x) < from_x) ? -1 : 1;
    u8 was_stunned = o->state == ST_HIT || o->state == ST_KNOCK;
    u8 blockable = o->y == 0 && (o->state == ST_STAND || o->state == ST_WALK || o->state == ST_CROUCH || o->state == ST_BLOCK);

    if (blockable && holding_block(o, kind)) {
        o->health -= damage / 8;                         /* chip damage from specials only matters */
        if (o->health < 1) o->health = 1;
        o->stun = heavy ? 14 : 9;
        o->vx = away * FIX(heavy ? 3 : 2);
        set_state(o, ST_BLOCK, o->in.dir <= 3 ? AN_GUARD_CROUCH : AN_GUARD);
        o->power = o->power + 2 > POWER_MAX ? POWER_MAX : o->power + 2;
        hitstop = heavy ? 8 : 5;
        fx_spark(PX(o->x) - (s16)away * 20, GROUND_Y - (o->in.dir <= 3 ? 40 : 70), FXA_BLOCK);
        sound_play(SFX_BLOCK);
        return;
    }

    o->combo = was_stunned ? o->combo + 1 : 1;
    Fighter *att = o->foe;
    /* counter hit: caught while starting an attack of its own, 25% more damage */
    u8 counter = (o->state == ST_ATTACK || o->state == ST_AIR_ATTACK) && o->combo == 1;
    if (counter) damage += damage / 4;
    if (!first_hit_done) {
        first_hit_done = 1;
        event(EV_FIRST_ATTACK, att->id);
    } else if (att->move && att->reversal) {
        event(EV_REVERSAL, att->id);
        att->reversal = 0;
    } else if (counter && heavy) {
        event(EV_COUNTER, att->id);
    }
    if (att->anim == AN_SUPER) event(EV_SUPER_HIT, att->id);
    else if (heavy) event(EV_HEAVY, att->id);
    /* damage scaling in long combos */
    if (o->combo > 3) damage = damage * 3 / 4;
    o->health -= damage;
    o->power = o->power + 3 > POWER_MAX ? POWER_MAX : o->power + 3;
    hitstop = heavy ? 10 : 6;
    if (heavy) shake = 8;
    sound_play(heavy ? SFX_HIT_HEAVY : SFX_HIT_LIGHT);

    if (o->health <= 0) {
        o->health = 0;
        event(EV_KO, att->id);
        knockdown = 1;
        hitstop = 30;
        shake = 20;
        sound_play(VOICE_KO);
        sound_play(SFX_KO_BOOM);
    }
    if (knockdown || o->y > 0) {
        o->vx = away * FIX(3);
        o->vy = FIX(o->health ? 5 : 7);
        o->y += FIX(1);
        set_state(o, ST_KNOCK, AN_KNOCK);
        o->stun = 0;
        /* the hurt shout used to play on every knockdown and air hit ("a groan on every blow"):
         * now only the K.O. scream and heavy knockdowns, at most every 4 seconds */
        if (o->health == 0 || (heavy && knockdown && (u16)(frame_count - last_hurt_voice) > 240)) {
            sound_play(voice_hurt[o->ch->portrait]);
            last_hurt_voice = frame_count;
        }
    } else {
        o->stun = heavy ? 20 : 13;
        o->vx = away * FIX(heavy ? 4 : 2);
        set_state(o, ST_HIT, o->in.dir <= 3 ? AN_HIT_CROUCH : (heavy ? AN_HIT_HEAVY : AN_HIT_HIGH));
    }
}

/* Called for projectiles too (fx.c). */
void fighter_take_projectile(Fighter *f, const Move *m, s16 from_x) {
    if (f->invulnerable || f->state == ST_KNOCK || f->state == ST_DOWN || f->state == ST_GETUP) return;
    take_hit(f, (s16)(m->damage * f->foe->ch->damage_scale / 100), 1, HIT_MID, m->kind == MOVE_BEAM, from_x);
    f->foe->power = f->foe->power + 4 > POWER_MAX ? POWER_MAX : f->foe->power + 4;
}

static void box_screen(const Fighter *f, const s16 *box, s16 *out) {
    s16 x = PX(f->x), y = -PX(f->y);
    if (f->facing_right) {
        out[0] = x + box[0]; out[2] = x + box[2];
    } else {
        out[0] = x - box[2]; out[2] = x - box[0];
    }
    out[1] = y + box[1]; out[3] = y + box[3];
}

static void check_attack(Fighter *f) {
    Fighter *o = f->foe;
    const Step *s = current_step(f);
    if (!s->hit || f->attack_connected || f->hits_left == 0) return;
    if (o->invulnerable || o->state == ST_DOWN || o->state == ST_GETUP || o->state == ST_KO ||
        (o->state == ST_KNOCK && f->move == 0)) return;
    const Frame *af = &f->ch->images.frames[s->frame];
    const Frame *of = &o->ch->images.frames[o->ch->steps[o->ch->anims[o->anim].first + o->step].frame];
    s16 hit[4], hurt[4];
    box_screen(f, af->hit, hit);
    box_screen(o, of->hurt, hurt);
    if (hit[2] == hit[0] || hit[0] > hurt[2] || hit[2] < hurt[0] || hit[1] > hurt[3] || hit[3] < hurt[1]) return;

    f->attack_connected = 1;
    if (f->hits_left > 1) {
        f->hits_left--;
        f->attack_connected = 0;       /* rushes hit again after the next step starts */
    }
    u8 knockdown = f->anim == AN_SWEEP || (f->move && f->move->kind != MOVE_PROJECTILE && f->hits_left <= 1) || f->anim == AN_SUPER;
    take_hit(o, f->attack_damage, f->attack_heavy, f->attack_kind, knockdown, PX(f->x));
    f->power = f->power + (f->move ? 2 : 5) > POWER_MAX ? POWER_MAX : f->power + (f->move ? 2 : 5);
    s16 sx = (hit[0] > hurt[0] ? hit[0] : hurt[0]) + (hit[2] < hurt[2] ? hit[2] : hurt[2]);
    s16 sy = (hit[1] > hurt[1] ? hit[1] : hurt[1]) + (hit[3] < hurt[3] ? hit[3] : hurt[3]);
    if (o->state != ST_BLOCK) fx_spark(sx / 2, GROUND_Y + sy / 2, f->attack_heavy ? FXA_HEAVY : FXA_LIGHT);
}

/* Normals can be cancelled into a special or the super while they are connecting. */
static u8 cancel_window(const Fighter *f) {
    if (f->move || f->anim == AN_SWEEP || f->anim == AN_JP || f->anim == AN_JK) return 0;
    return f->attack_connected && f->step_time < 10;
}

/* ------------------------------------------------------------------------------------- update */

void fighter_update(Fighter *f, u8 raw) {
    Input *in = &f->in;
    input_update(in, raw, f->facing_right);
    if (f->invulnerable) f->invulnerable--;
    f->state_time++;

    switch (f->state) {
    case ST_INTRO:
        advance_anim(f, 1);
        break;
    case ST_STAND:
    case ST_WALK:
    case ST_CROUCH:
        neutral_ground(f);
        break;
    case ST_PREJUMP:
        if (!(in->dir >= 7) && f->state_time <= 3) f->short_hop = 1;
        if (f->state_time >= 4) {
            f->vy = f->short_hop ? FIX(6) : FIX(8) + FIX(3 - f->ch->weight) / 4;
            f->y = FIX(1);
            set_state(f, ST_AIR, AN_JUMP_UP);
            sound_play(SFX_JUMP);
        }
        break;
    case ST_AIR:
        if (try_normals(f)) break;
        if (f->vy < FIX(2) && f->vy > -FIX(2)) {
            if (f->anim != AN_JUMP_APEX) set_anim(f, AN_JUMP_APEX);
        } else if (f->vy <= -FIX(2) && f->anim != AN_JUMP_FALL) {
            set_anim(f, AN_JUMP_FALL);
        }
        break;
    case ST_RUN:
        face_opponent(f);
        if (try_moves(f) || try_normals(f)) break;
        if (in->dir != 6 && in->dir != 9 && in->dir != 3) {
            set_state(f, ST_STAND, AN_IDLE);
            f->vx = 0;
            break;
        }
        if (in->dir == 9) {
            f->vx = (f->facing_right ? 1 : -1) * FIX(5);
            set_state(f, ST_PREJUMP, AN_JUMP_TAKEOFF);
            break;
        }
        f->vx = (f->facing_right ? 1 : -1) * FIX(5);
        advance_anim(f, 1);
        if (f->state_time % 10 == 1) fx_spark(PX(f->x) - (f->facing_right ? 16 : -16), GROUND_Y, FXA_DUST);
        break;
    case ST_BACKSTEP:
        break;                              /* landing ends it */
    case ST_ATTACK:
    case ST_THROW:
        if (f->state == ST_ATTACK) {
            if (cancel_window(f) && try_moves(f)) break;
            check_attack(f);
            if (f->move && current_step(f)->hit) {
                s32 fwd = f->facing_right ? 1 : -1;
                if (f->move->kind == MOVE_RUSH || f->move->kind == MOVE_GRAB) f->vx = fwd * FIX(f->move->speed_x);
                if (f->move->kind == MOVE_RISING && f->y == 0 && f->step_time == 0) {
                    f->vy = FIX(f->move->speed_y);
                    f->vx = fwd * FIX(2);
                    f->y = FIX(1);
                }
            } else if (f->move && (f->move->kind == MOVE_RUSH || f->move->kind == MOVE_GRAB)) {
                f->vx = 0;
            }
            /* projectile leaves the hand on the step with the longest pose (the release) */
            if (f->move && (f->move->kind == MOVE_PROJECTILE || f->move->kind == MOVE_BEAM) &&
                f->step == 2 && f->step_time == 0) {
                fx_projectile(f, f->move);
            }
            if (f->move && f->move->kind == MOVE_TELEPORT && f->step == 2 && f->step_time == 0) {
                s16 target = PX(f->foe->x) + (f->facing_right ? 48 : -48);
                if (target < STAGE_LEFT) target = STAGE_LEFT;
                if (target > STAGE_RIGHT) target = STAGE_RIGHT;
                f->x = FIX(target);
            }
        } else if (f->step == 1 && f->step_time == 0) {
            Fighter *o = f->foe;
            s16 dmg = (s16)(30 * f->ch->damage_scale / 100);
            o->facing_right = !f->facing_right;
            o->x = f->x + (f->facing_right ? -FIX(24) : FIX(24));
            take_hit(o, dmg, 1, HIT_MID, 1, PX(f->x) + (f->facing_right ? 60 : -60));
            shake = 12;
        }
        if (advance_anim(f, 0)) {
            f->move = 0;
            if (f->y > 0) {
                set_state(f, ST_AIR, AN_JUMP_FALL);
            } else {
                set_state(f, in->dir <= 3 ? ST_CROUCH : ST_STAND, in->dir <= 3 ? AN_CROUCH : AN_IDLE);
            }
        }
        break;
    case ST_AIR_ATTACK:
        check_attack(f);
        if (f->step + 1 < f->ch->anims[f->anim].count) advance_anim(f, 0);
        break;
    case ST_BLOCK:
    case ST_HIT:
        if (f->vx) f->vx = f->vx * 7 / 8;
        if (f->stun) f->stun--;
        if (f->stun == 0) {
            f->combo = 0;
            f->recovered_at = frame_count;
            set_state(f, in->dir <= 3 ? ST_CROUCH : ST_STAND, in->dir <= 3 ? AN_CROUCH : AN_IDLE);
        }
        break;
    case ST_THROWN:
        break;
    case ST_KNOCK:
        advance_anim(f, 0);
        break;
    case ST_DOWN:
        f->vx = 0;
        if (f->health == 0) {
            f->state = ST_KO;
        } else if (f->state_time > 30) {
            set_state(f, ST_GETUP, AN_GETUP);
            f->invulnerable = 24;
        }
        break;
    case ST_GETUP:
        if (advance_anim(f, 0)) {
            f->combo = 0;
            f->recovered_at = frame_count;
            set_state(f, ST_STAND, AN_IDLE);
        }
        break;
    case ST_WIN:
        if (f->step + 1 < f->ch->anims[AN_WIN].count) advance_anim(f, 0);
        break;
    case ST_KO:
        break;
    }

    /* physics */
    f->x += f->vx;
    if (f->y > 0 || f->vy > 0) {
        f->y += f->vy;
        f->vy -= GRAVITY;
        if (f->y <= 0) {
            f->y = 0;
            f->vy = 0;
            if (f->state == ST_KNOCK) {
                set_state(f, ST_DOWN, AN_DOWN);
                shake = 6;
                fx_spark(PX(f->x), GROUND_Y, FXA_DUST);
                sound_play(SFX_LAND_HEAVY);
            } else if (f->state == ST_AIR || f->state == ST_AIR_ATTACK || f->state == ST_BACKSTEP ||
                       (f->state == ST_ATTACK && f->move && f->move->kind == MOVE_RISING)) {
                f->vx = 0;
                f->move = 0;
                set_state(f, ST_STAND, AN_IDLE);
                sound_play(SFX_LAND);
            }
        }
    }
    if (f->x < FIX(STAGE_LEFT)) f->x = FIX(STAGE_LEFT);
    if (f->x > FIX(STAGE_RIGHT)) f->x = FIX(STAGE_RIGHT);
    if (f->health < f->shown_health) f->shown_health--;
    if (f->trail_health > f->shown_health && f->stun == 0 && f->state != ST_KNOCK) f->trail_health -= 2;
    if (f->trail_health < f->shown_health) f->trail_health = f->shown_health;
}

/* Keeps the two bodies from overlapping and within one screen of each other. */
void fighters_collide(Fighter *a, Fighter *b) {
    s16 ax = PX(a->x), bx = PX(b->x);
    s16 d = bx - ax;
    s16 overlap = 40 - (d < 0 ? -d : d);
    if (overlap > 0 && a->y < FIX(40) && b->y < FIX(40) && a->state != ST_THROWN && b->state != ST_THROWN) {
        s32 push = FIX(overlap) / 2;
        if (d >= 0) { a->x -= push; b->x += push; } else { a->x += push; b->x -= push; }
    }
    d = PX(b->x) - PX(a->x);
    if (d > MAX_DISTANCE) { a->x += FIX(d - MAX_DISTANCE) / 2; b->x -= FIX(d - MAX_DISTANCE) / 2; }
    if (d < -MAX_DISTANCE) { a->x -= FIX(-d - MAX_DISTANCE) / 2; b->x += FIX(-d - MAX_DISTANCE) / 2; }
    /* pushback against the corner goes to the attacker */
    if (a->x <= FIX(STAGE_LEFT) && (b->state == ST_ATTACK) && (a->state == ST_HIT || a->state == ST_BLOCK)) b->x += a->vx < 0 ? -a->vx : a->vx;
    if (a->x >= FIX(STAGE_RIGHT) && (b->state == ST_ATTACK) && (a->state == ST_HIT || a->state == ST_BLOCK)) b->x -= a->vx < 0 ? -a->vx : a->vx;
    if (b->x <= FIX(STAGE_LEFT) && (a->state == ST_ATTACK) && (b->state == ST_HIT || b->state == ST_BLOCK)) a->x += b->vx < 0 ? -b->vx : b->vx;
    if (b->x >= FIX(STAGE_RIGHT) && (a->state == ST_ATTACK) && (b->state == ST_HIT || b->state == ST_BLOCK)) a->x -= b->vx < 0 ? -b->vx : b->vx;
}

void fighter_draw(Fighter *f, u16 first, s16 camera_x, s16 camera_y) {
    const Step *s = current_step(f);
    s16 x = PX(f->x) - camera_x;
    s16 y = GROUND_Y - PX(f->y) - camera_y;
    if (hitstop && f->state == ST_HIT && (frame_count & 2)) x += 2;     /* victim shakes during hitstop */
    u8 flash = f->invulnerable && f->state == ST_GETUP && (frame_count & 4);
    if (flash) {
        hide_sprites(first, 16);
        f->cols_drawn = 0;
        return;
    }
    f->cols_drawn = draw_frame(first, 16, &f->ch->images, s->frame, x, y, !f->facing_right, f->palette);
    if (!hitstop) {
        f->trail_head = (f->trail_head + 1) & 7;
        f->trail_x[f->trail_head] = PX(f->x);
        f->trail_y[f->trail_head] = PX(f->y);
        f->trail_frame[f->trail_head] = s->frame;
        f->trail_flip[f->trail_head] = !f->facing_right;
    }
}

/* afterimage: where the fighter was 6 frames ago, in its ghost palettes. Returns 0 when not needed. */
u8 fighter_draw_afterimage(Fighter *f, u16 first, s16 camera_x, s16 camera_y) {
    u8 fast = f->state == ST_ATTACK && f->move && (f->move->kind == MOVE_RUSH || f->move->kind == MOVE_RISING ||
              f->move->kind == MOVE_GRAB || f->anim == AN_SUPER);
    if (!fast && f->state != ST_RUN && f->state != ST_BACKSTEP) return 0;
    u8 k = (f->trail_head + 2) & 7;
    draw_frame(first, 16, &f->ch->images, f->trail_frame[k], f->trail_x[k] - camera_x,
               GROUND_Y - f->trail_y[k] - camera_y, f->trail_flip[k], f->palette + PAL_AFTER);
    return 1;
}

/* ghost palettes: a darker copy of the fighter's colors pushed toward violet-blue */
void fighter_ghost_palettes(Fighter *f, const u16 *src) {
    volatile u16 *p = (volatile u16 *)0x400000 + (f->palette + PAL_AFTER) * 16;
    for (u16 i = 0; i < 4 * 16; i++) {
        u16 c = src[i];
        if ((i & 15) == 0) {
            *p++ = c;
            continue;
        }
        u16 r = (c >> 8) & 15, g = (c >> 4) & 15, b = c & 15;
        u16 l = (r + g * 2 + b) / 4;
        r = l / 2 + 1;
        g = l * 2 / 3;
        b = l + 5 > 15 ? 15 : l + 5;
        *p++ = r << 8 | g << 4 | b;
    }
}
