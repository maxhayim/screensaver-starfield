/*
 * A small software canvas for the Windows and Linux savers: a 32-bit pixel
 * buffer with anti-aliased lines and dots, a fade, and a drawn clock. macOS
 * uses Core Graphics instead.
 *
 * Pixels are 0x00RRGGBB, which in memory is B, G, R, X: the layout of a
 * 32-bit Windows DIB and of a TrueColor X11 image.
 */
#ifndef SF_CANVAS_H
#define SF_CANVAS_H

#include <stdint.h>

#include "starfield.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t *pixels;
    int width, height; /* in pixels */
    int stride;        /* in pixels */
    float scale;       /* pixels per point */
} sf_canvas;

/* Allocates the pixels. Returns 0 if out of memory. */
int sf_canvas_init(sf_canvas *c, int width, int height, float scale);
/* Wraps pixels owned by someone else (an X shared-memory image, say). */
void sf_canvas_wrap(sf_canvas *c, uint32_t *pixels, int width, int height, int stride, float scale);
void sf_canvas_free(sf_canvas *c);

void sf_canvas_clear(sf_canvas *c, sf_color color);
void sf_canvas_copy(sf_canvas *dst, const sf_canvas *src);

/* Draw with the starfield core: sf_render(s, &renderer). Coordinates are in points. */
sf_renderer sf_canvas_renderer(sf_canvas *c);

/*
 * The clock overlay: `text` (digits, ':', ' ', 'A', 'P', 'M') with its
 * bottom-left corner at (x, bottom), `height` points tall.
 */
void sf_canvas_text(sf_canvas *c, const char *text, float x, float bottom, float height, sf_color color);
float sf_text_width(const char *text, float height);

/* The local time as "7:05 PM", or "19:05" when use_24h is set. */
void sf_format_time(char *out, int size, int use_24h);

#ifdef __cplusplus
}
#endif

#endif
