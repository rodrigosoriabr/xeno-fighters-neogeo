/* Xeno Fighters: game flow. The MVS BIOS calls main_mvs_title() for the title screen, main() for the
 * attract sequence (story intro + cpu demo) and for the game once a player pressed start (with credits
 * on MVS), and player_start() when start is pressed. */
#include <ngdevkit/neogeo.h>
#include <ngdevkit/bios-ram.h>
#include "video.h"
#include "fighter.h"
#include "stage.h"
#include "hud.h"
#include "sound.h"
#include "fx.h"
#include "screens.h"
#include "screens_data.h"
#include <ngdevkit/ng-fix.h>

#define USER_MODE_DEMO 1
#define USER_MODE_GAME 2
#define ROUND_SECONDS 60
#define TIMER_TICK 45          /* frames per timer second (arcade timers run fast) */
#define LADDER_FOES 6          /* regular opponents before the boss */

extern const Stage stage_temple, stage_sanctuary, stage_hive, stage_mine, stage_spaceport, stage_rooftops, stage_colosseum,
    stage_dome, stage_throne;
/* each character's home stage, where you fight them */
static const Stage *const stages[CHARACTER_COUNT] = {&stage_temple, &stage_sanctuary, &stage_hive, &stage_mine, &stage_spaceport,
                                                     &stage_rooftops, &stage_colosseum, &stage_dome, &stage_throne};

static Fighter fighters[2];
static const Stage *current_stage;
static u8 challenger;          /* set when the other player pressed start during a cpu match */
u8 demo;                       /* attract mode: cpu vs cpu until start or timeout */
u16 demo_frames;
u8 difficulty = NORMAL;
u8 turbo = 1;

/* portrait palettes for the super cut-in */
#define SLOT_CUTIN1 96
#define SLOT_CUTIN2 108
#define BLOCK_CUTIN (SPR_SCREEN + 20)
#define BLOCK_ANNOUNCE (SPR_SCREEN + 92)

void player_start(void) {
    bios_user_mode = USER_MODE_GAME;
    if (bios_start_flag & 1) bios_player_mod1 = 1;
    if (bios_start_flag & 2) bios_player_mod2 = 1;
    if ((fighters[0].cpu && (bios_start_flag & 1)) || (fighters[1].cpu && (bios_start_flag & 2))) {
        challenger = 1;
    }
}

void coin_sound(void) {
    sound_play(SFX_COIN);
}

/* Every VRAM write must happen right after the vblank: the runtime's VBlank interrupt calls the
 * BIOS SYSTEM_IO routine, which moves REG_VRAMADDR (MVS credit display), so a write sequence that
 * straddles the interrupt lands in the wrong sprites (fighters showed up sliced into offset columns).
 * Loops therefore start with frame_start(), draw, and only then run the game logic. */
static void frame_start(void) {
    wait_frame();
    sound_frame();
}

/* ---------------------------------------------------------------------------------------- fight */

static void draw_shadow(u8 i, const Fighter *f, s16 cx, s16 cy) {
    /* shrinks with the hardware zoom as the fighter rises */
    s16 h = (s16)(f->y >> 8);
    u8 scale = h > 120 ? 8 : (u8)(16 - h / 15);
    s16 x = (s16)(f->x >> 8) - cx;
    draw_frame_scaled(SPR_SHADOW + i * 4, 4, &screen_images, screen_image[IMG_HUD].frame + HUD_SHADOW,
                      x - (32 * scale >> 4), GROUND_Y - 5 - cy, PAL_HUD, scale);
}

