/* Software canvas: see canvas.h. */
#include "canvas.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int sf_canvas_init(sf_canvas *c, int width, int height, float scale) {
    c->pixels = NULL;
    c->width = c->height = c->stride = 0;
    c->scale = scale > 0 ? scale : 1;
    if (width <= 0 || height <= 0) return 0;
    c->pixels = calloc((size_t)width * (size_t)height, sizeof *c->pixels);
    if (!c->pixels) return 0;
    c->width = width;
    c->height = height;
    c->stride = width;
    return 1;
}

void sf_canvas_wrap(sf_canvas *c, uint32_t *pixels, int width, int height, int stride, float scale) {
    c->pixels = pixels;
    c->width = width;
    c->height = height;
    c->stride = stride;
    c->scale = scale > 0 ? scale : 1;
}

void sf_canvas_free(sf_canvas *c) {
    free(c->pixels);
    c->pixels = NULL;
    c->width = c->height = c->stride = 0;
}

static int to8(float v) {
    int i = (int)(v * 255.0f + 0.5f);
    return i < 0 ? 0 : (i > 255 ? 255 : i);
}

static uint32_t pack(sf_color k) { return (uint32_t)to8(k.r) << 16 | (uint32_t)to8(k.g) << 8 | (uint32_t)to8(k.b); }

void sf_canvas_clear(sf_canvas *c, sf_color color) {
    uint32_t v = pack(color);
    for (int y = 0; y < c->height; y++) {
        uint32_t *row = c->pixels + (size_t)y * c->stride;
        for (int x = 0; x < c->width; x++) row[x] = v;
    }
}

void sf_canvas_copy(sf_canvas *dst, const sf_canvas *src) {
    int w = dst->width < src->width ? dst->width : src->width;
    int h = dst->height < src->height ? dst->height : src->height;
    for (int y = 0; y < h; y++)
        memcpy(dst->pixels + (size_t)y * dst->stride, src->pixels + (size_t)y * src->stride, (size_t)w * sizeof(uint32_t));
}

/* Move one channel a share `a` (0..256) of the way to `to`, by at least one step so fades finish. */
static uint32_t mix(uint32_t from, uint32_t to, int a) {
    int d = (int)to - (int)from;
    if (!d || !a) return from;
    int step = (d * a) / 256;
    if (!step) step = d > 0 ? 1 : -1;
    return (uint32_t)((int)from + step);
}

static void blend(uint32_t *p, uint32_t color, int a) {
    uint32_t v = *p;
    uint32_t r = mix(v >> 16 & 0xff, color >> 16 & 0xff, a);
    uint32_t g = mix(v >> 8 & 0xff, color >> 8 & 0xff, a);
    uint32_t b = mix(v & 0xff, color & 0xff, a);
    *p = r << 16 | g << 8 | b;
}

static void fill_cb(void *ctx, sf_color k) {
    sf_canvas *c = ctx;
    int a = (int)(k.a * 256.0f + 0.5f);
    if (a <= 0) return;
    if (a >= 256) {
        sf_canvas_clear(c, k);
        return;
    }
    uint32_t color = pack(k);
    for (int y = 0; y < c->height; y++) {
        uint32_t *row = c->pixels + (size_t)y * c->stride;
        for (int x = 0; x < c->width; x++) blend(&row[x], color, a);
    }
}

/*
 * A round-capped line from (x0, y0) to (x1, y1) in pixels, anti-aliased by
 * distance. Each covered pixel is handed to `plot` with its coverage (0..1).
 */
typedef void (*plot_fn)(void *ctx, int x, int y, float cover);

static void capsule(int width, int height, float x0, float y0, float x1, float y1, float r, plot_fn plot, void *ctx) {
    float pad = r + 1;
    int xa = (int)floorf(fminf(x0, x1) - pad), xb = (int)ceilf(fmaxf(x0, x1) + pad);
    int ya = (int)floorf(fminf(y0, y1) - pad), yb = (int)ceilf(fmaxf(y0, y1) + pad);
    if (xa < 0) xa = 0;
    if (ya < 0) ya = 0;
    if (xb > width - 1) xb = width - 1;
    if (yb > height - 1) yb = height - 1;

    float dx = x1 - x0, dy = y1 - y0;
    float len2 = dx * dx + dy * dy;
    for (int y = ya; y <= yb; y++) {
        float py = (float)y + 0.5f;
        for (int x = xa; x <= xb; x++) {
            float px = (float)x + 0.5f;
            float t = len2 > 0 ? ((px - x0) * dx + (py - y0) * dy) / len2 : 0;
            t = t < 0 ? 0 : (t > 1 ? 1 : t);
            float ex = px - (x0 + t * dx), ey = py - (y0 + t * dy);
            float cover = r + 0.5f - sqrtf(ex * ex + ey * ey);
            if (cover <= 0) continue;
            plot(ctx, x, y, cover > 1 ? 1 : cover);
        }
    }
}

