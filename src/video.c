#include <ngdevkit/neogeo.h>
#include "video.h"

#define PALETTE_RAM ((volatile u16 *)0x400000)
#define BACKDROP ((volatile u16 *)0x401ffe)

u16 frame_count;
static volatile u8 vblank_flag;

/* Called by the runtime's VBlank interrupt (it also acks the IRQ and kicks the watchdog). */
void rom_callback_VBlank(void) {
    vblank_flag = 1;
}

void wait_frame(void) {
    while (!vblank_flag) {
    }
    vblank_flag = 0;
    frame_count++;
}

/* What each sprite block shows now, so SCB1 (tiles) is only rewritten when the image changes. */
typedef struct {
    const ImageSet *set;
    u16 frame;
    u8 flip, palette, cols;
} BlockCache;

static BlockCache cache[SPR_LAST];

void video_init(void) {
    for (u16 i = 0; i < SPR_LAST; i++) {
        cache[i].set = 0;
    }
    *REG_VRAMADDR = ADDR_SCB2;
    *REG_VRAMMOD = 1;
    for (u16 i = 0; i < SPR_LAST; i++) {
        *REG_VRAMRW = 0x0fff;     /* full size */
    }
    hide_sprites(0, SPR_LAST);
    fix_clear();
}

void set_palettes(u16 first, const u16 *colors, u16 count) {
    volatile u16 *p = PALETTE_RAM + first * 16;
    for (u16 i = 0; i < count * 16; i++) {
        *p++ = colors[i];
    }
}

/* Neo Geo color word -> scaled toward black. level 16 = original, 0 = black. */
static u16 dim(u16 c, u8 level) {
    u16 r = ((c >> 8) & 15) << 1 | ((c >> 14) & 1);
    u16 g = ((c >> 4) & 15) << 1 | ((c >> 13) & 1);
    u16 b = (c & 15) << 1 | ((c >> 12) & 1);
    r = r * level >> 4;
    g = g * level >> 4;
    b = b * level >> 4;
    return (r & 1) << 14 | (g & 1) << 13 | (b & 1) << 12 | (r >> 1) << 8 | (g >> 1) << 4 | (b >> 1);
}

void fade_palettes(u16 first, const u16 *colors, u16 count, u8 level) {
    volatile u16 *p = PALETTE_RAM + first * 16;
    for (u16 i = 0; i < count * 16; i++) {
        *p++ = (i & 15) ? dim(colors[i], level) : colors[i];
    }
}

static u16 brighten(u16 c, u8 level) {
    u16 r = ((c >> 8) & 15) << 1 | ((c >> 14) & 1);
    u16 g = ((c >> 4) & 15) << 1 | ((c >> 13) & 1);
    u16 b = (c & 15) << 1 | ((c >> 12) & 1);
    r += (31 - r) * level >> 4;
    g += (31 - g) * level >> 4;
    b += (31 - b) * level >> 4;
    return (r & 1) << 14 | (g & 1) << 13 | (b & 1) << 12 | (r >> 1) << 8 | (g >> 1) << 4 | (b >> 1);
}

void flash_palettes(u16 first, const u16 *colors, u16 count, u8 level) {
    volatile u16 *p = PALETTE_RAM + first * 16;
    for (u16 i = 0; i < count * 16; i++) {
        *p++ = (i & 15) ? brighten(colors[i], level) : colors[i];
    }
}

void set_backdrop(u16 color) {
    *BACKDROP = color;
}

void hide_sprites(u16 first, u16 count) {
    *REG_VRAMADDR = ADDR_SCB3 + first;
    *REG_VRAMMOD = 1;
    for (u16 i = 0; i < count; i++) {
        *REG_VRAMRW = 0;          /* height 0 = not drawn */
    }
    for (u16 i = first; i < first + count && i < SPR_LAST; i++) {
        cache[i].set = 0;
    }
}

static inline u16 scb3(s16 y, u8 rows) {
    return SCB3_Y(y) | rows;
}