static void draw_fight(void) {
    s16 cx = camera_x, cy = camera_y;
    if (shake) {
        cx += (frame_count & 2) ? 3 : -3;
        cy += (frame_count & 4) ? 2 : -1;
    }
    stage_draw();
    draw_shadow(0, &fighters[0], cx, cy);
    draw_shadow(1, &fighters[1], cx, cy);
    /* the attacking fighter is drawn in front */
    Fighter *front = &fighters[1], *back = &fighters[0];
    if (fighters[0].state == ST_ATTACK || fighters[0].state == ST_THROW || fighters[0].state == ST_AIR_ATTACK) {
        front = &fighters[0];
        back = &fighters[1];
    }
    if (!fighter_draw_afterimage(front, SPR_AFTER, cx, cy) && !fighter_draw_afterimage(back, SPR_AFTER, cx, cy)) {
        hide_sprites(SPR_AFTER, 16);
    }
    fighter_draw(back, SPR_FIGHTER, cx, cy);
    fighter_draw(front, SPR_FIGHTER + 16, cx, cy);
    fx_draw(cx, cy);
    stage_draw_front(cx, cy);
}

static u8 read_input(Fighter *f) {
    if (f->cpu) return cpu_think(f);
    return f->id == 0 ? bios_p1current : bios_p2current;
}

#define WORD_Y 94

/* small announcement (FIRST ATTACK, COUNTER, REVERSAL) over the attacker's side, zooming in */
static u8 announce_word, announce_time;
static s16 announce_x;

static void announce(u8 word, u8 side, u8 voice) {
    announce_word = word;
    announce_time = 60;
    announce_x = side == 0 ? 96 : SCREEN_W - 96;
    sound_play(voice);
}

static void draw_announce(u8 big_word_showing) {
    if (!announce_time || big_word_showing) return;
    announce_time--;
    if (announce_time == 0) {
        word_hide();
        return;
    }
    word_zoom(announce_word, announce_x, 64, (u8)(60 - announce_time), 9);
}

/* super cut-in: the portrait sweeps across while the super pose freezes the fight */
static void draw_cutin(u8 side, u8 t) {
    s16 x;
    if (t < 8) x = -150 + (s16)t * 22;
    else if (t < 30) x = 26 + (s16)(t - 8);
    else x = 48 + ((s16)t - 30) * 36;
    if (side) {
        draw_frame(BLOCK_CUTIN, 14, &screen_images, screen_image[IMG_PORTRAIT(fighters[1].ch->portrait)].frame,
                   SCREEN_W - x, 0, 1, SLOT_CUTIN2);
    } else {
        draw_frame(BLOCK_CUTIN, 14, &screen_images, screen_image[IMG_PORTRAIT(fighters[0].ch->portrait)].frame, x, 0, 0, SLOT_CUTIN1);
    }
}

/* crowd and announcer react to what just happened */
static u16 crowd_quiet_until;

static void react(u8 *big_word) {
    u8 ev = fight_event, side = fight_event_side;
    fight_event = EV_NONE;
    if (ev == EV_NONE || demo) return;
    switch (ev) {
    case EV_FIRST_ATTACK: announce(WORD_FIRST_ATTACK, side, VOICE_FIRST_ATTACK); crowd_cheer(24); break;
    case EV_REVERSAL: announce(WORD_REVERSAL, side, VOICE_REVERSAL); crowd_cheer(40); break;
    case EV_COUNTER: announce(WORD_COUNTER, side, VOICE_COUNTER); crowd_cheer(24); break;
    case EV_SUPER_HIT: crowd_cheer(60); break;
    case EV_HEAVY: crowd_cheer(12); break;
    case EV_KO: crowd_cheer(160); break;
    }
    if (ev == EV_KO) {
        sound_play(SFX_CROWD_ROAR);
        crowd_quiet_until = frame_count + 240;
    } else if ((ev == EV_SUPER_HIT || ev == EV_REVERSAL) && frame_count > crowd_quiet_until) {
        sound_play(SFX_CROWD_CHEER);
        crowd_quiet_until = frame_count + 180;
    } else if (ev == EV_HEAVY && frame_count > crowd_quiet_until && (frame_count & 3) == 0) {
        sound_play(SFX_CROWD_OOH);
        crowd_quiet_until = frame_count + 300;
    }
    (void)big_word;
}

