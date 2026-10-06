/*
 * Starfield for web pages: the core compiled to WebAssembly. Drawing goes
 * out through three imports from the module "host" (see web/index.js);
 * nothing else is imported.
 */
#include "presets.h"
#include "starfield.h"

#define IMPORT(name) __attribute__((import_module("host"), import_name(name)))
#define EXPORT(name) __attribute__((export_name(name)))

IMPORT("fill") void js_fill(float r, float g, float b, float a);
IMPORT("line") void js_line(float x0, float y0, float x1, float y1, float width, float r, float g, float b, float a);
IMPORT("circle") void js_circle(float x, float y, float radius, float r, float g, float b, float a);

static void fill(void *c, sf_color k) {
    (void)c;
    js_fill(k.r, k.g, k.b, k.a);
}
static void line(void *c, float x0, float y0, float x1, float y1, float w, sf_color k) {
    (void)c;
    js_line(x0, y0, x1, y1, w, k.r, k.g, k.b, k.a);
}
static void circle(void *c, float x, float y, float rad, sf_color k) {
    (void)c;
    js_circle(x, y, rad, k.r, k.g, k.b, k.a);
}
static const sf_renderer RENDERER = {0, fill, line, circle};

EXPORT("create") starfield *create(unsigned seed) { return sf_create(seed); }
EXPORT("destroy") void destroy(starfield *s) { sf_destroy(s); }
EXPORT("resize") void resize(starfield *s, float w, float h) { sf_resize(s, w, h); }
EXPORT("step") void step(starfield *s, float dt) { sf_step(s, dt); }
EXPORT("render") void render(starfield *s) { sf_render(s, &RENDERER); }
EXPORT("take_needs_clear") int take_needs_clear(starfield *s) { return sf_take_needs_clear(s); }

/* Colors as 0..1 floats; the rest as in sf_options. */
EXPORT("set_options")
void set_options(starfield *s, float br, float bg, float bb, float sr, float sg, float sb, float ar, float ag, float ab,
                 float accent_share, float speed, float trails, int reduced_motion, int compact) {
    sf_options o = sf_default_options();
    o.background = (sf_color){br, bg, bb, 1};
    o.star = (sf_color){sr, sg, sb, 1};
    o.accent = (sf_color){ar, ag, ab, 1};
    o.accent_share = accent_share;
    o.speed = speed;
    o.trails = trails;
    o.reduced_motion = reduced_motion;
    o.compact = compact;
    sf_set_options(s, &o);
}

EXPORT("preset_count") int preset_count(void) { return SF_PRESET_COUNT; }
EXPORT("star_count") int star_count(starfield *s) { return sf_star_count(s); }