u8 draw_frame(u16 first, u8 max_cols, const ImageSet *set, u16 f, s16 x, s16 y, u8 flip, u8 palette) {
    const Frame *fr = &set->frames[f];
    u8 cols = fr->cols < max_cols ? fr->cols : max_cols;
    BlockCache *bc = &cache[first];
    if (bc->set != set || bc->frame != f || bc->flip != flip || bc->palette != palette) {
        const u16 *cells = set->cells + fr->cell;
        const u8 *attrs = set->attrs + fr->cell;
        for (u8 c = 0; c < cols; c++) {
            /* sprite c always shows image column c; mirroring only changes where it is placed
             * (right to left, below) and flips each tile */
            u8 src = c;
            *REG_VRAMADDR = ADDR_SCB1 + (first + c) * 64;
            *REG_VRAMMOD = 1;
            for (u8 r = 0; r < fr->rows; r++) {
                u16 cell = cells[src * fr->rows + r];
                u8 attr = attrs[src * fr->rows + r];
                u32 tile = cell ? set->tile_base + cell : TILE_EMPTY;
                *REG_VRAMRW = (u16)tile;
                *REG_VRAMRW = (u16)(palette + (attr & 15)) << 8 | ((tile >> 16) & 15) << 4 | (((attr >> 4) ^ flip) & 1);
            }
        }
        if (bc->cols > cols) {
            hide_sprites(first + cols, bc->cols - cols);
        }
        bc->set = set;
        bc->frame = f;
        bc->flip = flip;
        bc->palette = palette;
        bc->cols = cols;
    }
    s16 top = y + fr->y;
    for (u8 c = 0; c < cols; c++) {
        s16 left = flip ? x - fr->x - (c + 1) * 16 : x + fr->x + c * 16;
        *REG_VRAMADDR = ADDR_SCB3 + first + c;
        *REG_VRAMMOD = 0x200;      /* SCB3 -> SCB4 of the same sprite */
        *REG_VRAMRW = scb3(top, fr->rows);
        *REG_VRAMRW = (u16)(left & 0x1ff) << 7;
    }
    return cols;
}

/* Hardware zoom: SCB2 holds horizontal (0-15: tile width - 1) and vertical (0-255) scale. A shrunk
 * column is `scale` px wide, so columns are placed at that pitch around the anchor. */
u8 draw_frame_scaled(u16 first, u8 max_cols, const ImageSet *set, u16 f, s16 x, s16 y, u8 palette, u8 scale) {
    const Frame *fr = &set->frames[f];
    if (scale >= 16) {
        unscale_sprites(first, max_cols);
        return draw_frame(first, max_cols, set, f, x, y, 0, palette);
    }
    if (scale == 0) scale = 1;
    u8 cols = draw_frame(first, max_cols, set, f, x, y, 0, palette);
    s16 top = y + (fr->y * scale >> 4);
    s16 left = x + (fr->x * scale >> 4);
    *REG_VRAMADDR = ADDR_SCB2 + first;
    *REG_VRAMMOD = 1;
    for (u8 c = 0; c < cols; c++) {
        *REG_VRAMRW = (u16)(scale - 1) << 8 | (u16)(scale * 16 - 1);
    }
    for (u8 c = 0; c < cols; c++) {
        *REG_VRAMADDR = ADDR_SCB3 + first + c;
        *REG_VRAMMOD = 0x200;
        *REG_VRAMRW = scb3(top, fr->rows);
        *REG_VRAMRW = (u16)((left + c * scale) & 0x1ff) << 7;
    }
    return cols;
}

void unscale_sprites(u16 first, u16 count) {
    *REG_VRAMADDR = ADDR_SCB2 + first;
    *REG_VRAMMOD = 1;
    for (u16 i = 0; i < count; i++) {
        *REG_VRAMRW = 0x0fff;
    }
}

void move_frame(u16 first, u8 cols, s16 x, s16 y) {
    for (u8 c = 0; c < cols; c++) {
        *REG_VRAMADDR = ADDR_SCB3 + first + c;
        *REG_VRAMMOD = 0x200;
        *REG_VRAMRW = (*REG_VRAMRW & 0x3f) | SCB3_Y(y);
        *REG_VRAMRW = (u16)((x + c * 16) & 0x1ff) << 7;
    }
}

void fix_put(u8 x, u8 y, u8 palette, u16 tile) {
    *REG_VRAMADDR = ADDR_FIXMAP + x * 32 + y;
    *REG_VRAMRW = (u16)palette << 12 | tile;
}

void fix_text(u8 x, u8 y, u8 palette, const char *text) {
    *REG_VRAMADDR = ADDR_FIXMAP + x * 32 + y;
    *REG_VRAMMOD = 32;             /* next column */
    while (*text) {
        *REG_VRAMRW = (u16)palette << 12 | (u8)*text++;
    }
}

void fix_clear(void) {
    *REG_VRAMADDR = ADDR_FIXMAP;
    *REG_VRAMMOD = 1;
    for (u16 i = 0; i < 40 * 32; i++) {
        *REG_VRAMRW = 0x00ff;      /* tile 255 = blank in the base S-ROM */
    }
}