/* One round. Returns the winner (0/1), 2 for a draw or 3 when interrupted. */
static u8 play_round(u8 round) {
    Fighter *p1 = &fighters[0], *p2 = &fighters[1];
    fighter_round_start(p1, stage_width / 2 - 70);
    fighter_round_start(p2, stage_width / 2 + 70);
    p1->facing_right = 1;
    p2->facing_right = 0;
    fx_reset();
    hitstop = shake = 0;
    first_hit_done = 0;
    fight_event = EV_NONE;
    announce_time = 0;
    camera_x = (stage_width - SCREEN_W) / 2;
    hud_update(p1, p2, ROUND_SECONDS);

    /* intro: ROUND n ... FIGHT! (zooming in) */
    u8 round_word = round == 2 ? WORD_FINAL_ROUND : round == 0 ? WORD_ROUND_1 : WORD_ROUND_2;
    sound_play(round == 2 ? VOICE_FINAL : round == 0 ? VOICE_ROUND1 : VOICE_ROUND2);
    for (u8 t = 0; t < 90; t++) {
        frame_start();
        draw_fight();
        word_zoom(round_word, SCREEN_W / 2, WORD_Y, t, 16);
        fighter_update(p1, 0);
        fighter_update(p2, 0);
        stage_update();
    }
    word_hide();
    sound_play(VOICE_FIGHT);
    crowd_cheer(40);

    s16 timer = ROUND_SECONDS;
    u8 tick = 0, message_time = 40, slow = 0, ended = 0, winner = 2, big_word = 1;
    u8 cutin = 0, cutin_side = 0, ko_flash = 0;
    u16 end_time = 0;          /* frames since the round ended (u8 wrapped before 260: round never ended) */
    const u16 *stage_pal = current_stage->palettes;
    u8 super_dim = 0;

    for (;;) {
        frame_start();
        stage_camera((s16)(p1->x >> 8), (s16)(p2->x >> 8), (s16)((p1->y > p2->y ? p1->y : p2->y) >> 8));
        draw_fight();
        hud_update(p1, p2, timer);
        if (message_time) {
            word_zoom(WORD_FIGHT, SCREEN_W / 2, WORD_Y, (u8)(40 - message_time), 16);
            if (--message_time == 0) {
                word_hide();
                big_word = 0;
            }
        }
        if (cutin) {
            draw_cutin(cutin_side, (u8)(40 - cutin));
            if (--cutin == 0) hide_sprites(BLOCK_CUTIN, 14);
        }
        if (ko_flash) {
            ko_flash--;
            flash_palettes(PAL_STAGE, stage_pal, current_stage->palette_count, (u8)(ko_flash * 2));
        }
        draw_announce(big_word);
        if (challenger) return 3;
        if (demo && (bios_user_mode == USER_MODE_GAME || ++demo_frames > 1800)) return 3;
        /* standard speed: the game logic skips every 4th frame (75% of turbo, which is the original pace) */
        if (!turbo && !demo && (frame_count & 3) == 0) continue;

        if (hitstop) {
            hitstop--;
            /* super flash: darken the stage while the super pose holds, with the portrait cut-in */
            u8 super = (p1->anim == AN_SUPER && p1->step == 0) || (p2->anim == AN_SUPER && p2->step == 0);
            if (super && !super_dim) {
                fade_palettes(PAL_STAGE, stage_pal, current_stage->palette_count, 4);
                super_dim = 1;
                cutin = 40;
                cutin_side = (p2->anim == AN_SUPER && p2->step == 0);
                crowd_cheer(40);
            }
            if (super) {
                if (p1->anim == AN_SUPER && p1->step == 0) fighter_update(p1, read_input(p1));
                if (p2->anim == AN_SUPER && p2->step == 0) fighter_update(p2, read_input(p2));
            }
        } else if (!slow || (frame_count & 1)) {
            if (super_dim) { set_palettes(PAL_STAGE, stage_pal, current_stage->palette_count); super_dim = 0; }
            u8 i1 = ended ? 0 : read_input(p1);
            u8 i2 = ended ? 0 : read_input(p2);
            fighter_update(p1, i1);
            fighter_update(p2, i2);
            fighters_collide(p1, p2);
            fx_update(p1, p2);
            if (shake) shake--;
            if (!ended && ++tick >= TIMER_TICK) {
                tick = 0;
                if (timer > 0) timer--;
            }
        }
        stage_update();
        react(&big_word);

        if (!ended) {
            if (p1->health == 0 || p2->health == 0 || timer == 0) {
                ended = 1;
                end_time = 0;
                if (p1->health == 0 && p2->health == 0) winner = 2;
                else if (p1->health == 0) winner = 1;
                else if (p2->health == 0) winner = 0;
                else winner = p1->health == p2->health ? 2 : (p1->health > p2->health ? 0 : 1);
                announce_time = 0;
                message_time = 0;
                big_word = 1;
                if (timer == 0 && p1->health && p2->health) {
                    word_show(WORD_TIME_OVER, WORD_Y);
                    sound_play(VOICE_TIMEOVER);
                } else {
                    slow = 1;
                    ko_flash = 8;
                    Fighter *loser = p1->health == 0 ? p1 : p2;
                    fx_spark((s16)(loser->x >> 8), GROUND_Y - 60, FXA_FIREBURST);
                    sound_play(SFX_FIRE);
                }
            }
        } else {
            end_time++;
            if (end_time < 40 && (p1->health == 0 || p2->health == 0)) word_zoom(WORD_KO, SCREEN_W / 2, WORD_Y, (u8)end_time, 16);
            if (end_time == 90) {
                slow = 0;
                word_hide();
            }
            if (end_time == 150 && winner < 2) {
                Fighter *w = &fighters[winner];
                if (w->health == w->max_health) {
                    word_show(WORD_PERFECT, WORD_Y);
                    sound_play(VOICE_PERFECT);
                    sound_play(SFX_CROWD_APPLAUSE);
                }
                fighter_win(w);
                sound_play(voice_win[w->ch->portrait]);
                crowd_cheer(120);
            }
            if (end_time == 150 && winner == 2) {
                word_show(WORD_DRAW_GAME, WORD_Y);
                sound_play(VOICE_DRAW);
            }
            if (end_time > 260 && (p1->state == ST_WIN || p2->state == ST_WIN || p1->state == ST_KO || p2->state == ST_KO || winner == 2)) {
                word_hide();
                return winner;
            }
        }
    }
}

