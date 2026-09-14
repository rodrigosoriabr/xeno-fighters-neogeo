/* Stages: 3 horizontal bands (sky, crowd/buildings, floor) of 21 sprite columns each, scrolling at
 * different speeds for parallax. Bands never share a scanline, so a stage costs 21 sprites per line.
 * On top of the painting the stage lives: a row of spectator sprites that cheer on big moments,
 * palette-cycled glow on its brightest colors (lava, neon, lanterns) and ambient particles. */
#ifndef STAGE_H
#define STAGE_H

#include "video.h"

typedef struct {
    u8 frame;          /* band image */
    u8 speed;          /* scroll speed in 1/16 of the camera */
    s16 y;             /* top of the band in stage pixels */
} Band;

enum { MOTION_DRIFT, MOTION_RISE, MOTION_FALL, MOTION_TWINKLE };

typedef struct {
    ImageSet images;
    const u16 *palettes;
    u8 palette_count;
    s16 width, height;
    s16 ground;        /* stage pixel row of the fighters' floor line */
    Band bands[3];
    u16 backdrop;
    u8 crowd;          /* spectator set (crowd_data.h), in stage order */
    u8 particle;       /* FXA_P_* */
    u8 motion;         /* MOTION_* */
    const u16 *glow;   /* palette entries (palette * 16 + color) that pulse */
    u8 glow_count;
} Stage;

extern s16 stage_width;
extern s16 camera_x, camera_y;

void stage_load(const Stage *s, u8 unused);
void stage_draw(void);
void stage_draw_front(s16 camera_x, s16 camera_y);   /* crowd and particles (after the fighters) */
void stage_update(void);
void stage_hide(void);
/* camera follows the midpoint of the fighters and rises for jumps */
void stage_camera(s16 x1, s16 x2, s16 height);
/* spectators cheer for `frames` */
void crowd_cheer(u8 frames);

#endif
