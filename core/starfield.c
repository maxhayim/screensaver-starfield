/*
 * Starfield — drawing core, ported from StarfieldCanvas on maxhayim.com:
 * a 90s flight through space with the odd orange star.
 */
#include "starfield.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MAX_STARS 520
#define EDGE 10.0f

typedef struct {
    float x, y, z;  /* x, y in -1.2..1.2; z from 1 (far) to 0 (at the screen) */
    float px, py;   /* last frame's screen spot */
    float sx, sy;   /* this frame's screen spot */
    int has_prev;
    int visible;    /* drawn this frame (false right after a respawn) */
    int accent;
} star;

struct starfield {
    sf_options opts;
    float width, height;
    unsigned rng;
    float last_dt;
    int needs_clear;
    star stars[MAX_STARS];
    int count;
};

/* ---------- helpers ---------- */

static float rnd(starfield *s) {
    /* xorshift32 */
    unsigned x = s->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s->rng = x ? x : 0x9e3779b9u;
    return (float)(x & 0xffffff) / (float)0x1000000;
}

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static void spawn(starfield *s, star *st, int far) {
    st->x = (rnd(s) * 2 - 1) * 1.2f;
    st->y = (rnd(s) * 2 - 1) * 1.2f;
    st->z = far ? 1.0f : 0.2f + rnd(s) * 0.8f;
    st->has_prev = 0;
    st->visible = 0;
    st->accent = rnd(s) < s->opts.accent_share;
}

static int target_count(const starfield *s) {
    if (s->opts.compact) return 110;
    int n = (int)lroundf(s->width * s->height / 4200.0f);
    return n < 220 ? 220 : (n > MAX_STARS ? MAX_STARS : n);
}

static void populate(starfield *s) {
    s->count = target_count(s);
    for (int i = 0; i < s->count; i++) spawn(s, &s->stars[i], 0);
    s->needs_clear = 1;
}

static sf_color with_alpha(sf_color c, float a) {
    c.a *= clampf(a, 0, 1);
    return c;
}

/* ---------- API ---------- */

sf_options sf_default_options(void) {
    sf_options o;
    sf_parse_color("#0b0b0a", &o.background);
    sf_parse_color("#eeebe4", &o.star);
    sf_parse_color("#f06a2a", &o.accent);
    o.accent_share = 0.07f;
    o.speed = 1.0f;
    o.trails = 0.58f;
    o.reduced_motion = 0;
    o.compact = 0;
    return o;
}

starfield *sf_create(unsigned seed) {
    starfield *s = calloc(1, sizeof *s);
    if (!s) return NULL;
    s->rng = seed ? seed : 0x2545f491u;
    s->opts = sf_default_options();
    return s;
}

void sf_destroy(starfield *s) { free(s); }

void sf_set_options(starfield *s, const sf_options *opts) {
    int reshape = opts->compact != s->opts.compact;
    int recolor = memcmp(&opts->background, &s->opts.background, sizeof opts->background) != 0;
    int reshare = opts->accent_share != s->opts.accent_share;
    s->opts = *opts;
    s->opts.accent_share = clampf(s->opts.accent_share, 0, 1);
    s->opts.trails = clampf(s->opts.trails, 0, 0.95f);
    if (s->opts.speed <= 0) s->opts.speed = 1.0f;
    if (recolor) s->needs_clear = 1;
    if (s->width <= 0) return;
    if (reshape) {
        populate(s);
    } else if (reshare) {
        for (int i = 0; i < s->count; i++) s->stars[i].accent = rnd(s) < s->opts.accent_share;
    }
}

void sf_resize(starfield *s, float width, float height) {
    if (width <= 0 || height <= 0) return;
    if (width == s->width && height == s->height) return;
    s->width = width;
    s->height = height;
    populate(s);
}

void sf_step(starfield *s, float dt) {
    dt = clampf(dt, 0, 0.05f);
    s->last_dt = dt;
    if (s->width <= 0) return;
    float speed = (s->opts.reduced_motion ? 0.12f : 0.34f) * s->opts.speed * dt;
    float cx = s->width / 2, cy = s->height / 2;
    float scale = (s->width > s->height ? s->width : s->height) * 0.5f;

    for (int i = 0; i < s->count; i++) {
        star *st = &s->stars[i];
        if (st->visible) {
            st->px = st->sx;
            st->py = st->sy;
            st->has_prev = 1;
        }
        st->z -= speed;
        if (st->z <= 0.02f) {
            spawn(s, st, 1);
            continue;
        }
        float sx = cx + (st->x / st->z) * scale;
        float sy = cy + (st->y / st->z) * scale;
        if (sx < -EDGE || sx > s->width + EDGE || sy < -EDGE || sy > s->height + EDGE) {
            spawn(s, st, 1);
            continue;
        }
        st->sx = sx;
        st->sy = sy;
        st->visible = 1;
    }
}

void sf_render(starfield *s, const sf_renderer *r) {
    /* The site fades by 42% per frame at 60 fps; keep that rate at any frame rate. */
    float keep = s->opts.trails;
    float frames = s->last_dt > 0 ? s->last_dt * 60.0f : 1.0f;
    float fade = 1.0f - powf(keep, frames);
    r->fill(r->ctx, with_alpha(s->opts.background, fade));

    int compact = s->opts.compact;
    for (int i = 0; i < s->count; i++) {
        const star *st = &s->stars[i];
        if (!st->visible) continue;
        float nearness = 1 - st->z;
        float size = (compact ? 0.4f : 0.6f) + nearness * (compact ? 1.6f : 2.8f);
        sf_color c = st->accent ? with_alpha(s->opts.accent, 0.4f + nearness * 0.6f)
                                : with_alpha(s->opts.star, 0.25f + nearness * 0.75f);
        if (st->has_prev)
            r->line(r->ctx, st->px, st->py, st->sx, st->sy, size, c);
        else
            r->circle(r->ctx, st->sx, st->sy, size / 2, c);
    }
}

int sf_take_needs_clear(starfield *s) {
    int n = s->needs_clear;
    s->needs_clear = 0;
    return n;
}

int sf_star_count(const starfield *s) { return s->count; }

int sf_accent_count(const starfield *s) {
    int n = 0;
    for (int i = 0; i < s->count; i++) n += s->stars[i].accent;
    return n;
}

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int sf_parse_color(const char *hex, sf_color *out) {
    if (!hex || !out) return 0;
    if (*hex == '#') hex++;
    size_t len = strlen(hex);
    if (len != 6 && len != 8) return 0;
    int v[8];
    for (size_t i = 0; i < len; i++)
        if ((v[i] = hexval(hex[i])) < 0) return 0;
    out->r = (float)(v[0] * 16 + v[1]) / 255.0f;
    out->g = (float)(v[2] * 16 + v[3]) / 255.0f;
    out->b = (float)(v[4] * 16 + v[5]) / 255.0f;
    out->a = len == 8 ? (float)(v[6] * 16 + v[7]) / 255.0f : 1.0f;
    return 1;
}
