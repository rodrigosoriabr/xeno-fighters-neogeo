#include <ngdevkit/neogeo.h>
#include "sound.h"

#define QUEUE 8
static u8 queue[QUEUE];
static u8 head, count;

void sound_init(void) {
    head = count = 0;
    *REG_SOUND = 3;             /* reset the driver */
}

void sound_play(u8 command) {
    if (command == 0) return;
    if (count == QUEUE) return; /* drop rather than lag behind the action */
    queue[(head + count) % QUEUE] = command;
    count++;
}

void sound_music(u8 command) {
    sound_play(command);
}

void sound_frame(void) {
    if (count) {
        *REG_SOUND = queue[head];
        head = (head + 1) % QUEUE;
        count--;
    }
}