/* Best of three. Returns the winner (0/1), or 3 when a challenger interrupts. */
u8 play_match(u8 char1, u8 char2, u8 cpu1, u8 cpu2) {
    Fighter *p1 = &fighters[0], *p2 = &fighters[1];
    video_init();
    fighter_init(p1, &characters[char1], 0, 0);
    fighter_init(p2, &characters[char2], 1, char1 == char2);
    p1->foe = p2;
    p2->foe = p1;
    p1->cpu = cpu1;
    p2->cpu = cpu2;
    challenger = 0;

    current_stage = stages[char2];
    stage_load(current_stage, char2);
    screens_load_palettes();
    screen_load(IMG_PORTRAIT(char1), SLOT_CUTIN1);
    screen_load(IMG_PORTRAIT(char2), SLOT_CUTIN2);
    hud_init(char1, char2);
    hud_wins(0, 0);
    sound_music(music_stage[char2]);

    for (u8 round = 0; round < 4; round++) {
        u8 w = play_round(p1->wins == 1 && p2->wins == 1 ? 2 : round > 1 ? 2 : round);
        if (w == 3) return 3;
        if (w < 2) fighters[w].wins++;
        hud_wins(p1->wins, p2->wins);
        if (p1->wins == 2 || p2->wins == 2) break;
    }
    hud_hide();
    if (p1->wins == p2->wins) return 1;     /* draw after all rounds: the cpu side wins */
    return p1->wins > p2->wins ? 0 : 1;
}

/* ---------------------------------------------------------------------------------------- modes */

#ifdef SCREEN_TEST
/* make EXTRA=-DSCREEN_TEST: walks through the screens a random soak rarely reaches (test/screen_test.lua) */
static void screen_test(void) {
    win_screen(CHAR_NYXA, CHAR_VORAX);
    boss_intro();
    vs_screen(CHAR_NYXA, CHAR_XAL);
    bios_user_mode = 1;          /* demo mode so the match ends after demo_frames */
    demo = 1;
    demo_frames = 1200;
    play_match(CHAR_NYXA, CHAR_XAL, 8, 8);
    demo = 0;
    bios_user_mode = 2;
    continue_screen(CHAR_NYXA);
    ending_screen(CHAR_BRUTOK);
}
#endif

