/*
 * Renders the Windows/Linux drawing path (core + software canvas) to a BMP,
 * so it can be checked without either OS:
 *   cc -std=c99 -O2 -Icore core/starfield.c core/canvas.c tools/render.c -lm -o build/render
 *   build/render out.bmp [seconds] [width] [height] [scale] [text]
 */
#include <stdio.h>
#include <stdlib.h>

#include "canvas.h"

static void put16(FILE *f, unsigned v) { fputc(v & 0xff, f), fputc(v >> 8 & 0xff, f); }
static void put32(FILE *f, unsigned v) { put16(f, v & 0xffff), put16(f, v >> 16); }

static int write_bmp(const char *path, const sf_canvas *c) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    unsigned size = (unsigned)(c->width * c->height * 4);
    fputs("BM", f);
    put32(f, 54 + size), put32(f, 0), put32(f, 54);
    put32(f, 40), put32(f, (unsigned)c->width), put32(f, (unsigned)-c->height); /* top-down */
    put16(f, 1), put16(f, 32), put32(f, 0), put32(f, size), put32(f, 2835), put32(f, 2835), put32(f, 0), put32(f, 0);
    for (int y = 0; y < c->height; y++) fwrite(c->pixels + (size_t)y * c->stride, 4, (size_t)c->width, f);
    return fclose(f) == 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: render <out.bmp> [seconds] [width] [height] [scale] [text]\n");
        return 1;
    }
    float seconds = argc > 2 ? (float)atof(argv[2]) : 3;
    int width = argc > 3 ? atoi(argv[3]) : 1440;
    int height = argc > 4 ? atoi(argv[4]) : 900;
    float scale = argc > 5 ? (float)atof(argv[5]) : 1;
    const char *text = argc > 6 ? argv[6] : NULL;

    sf_canvas trail, frame;
    if (!sf_canvas_init(&trail, (int)(width * scale), (int)(height * scale), scale) ||
        !sf_canvas_init(&frame, trail.width, trail.height, scale))
        return 2;
    starfield *s = sf_create(7);
    sf_options o = sf_default_options();
    o.compact = width < 500;
    sf_set_options(s, &o);
    sf_resize(s, (float)width, (float)height);
    sf_canvas_clear(&trail, o.background);
    sf_renderer r = sf_canvas_renderer(&trail);
    for (float t = 0; t < seconds; t += 1.0f / 60) {
        sf_step(s, 1.0f / 60);
        sf_render(s, &r);
    }
    sf_canvas_copy(&frame, &trail);
    char clock[16];
    sf_format_time(clock, sizeof clock, 0);
    sf_color ink = o.star;
    ink.a = 0.8f;
    float h = o.compact ? 12 : 34;
    sf_canvas_text(&frame, text ? text : clock, o.compact ? 8 : 32, (float)height - (o.compact ? 8 : 32), text ? 120 : h, ink);
    int ok = write_bmp(argv[1], &frame);
    printf("%s %s (%dx%d)\n", ok ? "wrote" : "failed", argv[1], frame.width, frame.height);
    sf_destroy(s);
    return ok ? 0 : 3;
}
