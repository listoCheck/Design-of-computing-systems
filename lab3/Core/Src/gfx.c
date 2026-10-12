#include "gfx.h"
#include <string.h>
/* Hand-specified 5x7 column glyphs, bit 0 is the upper pixel. */
static const char characters[] = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ:/.-?";
static const uint8_t glyphs[][5] = {
 {0,0,0,0,0}, {0x3E,0x51,0x49,0x45,0x3E}, {0,0x42,0x7F,0x40,0},
 {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31}, {0x18,0x14,0x12,0x7F,0x10},
 {0x27,0x45,0x45,0x45,0x39}, {0x3C,0x4A,0x49,0x49,0x30}, {1,0x71,9,5,3},
 {0x36,0x49,0x49,0x49,0x36}, {6,0x49,0x49,0x29,0x1E},
 {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36}, {0x3E,0x41,0x41,0x41,0x22},
 {0x7F,0x41,0x41,0x22,0x1C}, {0x7F,0x49,0x49,0x49,0x41}, {0x7F,9,9,9,1},
 {0x3E,0x41,0x49,0x49,0x7A}, {0x7F,8,8,8,0x7F}, {0,0x41,0x7F,0x41,0},
 {0x20,0x40,0x41,0x3F,1}, {0x7F,8,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40},
 {0x7F,2,0x0C,2,0x7F}, {0x7F,4,8,0x10,0x7F}, {0x3E,0x41,0x41,0x41,0x3E},
 {0x7F,9,9,9,6}, {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,9,0x19,0x29,0x46},
 {0x46,0x49,0x49,0x49,0x31}, {1,1,0x7F,1,1}, {0x3F,0x40,0x40,0x40,0x3F},
 {0x1F,0x20,0x40,0x20,0x1F}, {0x3F,0x40,0x38,0x40,0x3F}, {0x63,0x14,8,0x14,0x63},
 {7,8,0x70,8,7}, {0x61,0x51,0x49,0x45,0x43},
 {0,0x36,0x36,0,0}, {0x20,0x10,8,4,2}, {0,0x60,0x60,0,0}, {8,8,8,8,8}, {2,1,0x51,9,6}
};
_Static_assert(sizeof glyphs / sizeof glyphs[0] == sizeof characters - 1U, "font size");
void gfx_clear(Canvas *canvas) { memset(canvas, 0, sizeof *canvas); }
void gfx_pixel(Canvas *canvas, int x, int y, bool on)
{
    if (x < 0 || y < 0 || x >= GFX_WIDTH || y >= GFX_HEIGHT) return;
    uint8_t *byte = &canvas->bytes[x + (y / 8) * GFX_WIDTH];
    if (on) *byte |= (uint8_t)(1U << (y % 8)); else *byte &= (uint8_t)~(1U << (y % 8));
}
void gfx_line(Canvas *canvas, int x0, int y0, int x1, int y1, bool on)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1, error = dx + dy;
    for (;;) {
        gfx_pixel(canvas, x0, y0, on); if (x0 == x1 && y0 == y1) break;
        int twice = 2 * error;
        if (twice >= dy) { error += dy; x0 += sx; }
        if (twice <= dx) { error += dx; y0 += sy; }
    }
}
void gfx_rect(Canvas *canvas, int x, int y, int width, int height, bool on)
{
    if (width < 1 || height < 1) return;
    gfx_line(canvas, x, y, x + width - 1, y, on); gfx_line(canvas, x, y, x, y + height - 1, on);
    gfx_line(canvas, x, y + height - 1, x + width - 1, y + height - 1, on);
    gfx_line(canvas, x + width - 1, y, x + width - 1, y + height - 1, on);
}
void gfx_text(Canvas *canvas, int x, int y, const char *text, unsigned scale)
{
    if (!scale || scale > 4) return;
    for (; *text; ++text, x += (int)(6U * scale)) {
        const char *p = strchr(characters, *text); size_t index = p ? (size_t)(p - characters) : sizeof glyphs / sizeof glyphs[0] - 1U;
        for (unsigned column = 0; column < 5; ++column) for (unsigned row = 0; row < 7; ++row)
            for (unsigned i = 0; i < scale; ++i) for (unsigned j = 0; j < scale; ++j)
                gfx_pixel(canvas, x + (int)(column * scale + i), y + (int)(row * scale + j), (glyphs[index][column] & (1U << row)) != 0);
    }
}
