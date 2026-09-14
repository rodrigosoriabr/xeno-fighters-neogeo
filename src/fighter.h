/* Fighters: character data, state machine, attacks, hit detection. */
#ifndef FIGHTER_H
#define FIGHTER_H

#include <ngdevkit/types.h>
#include "video.h"
#include "input.h"

/* Animation order shared by every character (tools/make_fighter.py writes them in this order). */
enum {
    AN_IDLE, AN_WALK, AN_CROUCH_DOWN, AN_CROUCH, AN_JUMP_TAKEOFF, AN_JUMP_UP, AN_JUMP_APEX, AN_JUMP_FALL,
    AN_DASH, AN_GUARD, AN_LP, AN_HP, AN_LK, AN_HK, AN_CLP, AN_CLK, AN_CHP, AN_SWEEP, AN_JP, AN_JK,
    AN_HIT_HIGH, AN_HIT_HEAVY, AN_HIT_CROUCH, AN_GUARD_CROUCH, AN_KNOCK, AN_DOWN, AN_GETUP, AN_THROW,
    AN_SPECIAL1, AN_SPECIAL2, AN_SUPER, AN_WIN, AN_COUNT
};

typedef struct { u16 frame; u8 duration; u8 hit; } Step;
typedef struct { u16 first; u16 count; } Anim;

/* How a special or super behaves. */
enum { MOVE_PROJECTILE, MOVE_RISING, MOVE_RUSH, MOVE_TELEPORT, MOVE_BEAM, MOVE_GRAB };

typedef struct {
    const char *motion;   /* numpad digits, e.g. "236" */
    u8 buttons;           /* any of these (BTN_*) */
    u8 kind;
    u8 damage;
    u8 hits;              /* multi-hit rushes and supers */
    s8 speed_x;           /* px/frame forward while active (rush) or projectile speed */
    s8 speed_y;           /* rising moves: initial upward speed */
    u8 invulnerable;      /* frames of invulnerability from the start */
    u8 projectile;        /* fx image for the projectile */
    u8 voice;             /* sound command for the shout */
} Move;

typedef struct {
    const char *name;
    ImageSet images;
    const Step *steps;
    const Anim *anims;
    const u16 *palettes_p1, *palettes_p2;
    u8 walk_speed;        /* 1/4 px per frame */
    u8 weight;            /* 0 light .. 3 heavy: jump height, pushback */
    u8 damage_scale;      /* percent */
    Move special1, special2, super;
    u8 portrait;          /* index in the per-character tables (voices, portraits, story) */
    u8 stage;
    u8 health_scale;      /* percent: the boss has more */
} Character;

extern const Character characters[];
#define CHAR_VORAX 0
#define CHAR_ZYRA 1
#define CHAR_OSSK 2
#define CHAR_GRUMM 3
#define CHAR_KRELL 4
#define CHAR_NYXA 5
#define CHAR_BRUTOK 6
#define CHAR_KORAL 7
#define CHAR_XAL 8
#define CHARACTER_COUNT 9
#define PLAYABLE_COUNT 8

enum {
    ST_STAND, ST_WALK, ST_CROUCH, ST_PREJUMP, ST_AIR, ST_RUN, ST_BACKSTEP, ST_ATTACK, ST_AIR_ATTACK,
    ST_BLOCK, ST_HIT, ST_KNOCK, ST_DOWN, ST_GETUP, ST_THROW, ST_THROWN, ST_WIN, ST_KO, ST_INTRO
};

typedef struct Fighter {
    const Character *ch;
    u8 id;                /* 0 or 1 */
    u8 cpu;               /* 0 human, 1+ cpu difficulty */
    u8 palette;
    s32 x, y;             /* 24.8 fixed point, y = height above ground (up positive) */
    s32 vx, vy;
    u8 facing_right;
    u8 state;
    u16 state_time;
    u8 anim, step, step_time;
    s16 health, shown_health, trail_health;
    s16 max_health;
    u16 power;            /* 0..POWER_MAX; every POWER_STOCK is one super in stock */
    u8 stun;              /* hit or block stun frames left */
    u8 invulnerable;
    u8 attack_connected;  /* the current attack already hit (one hit per active window) */
    u8 hits_left;
    const Move *move;     /* active special/super, 0 for normals */
    u8 attack_kind;       /* low / mid / overhead */
    u8 attack_damage;
    u8 attack_heavy;
    u8 combo;
    u8 wins;
    u8 short_hop;
    u8 cols_drawn;
    u8 reversal;          /* the current special started right after recovering */
    u16 recovered_at;     /* frame_count when hit/block stun or getup last ended */
    /* recent positions for the afterimage */
    s16 trail_x[8], trail_y[8];
    u16 trail_frame[8];
    u8 trail_flip[8], trail_head;
    Input in;
    struct Fighter *foe;
} Fighter;

#define HEALTH 400
#define FIX_ONE 256              /* 1 px in the 24.8 fixed point positions */
#define POWER_STOCK 96
#define STOCKS 3
#define POWER_MAX (POWER_STOCK * STOCKS)
#define GROUND_Y 200          /* screen line of the floor (visible lines; see SCREEN_TOP) */

extern u8 hitstop;            /* frames the whole fight is frozen after a hit */
extern u8 shake;              /* frames of screen shake */

/* things the fight announces or the crowd reacts to (main.c reads and clears them) */
enum { EV_NONE, EV_FIRST_ATTACK, EV_COUNTER, EV_REVERSAL, EV_HEAVY, EV_SUPER_HIT, EV_KO };
extern u8 fight_event, fight_event_side;
extern u8 first_hit_done;

/* options picked before the arcade run (main.c) */
enum { EASY, NORMAL, HARD };
extern u8 difficulty;
extern u8 turbo;              /* 1: every frame runs game logic; 0 (standard): 3 of 4 frames */

void fighter_init(Fighter *f, const Character *ch, u8 id, u8 palette_variant);
void fighter_round_start(Fighter *f, s16 x);
void fighter_update(Fighter *f, u8 raw_input);
void fighters_collide(Fighter *a, Fighter *b);
void fighter_draw(Fighter *f, u16 sprite_first, s16 camera_x, s16 camera_y);
u8 fighter_is_idle(const Fighter *f);
u8 fighter_draw_afterimage(Fighter *f, u16 first, s16 camera_x, s16 camera_y);
void fighter_ghost_palettes(Fighter *f, const u16 *src);
void fighter_win(Fighter *f);
u8 fx_threatens(const Fighter *f);

/* projectiles and sparks */
void fx_reset(void);
void fx_update(Fighter *p1, Fighter *p2);
void fx_draw(s16 camera_x, s16 camera_y);
void fx_spark(s16 x, s16 y, u8 kind);

/* cpu: returns the raw joystick bits the cpu "presses" this frame */
u8 cpu_think(Fighter *f);

#endif