#ifdef STAGE_TEST
/* make EXTRA=-DSTAGE_TEST: 10 s of CPU fight on every stage, in stage order (test/stage_test.lua) */
static void stage_test(void) {
    bios_user_mode = 1;
    demo = 1;
    for (u8 c = 0; c < CHARACTER_COUNT; c++) {
        demo_frames = 1200;
        play_match(c == CHAR_XAL ? CHAR_NYXA : (u8)((c + 1) % PLAYABLE_COUNT), c, 5, 5);
    }
    demo = 0;
    bios_user_mode = 2;
}
#endif

static void arcade(void) {
#ifdef STAGE_TEST
    stage_test();
    return;
#endif
#ifdef SCREEN_TEST
    screen_test();
    return;
#endif
    u8 human = bios_player_mod2 && !bios_player_mod1 ? 1 : 0;
    options_screen(human);
    u8 pick = select_screen(human);

    /* ladder: 6 of the other 7 fighters in random order, then the boss */
    u8 ladder[LADDER_FOES + 1];
    u8 pool[PLAYABLE_COUNT], count = 0;
    for (u8 i = 0; i < PLAYABLE_COUNT; i++) {
        if (i != pick) pool[count++] = i;
    }
    u16 seed = frame_count;
    for (u8 i = count - 1; i > 0; i--) {
        seed = seed * 25173 + 13849;
        u8 j = (u8)((seed >> 8) % (i + 1));
        u8 tmp = pool[i];
        pool[i] = pool[j];
        pool[j] = tmp;
    }
    for (u8 i = 0; i < LADDER_FOES; i++) ladder[i] = pool[i];
    ladder[LADDER_FOES] = CHAR_XAL;

    for (u8 stage = 0; stage <= LADDER_FOES;) {
        u8 foe = ladder[stage];
        if (foe == CHAR_XAL) boss_intro();
        vs_screen(human ? foe : pick, human ? pick : foe);
        /* cpu level 1..8 by difficulty and ladder position */
        static const u8 first_level[3] = {1, 2, 4}, boss_level[3] = {3, 6, 8};
        u8 level = foe == CHAR_XAL ? boss_level[difficulty] : (u8)(first_level[difficulty] + stage / (difficulty == EASY ? 3 : 2));
        u8 result = human ? play_match(foe, pick, level, 0) : play_match(pick, foe, 0, level);
        if (result == 3) {
            /* here comes a new challenger: versus match, then back to the ladder */
            challenger = 0;
            challenger_screen();
            u8 other = select_screen(1 - human);
            vs_screen(human ? other : pick, human ? pick : other);
            play_match(human ? other : pick, human ? pick : other, 0, 0);
            continue;
        }
        if (result == human) {
            win_screen(pick, foe);
            stage++;
        } else if (!continue_screen(pick)) {
            return;
        }
    }
    ending_screen(pick);
}

int main(void) {
    video_init();
    sound_init();
    if (bios_user_mode == USER_MODE_GAME) {
        arcade();
        bios_player_mod1 = bios_player_mod2 = 0;
        return 0;
    }
    /* attract: story intro, then a cpu demo match. If start is pressed meanwhile, the game must begin
     * right here: returning to the BIOS in game mode makes it drop back to the demo (seen in MAME: mode 2 -> 1). */
    if (!intro_story() && bios_user_mode != USER_MODE_GAME) demo_match();
    if (bios_user_mode == USER_MODE_GAME) {
        arcade();
        bios_player_mod1 = bios_player_mod2 = 0;
    }
    return 0;
}

int main_mvs_title(void) {
    video_init();
    sound_init();
    title_screen();
    if (bios_user_mode == USER_MODE_GAME) {
        arcade();
        bios_player_mod1 = bios_player_mod2 = 0;
    }
    return 0;
}
