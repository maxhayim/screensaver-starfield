/* Core tests: cc -std=c99 -Icore core/starfield.c core/canvas.c tests/test_starfield.c -lm && ./a.out */
#include "canvas.h"
#include "starfield.h"

#include <stdio.h>
#include <stdlib.h>

static int failures = 0;
#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                \
        }                                                              \
    } while (0)

static int fills, lines, circles;
static sf_color last_fill;
static void count_fill(void *c, sf_color k) {
    (void)c;
    fills++;
    last_fill = k;
}
static void count_line(void *c, float a, float b, float d, float e, float w, sf_color k) {
    (void)c, (void)a, (void)b, (void)d, (void)e, (void)w, (void)k;
    lines++;
}
static void count_circle(void *c, float x, float y, float r, sf_color k) {
    (void)c, (void)x, (void)y, (void)r, (void)k;
    circles++;
}

int main(void) {
    sf_color c;
    CHECK(sf_parse_color("#f06a2a", &c) && c.r > 0.93f && c.a == 1.0f);
    CHECK(sf_parse_color("eeebe480", &c) && c.a > 0.49f && c.a < 0.51f);
    CHECK(!sf_parse_color("#12345", &c));
    CHECK(!sf_parse_color("#zzzzzz", &c));

    /* Star count matches the site: area / 4200, kept within 220..520. */
    starfield *s = sf_create(42);
    sf_resize(s, 1440, 900);
    CHECK(sf_star_count(s) == 309);
    sf_resize(s, 640, 360);
    CHECK(sf_star_count(s) == 220);
    sf_resize(s, 5120, 2880);
    CHECK(sf_star_count(s) == 520);
    CHECK(sf_take_needs_clear(s) && !sf_take_needs_clear(s));

    sf_options o = sf_default_options();
    o.compact = 1;
    sf_set_options(s, &o);
    CHECK(sf_star_count(s) == 110);

    /* About 7% accent stars; none or all when the share is 0 or 1. */
    o.compact = 0;
    sf_set_options(s, &o);
    int accents = sf_accent_count(s);
    CHECK(accents > 15 && accents < 60);
    o.accent_share = 0;
    sf_set_options(s, &o);
    CHECK(sf_accent_count(s) == 0);
    o.accent_share = 1;
    sf_set_options(s, &o);
    CHECK(sf_accent_count(s) == sf_star_count(s));

    /* First frame draws dots (stars that start off-screen respawn far away, as on the site), then streaks once stars have moved. */
    sf_renderer r = {NULL, count_fill, count_line, count_circle};
    o = sf_default_options();
    sf_set_options(s, &o);
    sf_step(s, 1.0f / 60);
    sf_render(s, &r);
    CHECK(fills == 1 && circles > 50 && lines == 0);
    CHECK(last_fill.a > 0.41f && last_fill.a < 0.43f);
    for (int i = 0; i < 600; i++) sf_step(s, 1.0f / 60);
    lines = circles = 0;
    sf_render(s, &r);
    CHECK(lines > circles && lines > 300);

    /* The fade keeps the site's rate at other frame rates: one 30 fps frame = two 60 fps frames. */
    sf_step(s, 1.0f / 30);
    sf_render(s, &r);
    CHECK(last_fill.a > 0.65f && last_fill.a < 0.67f);

    /* A new background asks the host to clear. */
    sf_parse_color("#000000", &o.background);
    sf_set_options(s, &o);
    CHECK(sf_take_needs_clear(s));

    sf_destroy(s);

    /* Software canvas: fades reach the background exactly, so trails never leave a haze. */
    sf_canvas cv;
    CHECK(sf_canvas_init(&cv, 64, 32, 1));
    sf_color white, bg;
    sf_parse_color("#ffffff", &white);
    sf_parse_color("#0b0b0a", &bg);
    sf_canvas_clear(&cv, white);
    sf_renderer cr = sf_canvas_renderer(&cv);
    sf_color fade = bg;
    fade.a = 0.42f;
    for (int i = 0; i < 40; i++) cr.fill(cr.ctx, fade);
    CHECK(cv.pixels[0] == 0x0b0b0a && cv.pixels[64 * 32 - 1] == 0x0b0b0a);

    /* Lines cover their middle and leave far pixels alone; hairlines still show. */
    cr.line(cr.ctx, 4, 16, 60, 16, 3, white);
    CHECK(cv.pixels[16 * 64 + 32] == 0xffffff);
    CHECK(cv.pixels[2 * 64 + 32] == 0x0b0b0a);
    sf_canvas_clear(&cv, bg);
    cr.line(cr.ctx, 4, 8, 60, 8, 0.4f, white);
    CHECK((cv.pixels[8 * 64 + 32] & 0xff) > 0x0b);

    /* Clock text: width grows with the string, and drawing off the edge is safe. */
    CHECK(sf_text_width("12:00 PM", 34) > sf_text_width("1:00", 34));
    sf_canvas_text(&cv, "88:88 AM", -20, 40, 50, white);
    char clock[16];
    sf_format_time(clock, sizeof clock, 1);
    CHECK(clock[2] == ':' && clock[5] == 0);
    sf_format_time(clock, sizeof clock, 0);
    CHECK(clock[0] != '0' && (clock[1] == ':' || clock[2] == ':'));
    sf_canvas_free(&cv);

    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("all starfield core tests passed\n");
    return 0;
}
