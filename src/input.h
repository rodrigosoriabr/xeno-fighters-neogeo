/* Player input: raw joystick + buttons each frame, relative to the way the fighter faces, with a
 * history buffer for motion commands (quarter circles, dragon punch, double taps). */
#ifndef INPUT_H
#define INPUT_H

#include <ngdevkit/types.h>

/* directions in numpad notation relative to facing: 4 back, 6 forward, 2 down, 8 up */
#define BTN_A 1   /* light punch */
#define BTN_B 2   /* light kick */
#define BTN_C 4   /* heavy punch */
#define BTN_D 8   /* heavy kick */

#define HISTORY 32

typedef struct {
    u8 dir;            /* 1..9 relative to facing, 5 = neutral */
    u8 held;           /* buttons held (BTN_*) */
    u8 pressed;        /* buttons pressed this frame */
    u8 dirs[HISTORY];  /* direction history, dirs[head] is the newest */
    u8 head;
    u8 press_age[4];   /* frames since each button was last pressed (255 = long ago) */
} Input;

/* raw: BIOS bits (up down left right A B C D); facing_right selects what "forward" is */
void input_update(Input *in, u8 raw, u8 facing_right);
void input_clear(Input *in);

/* true when the motion (digits in numpad notation, e.g. "236") ended within `window` frames */
u8 input_motion(const Input *in, const char *motion, u8 window);
/* forward or back tapped twice quickly (6 5 6 / 4 5 4) */
u8 input_double_tap(const Input *in, u8 dir);
/* a button pressed recently (buffered press, `frames` of leniency) */
u8 input_buffered(const Input *in, u8 buttons, u8 frames);

#endif
