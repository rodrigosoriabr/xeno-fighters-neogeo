#include "input.h"

void input_clear(Input *in) {
    in->dir = 5;
    in->held = in->pressed = 0;
    in->head = 0;
    for (u8 i = 0; i < HISTORY; i++) {
        in->dirs[i] = 5;
    }
    for (u8 i = 0; i < 4; i++) {
        in->press_age[i] = 255;
    }
}

void input_update(Input *in, u8 raw, u8 facing_right) {
    s8 h = 0, v = 0;
    if (raw & 1) v = 1;            /* up */
    if (raw & 2) v = -1;           /* down */
    if (raw & 4) h = -1;           /* left */
    if (raw & 8) h = 1;            /* right */
    if (!facing_right) h = -h;
    in->dir = (u8)(5 + h + 3 * v);

    u8 buttons = raw >> 4;
    in->pressed = buttons & ~in->held;
    in->held = buttons;
    for (u8 i = 0; i < 4; i++) {
        if (in->pressed & (1 << i)) {
            in->press_age[i] = 0;
        } else if (in->press_age[i] < 255) {
            in->press_age[i]++;
        }
    }
    in->head = (in->head + 1) % HISTORY;
    in->dirs[in->head] = in->dir;
}

/* Walks the history backwards looking for the motion's digits in reverse order. Diagonals next to
 * a required cardinal count as that cardinal's neighbours are accepted by fighting game convention
 * only when they are the required digit, so "236" needs 2, 3, 6 in order (other inputs may sit
 * between them, as long as the whole motion fits in `window` frames). */
u8 input_motion(const Input *in, const char *motion, u8 window) {
    u8 len = 0;
    while (motion[len]) len++;
    s8 want = (s8)len - 1;
    for (u8 age = 0; age < window && age < HISTORY; age++) {
        u8 d = in->dirs[(in->head + HISTORY - age) % HISTORY];
        if (d == (u8)(motion[want] - '0')) {
            if (want == 0) return 1;
            want--;
        } else if (want == (s8)len - 1 && age > 3) {
            return 0;              /* the motion must have finished in the last few frames */
        }
    }
    return 0;
}

u8 input_double_tap(const Input *in, u8 dir) {
    /* newest frames: dir held now, a neutral gap, dir again, all within 14 frames */
    u8 stage = 0;
    for (u8 age = 0; age < 14; age++) {
        u8 d = in->dirs[(in->head + HISTORY - age) % HISTORY];
        if (stage == 0) {
            if (age == 0 && d != dir) return 0;
            if (d != dir) stage = 1;
        }
        if (stage == 1) {
            if (d == dir) stage = 2;
            else if (d != 5) return 0;
        }
        if (stage == 2 && d != dir) {
            return age > 2;
        }
    }
    return 0;
}

u8 input_buffered(const Input *in, u8 buttons, u8 frames) {
    for (u8 i = 0; i < 4; i++) {
        if ((buttons & (1 << i)) && in->press_age[i] <= frames) return 1;
    }
    return 0;
}
