#ifndef GFX_H
#define GFX_H
#include <stdbool.h>
#include <stdint.h>
enum { GFX_WIDTH = 128, GFX_HEIGHT = 64, GFX_BYTES = 1024 };
typedef struct { uint8_t bytes[GFX_BYTES]; } Canvas;
void gfx_clear(Canvas *canvas);
void gfx_pixel(Canvas *canvas, int x, int y, bool on);
void gfx_line(Canvas *canvas, int x0, int y0, int x1, int y1, bool on);
void gfx_rect(Canvas *canvas, int x, int y, int width, int height, bool on);
void gfx_text(Canvas *canvas, int x, int y, const char *text, unsigned scale);
#endif
