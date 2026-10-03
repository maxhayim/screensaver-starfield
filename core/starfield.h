/*
 * Starfield — the drawing core shared by the macOS, Windows, and Linux savers.
 *
 * The core owns the stars and draws through three callbacks, so each platform
 * only supplies "fill", "line", and "circle" with its native 2D API.
 * Coordinates are in points; the host scales to pixels.
 *
 * The trails come from never clearing the screen: each frame starts with a
 * translucent fill of the background color over the last frame, like a CRT's
 * afterglow. So the host must keep its drawing surface between frames.
 */
#ifndef STARFIELD_H
#define STARFIELD_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { float r, g, b, a; } sf_color;

typedef struct {
    void *ctx;
    /* Cover the whole surface with c (c.a < 1 for the afterglow fade). */
    void (*fill)(void *ctx, sf_color c);
    void (*line)(void *ctx, float x0, float y0, float x1, float y1, float width, sf_color c);
    void (*circle)(void *ctx, float x, float y, float radius, sf_color c);
} sf_renderer;

typedef struct {
    sf_color background;
    sf_color star;      /* most stars */
    sf_color accent;    /* the odd one out (orange on the site) */
    float accent_share; /* 0..1, share of accent stars; the site uses 0.07 */
    float speed;        /* 1 = the site's speed */
    float trails;       /* 0..1, how long the streaks linger; the site uses 0.58 */
    int reduced_motion; /* slower flight */
    int compact;        /* small preview thumbnail */
} sf_options;

typedef struct starfield starfield;

starfield *sf_create(unsigned seed);
void sf_destroy(starfield *s);

/* Defaults (the original look): #0b0b0a background, #eeebe4 stars, #f06a2a accent. */
sf_options sf_default_options(void);
void sf_set_options(starfield *s, const sf_options *opts);
void sf_resize(starfield *s, float width, float height);

void sf_step(starfield *s, float dt);
void sf_render(starfield *s, const sf_renderer *r);

/* After a resize or color change the host should clear its surface; this says so once. */
int sf_take_needs_clear(starfield *s);

int sf_star_count(const starfield *s);
int sf_accent_count(const starfield *s);

/* Parse "#rrggbb" or "#rrggbbaa". Returns 0 on bad input. */
int sf_parse_color(const char *hex, sf_color *out);

#ifdef __cplusplus
}
#endif

#endif