typedef struct {
    sf_canvas *c;
    uint32_t color;
    float alpha;
} paint;

static void plot_blend(void *ctx, int x, int y, float cover) {
    paint *p = ctx;
    int a = (int)(cover * p->alpha * 256.0f + 0.5f);
    if (a > 0) blend(&p->c->pixels[(size_t)y * p->c->stride + x], p->color, a > 256 ? 256 : a);
}

static void stroke(sf_canvas *c, float x0, float y0, float x1, float y1, float width, sf_color k) {
    float r = width / 2;
    paint p = {c, pack(k), k.a};
    /* Hairlines get a 1-pixel core and fade instead of flickering. */
    if (r < 0.5f) {
        p.alpha *= r / 0.5f;
        r = 0.5f;
    }
    if (p.alpha > 0.002f) capsule(c->width, c->height, x0, y0, x1, y1, r, plot_blend, &p);
}

static void line_cb(void *ctx, float x0, float y0, float x1, float y1, float width, sf_color k) {
    sf_canvas *c = ctx;
    float s = c->scale;
    stroke(c, x0 * s, y0 * s, x1 * s, y1 * s, width * s, k);
}

static void circle_cb(void *ctx, float x, float y, float radius, sf_color k) {
    sf_canvas *c = ctx;
    float s = c->scale;
    stroke(c, x * s, y * s, x * s, y * s, radius * 2 * s, k);
}

sf_renderer sf_canvas_renderer(sf_canvas *c) {
    sf_renderer r = {c, fill_cb, line_cb, circle_cb};
    return r;
}

/* ---------- clock ---------- */

/*
 * A stroke font for the clock, drawn with the same round-capped lines.
 * Glyphs sit in a box 1 unit tall (y down); paths are point lists, and a
 * point with x < -1 ends a path.
 */
#define MAXPTS 160
#define BREAK (-9.0f)
#define DEG (3.14159265f / 180.0f)

typedef struct {
    float x[MAXPTS], y[MAXPTS];
    int n;
} path;

static void pt(path *p, float x, float y) {
    if (p->n < MAXPTS) {
        p->x[p->n] = x;
        p->y[p->n] = y;
        p->n++;
    }
}

static void brk(path *p) { pt(p, BREAK, 0); }

/* An elliptical arc from angle a0 to a1 (degrees, counterclockwise, 90 = top). */
static void arc(path *p, float cx, float cy, float rx, float ry, float a0, float a1) {
    int steps = (int)(fabsf(a1 - a0) / 12.0f) + 2;
    for (int i = 0; i <= steps; i++) {
        float t = (a0 + (a1 - a0) * (float)i / (float)steps) * DEG;
        pt(p, cx + rx * cosf(t), cy - ry * sinf(t));
    }
}

/* Builds glyph `ch` and returns its advance width. */
static float glyph(char ch, path *p) {
    p->n = 0;
    switch (ch) {
    case '0': arc(p, .28f, .5f, .28f, .5f, 0, 360); return .56f;
    case '1': pt(p, .1f, .2f); pt(p, .34f, 0); pt(p, .34f, 1); return .56f;
    case '2':
        arc(p, .28f, .28f, .27f, .28f, 165, -35);
        pt(p, .02f, 1); pt(p, .56f, 1);
        return .56f;
    case '3':
        arc(p, .27f, .25f, .25f, .25f, 155, -90);
        brk(p);
        arc(p, .27f, .73f, .28f, .27f, 90, -155);
        return .56f;
    case '4': pt(p, .44f, 1); pt(p, .44f, 0); pt(p, 0, .7f); pt(p, .56f, .7f); return .56f;
    case '5':
        pt(p, .52f, 0); pt(p, .07f, 0); pt(p, .05f, .45f);
        arc(p, .28f, .68f, .28f, .32f, 135, -145);
        return .56f;
    case '6':
        arc(p, .28f, .7f, .28f, .3f, 0, 360);
        brk(p);
        arc(p, .54f, .7f, .54f, .7f, 98, 180);
        return .56f;
    case '7': pt(p, 0, 0); pt(p, .56f, 0); pt(p, .18f, 1); return .56f;
    case '8':
        arc(p, .28f, .25f, .23f, .25f, 0, 360);
        brk(p);
        arc(p, .28f, .74f, .28f, .26f, 0, 360);
        return .56f;
    case '9':
        arc(p, .28f, .3f, .28f, .3f, 0, 360);
        brk(p);
        arc(p, .02f, .3f, .54f, .7f, 0, -82);
        return .56f;
    case ':': pt(p, .1f, .3f); pt(p, .1f, .3f); brk(p); pt(p, .1f, .86f); pt(p, .1f, .86f); return .2f;
    case 'A': pt(p, 0, 1); pt(p, .3f, 0); pt(p, .6f, 1); brk(p); pt(p, .11f, .66f); pt(p, .49f, .66f); return .6f;
    case 'P':
        pt(p, 0, 1); pt(p, 0, 0); pt(p, .27f, 0);
        arc(p, .27f, .27f, .27f, .27f, 90, -90);
        pt(p, 0, .54f);
        return .54f;
    case 'M': pt(p, 0, 1); pt(p, 0, 0); pt(p, .34f, .72f); pt(p, .68f, 0); pt(p, .68f, 1); return .68f;
    case ' ': return .28f;
    default: return 0;
    }
}

