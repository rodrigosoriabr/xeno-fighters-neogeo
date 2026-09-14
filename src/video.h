/* Neo Geo video: sprites (columns of 16x16 tiles), palettes, fix layer, frame pacing.
 *
 * Sprite slots are fixed per use so the per-scanline limit (96) is not reached in practice:
 *   1-63    stage, 3 bands x 21 columns (bands never share a scanline)
 *   64-93   crowd (spectators in front of the middle band)
 *   94-101  shadows
 *   102-117 afterimage (rush and super moves)
 *   118-149 fighters, 16 columns each (the one in front gets the higher block)
 *   150-189 projectiles and hit sparks
 *   190-199 ambient particles
 *   200-259 HUD frames, icons, name plates
 *   260-380 screens: title, select, VS, story, lettering (in front of everything)
 * A higher sprite number is drawn in front of a lower one.
 *
 * Palettes (256 x 16 colors):
 *   0-15 fix layer (HUD)   16-19 player 1   20-23 player 1 afterimage   24-27 player 2   28-31 p2 afterimage
 *   32-47 stage   48-55 crowd   64-75 effects   76-95 HUD images   96-255 screens (loaded per screen)
 */
#ifndef VIDEO_H
#define VIDEO_H

#include <ngdevkit/types.h>

#define SCREEN_W 320
#define SCREEN_H 224
/* The MVS shows video lines 16-239. Sprite y in this code is the visible screen line (0 = top, same as
 * fix row 2); SCB3 counts from line 0, so SPRITE_Y() adds SCREEN_TOP. Until 14/09 the HUD frames sat
 * 16 px below the fix-layer bars they surround: the fighters had been tuned by eye and hid the offset. */
#define SCREEN_TOP 16
#define SCB3_Y(y) ((u16)((480 + SCREEN_TOP - (y)) & 0x1ff) << 7)

#define SPR_STAGE 1
#define SPR_CROWD 64
#define SPR_CROWD_COUNT 30
#define SPR_SHADOW 94
#define SPR_AFTER 102
#define SPR_FIGHTER 118
#define SPR_FX 150
#define SPR_FX_COUNT 40
#define SPR_PARTICLE 190
#define SPR_PARTICLE_COUNT 10
#define SPR_HUD 200
#define SPR_HUD_COUNT 60
#define SPR_SCREEN 260
#define SPR_SCREEN_COUNT 121
#define SPR_LAST 381         /* the Neo Geo has sprites 0-380 */

/* C-ROM tiles 0-255 hold the eye-catcher logo and tile 0 is NOT blank: empty cells use 255 */
#define TILE_EMPTY 255

#define PAL_P1 16
#define PAL_P2 24
#define PAL_AFTER 4           /* + PAL_P1 / PAL_P2 */
#define PAL_STAGE 32
#define PAL_CROWD 48
#define PAL_FX 64
#define PAL_HUD 76
#define PAL_SCREEN 96

/* A drawable image: `cols` sprite columns of `rows` tiles. cells/attrs are column-major;
 * cell 0 = empty, otherwise tile_base + cell - 1... see video.c draw_image(). */
typedef struct {
    s16 x, y;          /* top-left relative to the anchor (feet for fighters) */
    u8 cols, rows;
    u16 cell;          /* first cell index */
    s16 hurt[4];       /* x1, y1, x2, y2 relative to anchor, y up is negative */
    s16 hit[4];        /* all zero = no attack box */
} Frame;

typedef struct {
    const Frame *frames;
    const u16 *cells;
    const u8 *attrs;   /* low nibble: palette within the image set, bit 4: horizontal flip */
    u32 tile_base;     /* global C-ROM tile of local tile 0 */
} ImageSet;

extern u16 frame_count;

void video_init(void);
void wait_frame(void);
void set_palettes(u16 first, const u16 *colors, u16 count);
void fade_palettes(u16 first, const u16 *colors, u16 count, u8 level); /* level 0..16 */
void set_backdrop(u16 color);

/* Draws frame `f` of `set` with sprites [first, first + max_cols), anchor at screen (x, y),
 * mirrored when `flip`; unused columns of the block are hidden. Returns columns used. */
u8 draw_frame(u16 first, u8 max_cols, const ImageSet *set, u16 f, s16 x, s16 y, u8 flip, u8 palette);
void move_frame(u16 first, u8 cols, s16 x, s16 y);
/* Same, shrunk by the hardware (SCB2) to scale/16 around the frame anchor, 1..16. Call unscale_sprites()
 * on the block afterwards: every other drawing assumes full size. */
u8 draw_frame_scaled(u16 first, u8 max_cols, const ImageSet *set, u16 f, s16 x, s16 y, u8 palette, u8 scale);
void unscale_sprites(u16 first, u16 count);
void flash_palettes(u16 first, const u16 *colors, u16 count, u8 level);  /* level 0..16 toward white */
void hide_sprites(u16 first, u16 count);

/* fix layer, 40x32 tiles of 8x8 */
void fix_put(u8 x, u8 y, u8 palette, u16 tile);
void fix_text(u8 x, u8 y, u8 palette, const char *text);
void fix_clear(void);

#endif