#define GAP .2f

float sf_text_width(const char *text, float height) {
    path p;
    float w = 0;
    int first = 1;
    for (const char *s = text; *s; s++) {
        float adv = glyph(*s, &p);
        if (adv <= 0) continue;
        w += (first ? 0 : GAP) + adv;
        first = 0;
    }
    return w * height;
}

typedef struct {
    float *cover;
    int x0, y0, w;
} mask;

static void plot_max(void *ctx, int x, int y, float cover) {
    mask *m = ctx;
    float *v = &m->cover[(size_t)(y - m->y0) * m->w + (x - m->x0)];
    if (cover > *v) *v = cover;
}

void sf_canvas_text(sf_canvas *c, const char *text, float x, float bottom, float height, sf_color color) {
    float s = c->scale;
    float stroke_w = height * 0.13f;
    /* Inset by half the stroke so the outer edges land on the box. */
    float h = height - stroke_w;
    float top = bottom - height + stroke_w / 2;

    /*
     * Strokes overlap where they join, so gather the text's coverage first
     * (the most any stroke gives each pixel) and paint it once.
     */
    int mx0 = (int)floorf(x * s) - 2, my0 = (int)floorf((bottom - height) * s) - 2;
    int mx1 = (int)ceilf((x + sf_text_width(text, height)) * s) + 2, my1 = (int)ceilf(bottom * s) + 2;
    if (mx0 < 0) mx0 = 0;
    if (my0 < 0) my0 = 0;
    if (mx1 > c->width) mx1 = c->width;
    if (my1 > c->height) my1 = c->height;
    if (mx1 <= mx0 || my1 <= my0) return;
    mask m = {calloc((size_t)(mx1 - mx0) * (size_t)(my1 - my0), sizeof(float)), mx0, my0, mx1 - mx0};
    if (!m.cover) return;

    float pen = x + stroke_w / 2;
    path p;
    int first = 1;
    for (const char *ch = text; *ch; ch++) {
        float adv = glyph(*ch, &p);
        if (adv <= 0) continue;
        if (!first) pen += GAP * height;
        first = 0;
        for (int i = 0; i + 1 < p.n; i++) {
            if (p.x[i] < -1 || p.x[i + 1] < -1) continue;
            float x0 = (pen + p.x[i] * h) * s - mx0, y0 = (top + p.y[i] * h) * s - my0;
            float x1 = (pen + p.x[i + 1] * h) * s - mx0, y1 = (top + p.y[i + 1] * h) * s - my0;
            mask local = {m.cover, 0, 0, m.w};
            capsule(m.w, my1 - my0, x0, y0, x1, y1, stroke_w * s / 2, plot_max, &local);
        }
        pen += adv * height;
    }

    paint ink = {c, pack(color), color.a};
    for (int y = my0; y < my1; y++)
        for (int xx = mx0; xx < mx1; xx++) {
            float v = m.cover[(size_t)(y - my0) * m.w + (xx - mx0)];
            if (v > 0) plot_blend(&ink, xx, y, v);
        }
    free(m.cover);
}

void sf_format_time(char *out, int size, int use_24h) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (!t || size < 9) {
        if (size > 0) out[0] = 0;
        return;
    }
    if (use_24h) {
        strftime(out, (size_t)size, "%H:%M", t);
    } else {
        int h = t->tm_hour % 12;
        if (h == 0) h = 12;
        /* Avoid snprintf's locale and %I's leading zero. */
        int i = 0;
        if (h >= 10) out[i++] = '1';
        out[i++] = (char)('0' + h % 10);
        out[i++] = ':';
        out[i++] = (char)('0' + t->tm_min / 10);
        out[i++] = (char)('0' + t->tm_min % 10);
        out[i++] = ' ';
        out[i++] = t->tm_hour < 12 ? 'A' : 'P';
        out[i++] = 'M';
        out[i] = 0;
    }
}
