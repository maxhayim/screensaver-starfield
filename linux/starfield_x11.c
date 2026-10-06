/*
 * Starfield for Linux: an XScreenSaver "hack". XScreenSaver runs it with
 * -root and names the window to draw in through $XSCREENSAVER_WINDOW (the
 * full screen, or the small preview in its settings). -window opens a
 * window of its own, for trying it out.
 *
 * The stars are drawn by the shared core into a software canvas and copied
 * to the window, through X shared memory when the server is local.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include <X11/extensions/XShm.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "canvas.h"
#include "presets.h"
#include "starfield.h"

#ifndef VERSION_STR
#define VERSION_STR "0.0.0"
#endif

typedef struct {
    const char *background, *stars, *accent;
    int accent_percent, speed_percent, trails_percent;
    int show_clock, use_24h, reduced_motion;
    int label_mode;         /* SF_LABEL_* */
    const char *label_text; /* for SF_LABEL_CUSTOM */
    float scale; /* 0 = from Xft.dpi */
    int root, own_window;
    Window window_id;
} options;

static void usage(void) {
    fprintf(stderr,
            "screensaver-starfield %s: a 90s flight through space, for XScreenSaver.\n"
            "  -root | -window | -window-id <id>   where to draw\n"
            "  -preset <name>                      original, classic, deep-space, green-terminal,\n"
            "                                      amber-terminal, synthwave, paper\n"
            "  -background <#rrggbb>  -stars <#rrggbb>  -accent <#rrggbb>\n"
            "  -accent-percent <0-50>   share of accent stars (default %d)\n"
            "  -speed <25-300>          percent of normal speed (default %d)\n"
            "  -trails <0-95>           how long streaks linger (default %d)\n"
            "  -clock | -no-clock       show the time (default on)\n"
            "  -24h | -12h              clock format (default 12h)\n"
            "  -label-name | -label-user   your name, or your username, under the clock\n"
            "  -label <text>            your own text under the clock\n"
            "  -reduce-motion           slower flight\n"
            "  -scale <n>               HiDPI scale (default from Xft.dpi)\n",
            VERSION_STR, SF_DEFAULT_ACCENT_PERCENT, SF_DEFAULT_SPEED_PERCENT, SF_DEFAULT_TRAILS_PERCENT);
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

/* Preset names match ignoring case, with '-' or '_' for spaces ("deep-space"). */
static int same_name(const char *a, const char *b) {
    for (;; a++, b++) {
        char x = *a == '-' || *a == '_' ? ' ' : *a, y = *b == '-' || *b == '_' ? ' ' : *b;
        if (x >= 'A' && x <= 'Z') x = (char)(x + 32);
        if (y >= 'A' && y <= 'Z') y = (char)(y + 32);
        if (x != y) return 0;
        if (!x) return 1;
    }
}

/* XScreenSaver passes a color only when it was set; "" or "default" means the preset's. */
static int given(const char *s) { return s && *s && strcasecmp(s, "default") != 0; }

static int parse_args(int argc, char **argv, options *o) {
    const sf_preset *preset = &SF_PRESETS[0];
    const char *bg = NULL, *st = NULL, *ac = NULL;
    memset(o, 0, sizeof *o);
    o->accent_percent = SF_DEFAULT_ACCENT_PERCENT;
    o->speed_percent = SF_DEFAULT_SPEED_PERCENT;
    o->trails_percent = SF_DEFAULT_TRAILS_PERCENT;
    o->show_clock = 1;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (a[0] == '-' && a[1] == '-') a++; /* accept --option too */
        const char *val = i + 1 < argc ? argv[i + 1] : NULL;
#define TAKES(name) (!strcmp(a, name) && val && (i++, 1))
        if (!strcmp(a, "-root")) o->root = 1;
        else if (!strcmp(a, "-window")) o->own_window = 1;
        else if (TAKES("-window-id")) o->window_id = (Window)strtoul(val, NULL, 0);
        else if (TAKES("-preset")) {
            int found = 0;
            for (int p = 0; p < SF_PRESET_COUNT; p++)
                if (same_name(val, SF_PRESETS[p].name)) preset = &SF_PRESETS[p], found = 1;
            if (!found) fprintf(stderr, "screensaver-starfield: unknown preset \"%s\"\n", val);
        }
        else if (TAKES("-background")) bg = val;
        else if (TAKES("-stars")) st = val;
        else if (TAKES("-accent")) ac = val;
        else if (TAKES("-accent-percent")) o->accent_percent = atoi(val);
        else if (TAKES("-speed")) o->speed_percent = atoi(val);
        else if (TAKES("-trails")) o->trails_percent = atoi(val);
        else if (TAKES("-scale")) o->scale = (float)atof(val);
        else if (!strcmp(a, "-clock")) o->show_clock = 1;
        else if (!strcmp(a, "-no-clock")) o->show_clock = 0;
        else if (!strcmp(a, "-24h")) o->use_24h = 1;
        else if (!strcmp(a, "-12h")) o->use_24h = 0;
        else if (!strcmp(a, "-reduce-motion")) o->reduced_motion = 1;
        else if (!strcmp(a, "-label-name")) o->label_mode = SF_LABEL_NAME;
        else if (!strcmp(a, "-label-user")) o->label_mode = SF_LABEL_USERNAME;
        else if (!strcmp(a, "-label-none")) o->label_mode = SF_LABEL_NONE, o->label_text = NULL;
        else if (TAKES("-label")) {
            o->label_text = val;
            if (o->label_mode == SF_LABEL_NONE) o->label_mode = SF_LABEL_CUSTOM;
        }
        else if (!strcmp(a, "-label-custom")) o->label_mode = SF_LABEL_CUSTOM;
        else if (!strcmp(a, "-fps") || !strcmp(a, "-no-fps")) {} /* XScreenSaver's standard flags */
        else if (!strcmp(a, "-h") || !strcmp(a, "-help") || !strcmp(a, "-version")) {
            usage();
            exit(0);
        } else {
            fprintf(stderr, "screensaver-starfield: unknown option %s\n", argv[i]);
            usage();
            return 0;
        }
#undef TAKES
    }
    sf_color c;
    o->background = given(bg) && sf_parse_color(bg, &c) ? bg : preset->background;
    o->stars = given(st) && sf_parse_color(st, &c) ? st : preset->stars;
    o->accent = given(ac) && sf_parse_color(ac, &c) ? ac : preset->accent;
    o->accent_percent = clampi(o->accent_percent, 0, 50);
    o->speed_percent = clampi(o->speed_percent, 25, 300);
    o->trails_percent = clampi(o->trails_percent, 0, 95);
    return 1;
}

/* ---------- drawing surface ---------- */

typedef struct {
    Display *dpy;
    Window win;
    Visual *visual;
    int depth;
    GC gc;
    XImage *image;
    XShmSegmentInfo shm;
    int use_shm, fast; /* fast: the image's pixels are 0x00RRGGBB */
    sf_canvas trail, frame;
    uint32_t *convert; /* frame pixels, when the image needs another format */
    int width, height;
} surface;

static int shm_failed;
static int (*old_handler)(Display *, XErrorEvent *);
static int shm_error_handler(Display *d, XErrorEvent *e) {
    (void)d, (void)e;
    shm_failed = 1;
    return 0;
}

static void free_image(surface *s) {
    if (!s->image) return;
    if (s->use_shm) {
        XShmDetach(s->dpy, &s->shm);
        XSync(s->dpy, False);
        s->image->data = NULL;
        XDestroyImage(s->image);
        shmdt(s->shm.shmaddr);
    } else {
        if (s->fast) s->image->data = NULL; /* the canvas owns it */
        XDestroyImage(s->image);
    }
    s->image = NULL;
    free(s->convert);
    s->convert = NULL;
    sf_canvas_free(&s->trail);
    if (!s->use_shm && s->fast) sf_canvas_free(&s->frame);
    memset(&s->frame, 0, sizeof s->frame);
}

static int make_image(surface *s, int w, int h, float scale) {
    free_image(s);
    s->width = w;
    s->height = h;
    s->fast = (s->depth == 24 || s->depth == 32) && s->visual->red_mask == 0xff0000 &&
              s->visual->green_mask == 0xff00 && s->visual->blue_mask == 0xff;
    if (!sf_canvas_init(&s->trail, w, h, scale)) return 0;

    s->use_shm = s->fast && XShmQueryExtension(s->dpy) && !getenv("STARFIELD_NO_SHM");
    if (s->use_shm) {
        s->image = XShmCreateImage(s->dpy, s->visual, (unsigned)s->depth, ZPixmap, NULL, &s->shm, (unsigned)w, (unsigned)h);
        if (s->image && s->image->bits_per_pixel == 32) {
            s->shm.shmid = shmget(IPC_PRIVATE, (size_t)s->image->bytes_per_line * (size_t)h, IPC_CREAT | 0600);
            if (s->shm.shmid >= 0) {
                s->shm.shmaddr = s->image->data = shmat(s->shm.shmid, NULL, 0);
                s->shm.readOnly = False;
                shm_failed = 0;
                old_handler = XSetErrorHandler(shm_error_handler);
                XShmAttach(s->dpy, &s->shm);
                XSync(s->dpy, False);
                XSetErrorHandler(old_handler);
                shmctl(s->shm.shmid, IPC_RMID, NULL); /* freed once both sides detach */
                if (!shm_failed && s->shm.shmaddr != (char *)-1) {
                    sf_canvas_wrap(&s->frame, (uint32_t *)(void *)s->image->data, w, h, s->image->bytes_per_line / 4, scale);
                    return 1;
                }
                if (s->shm.shmaddr != (char *)-1) shmdt(s->shm.shmaddr);
            }
        }
        if (s->image) {
            s->image->data = NULL;
            XDestroyImage(s->image);
            s->image = NULL;
        }
        s->use_shm = 0;
    }

    if (!sf_canvas_init(&s->frame, w, h, scale)) return 0;
    if (s->fast) {
        s->image = XCreateImage(s->dpy, s->visual, (unsigned)s->depth, ZPixmap, 0, (char *)s->frame.pixels, (unsigned)w,
                                (unsigned)h, 32, w * 4);
    } else {
        /* Unusual visual: let Xlib convert pixel by pixel. */
        s->image = XCreateImage(s->dpy, s->visual, (unsigned)s->depth, ZPixmap, 0, NULL, (unsigned)w, (unsigned)h, 32, 0);
        if (s->image) s->image->data = malloc((size_t)s->image->bytes_per_line * (size_t)h);
        s->convert = s->frame.pixels;
        s->frame.pixels = NULL;
        sf_canvas_wrap(&s->frame, s->convert, w, h, w, scale);
    }
    return s->image != NULL;
}

static void put_image(surface *s) {
    if (!s->image) return;
    if (s->use_shm) {
        XShmPutImage(s->dpy, s->win, s->gc, s->image, 0, 0, 0, 0, (unsigned)s->width, (unsigned)s->height, False);
    } else {
        if (!s->fast) {
            Visual *v = s->visual;
            for (int y = 0; y < s->height; y++)
                for (int x = 0; x < s->width; x++) {
                    uint32_t p = s->convert[(size_t)y * s->width + x];
                    unsigned long r = p >> 16 & 0xff, g = p >> 8 & 0xff, b = p & 0xff;
                    unsigned long px = 0;
                    unsigned long masks[3] = {v->red_mask, v->green_mask, v->blue_mask}, vals[3] = {r, g, b};
                    for (int k = 0; k < 3; k++) {
                        unsigned long m = masks[k];
                        if (!m) continue;
                        int shift = 0, bits = 0;
                        while (!(m >> shift & 1)) shift++;
                        while (m >> (shift + bits) & 1) bits++;
                        px |= (vals[k] >> (8 - (bits > 8 ? 8 : bits))) << shift & m;
                    }
                    XPutPixel(s->image, x, y, px);
                }
        }
        XPutImage(s->dpy, s->win, s->gc, s->image, 0, 0, 0, 0, (unsigned)s->width, (unsigned)s->height);
    }
    XSync(s->dpy, False);
}

/* ---------- label ---------- */

/* The text under the clock (UTF-8), or "" for none. */
static void label_text(const options *o, char *out, size_t size) {
    out[0] = 0;
    const char *src = NULL;
    struct passwd *pw = getpwuid(getuid());
    char name[256];
    if (o->label_mode == SF_LABEL_CUSTOM) {
        src = o->label_text;
    } else if (o->label_mode == SF_LABEL_USERNAME) {
        src = pw ? pw->pw_name : getenv("USER");
    } else if (o->label_mode == SF_LABEL_NAME) {
        /* The full name is the first comma-separated field of the GECOS entry. */
        name[0] = 0;
        if (pw && pw->pw_gecos) {
            size_t n = strcspn(pw->pw_gecos, ",");
            if (n >= sizeof name) n = sizeof name - 1;
            memcpy(name, pw->pw_gecos, n);
            name[n] = 0;
        }
        src = name[0] ? name : (pw ? pw->pw_name : getenv("USER"));
    }
    if (!src) return;
    while (*src == ' ' || *src == '\t') src++;
    /* Copy up to SF_LABEL_MAX characters on one line, without cutting a UTF-8 sequence. */
    size_t i = 0;
    int chars = 0;
    for (const unsigned char *p = (const unsigned char *)src; *p && chars < SF_LABEL_MAX;) {
        size_t n = *p < 0x80 ? 1 : (*p >> 5) == 6 ? 2 : (*p >> 4) == 14 ? 3 : (*p >> 3) == 30 ? 4 : 1;
        if (i + n >= size) break;
        for (size_t k = 0; k < n && p[k]; k++) out[i++] = (char)(p[k] == '\n' || p[k] == '\r' || p[k] == '\t' ? ' ' : p[k]);
        p += n;
        chars++;
    }
    while (i && out[i - 1] == ' ') i--;
    out[i] = 0;
}

/* ---- right-to-left text: Xft draws characters in the order given, so put Hebrew and Arabic in display order. */

static int is_rtl(unsigned c) {
    return (c >= 0x0590 && c <= 0x08ff) || (c >= 0xfb1d && c <= 0xfdff) || (c >= 0xfe70 && c <= 0xfeff);
}

static int is_strong_ltr(unsigned c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= 0xc0 && c < 0x590 && c != 0xd7 && c != 0xf7);
}

static int utf8_decode(const char *s, unsigned *out, int max) {
    int n = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p && n < max) {
        unsigned c = *p;
        int len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 1;
        if (len > 1) {
            c &= 0xff >> (len + 1);
            for (int k = 1; k < len && p[k]; k++) c = c << 6 | (p[k] & 0x3f);
        }
        out[n++] = c;
        p += len;
    }
    return n;
}

static size_t utf8_encode(const unsigned *cps, int n, char *out, size_t size) {
    size_t i = 0;
    for (int k = 0; k < n; k++) {
        unsigned c = cps[k];
        char b[4];
        int len;
        if (c < 0x80) b[0] = (char)c, len = 1;
        else if (c < 0x800) b[0] = (char)(0xc0 | c >> 6), b[1] = (char)(0x80 | (c & 0x3f)), len = 2;
        else if (c < 0x10000) b[0] = (char)(0xe0 | c >> 12), b[1] = (char)(0x80 | (c >> 6 & 0x3f)), b[2] = (char)(0x80 | (c & 0x3f)), len = 3;
        else b[0] = (char)(0xf0 | c >> 18), b[1] = (char)(0x80 | (c >> 12 & 0x3f)), b[2] = (char)(0x80 | (c >> 6 & 0x3f)), b[3] = (char)(0x80 | (c & 0x3f)), len = 4;
        if (i + (size_t)len >= size) break;
        memcpy(out + i, b, (size_t)len);
        i += (size_t)len;
    }
    out[i] = 0;
    return i;
}

static void reverse(unsigned *a, int from, int to) {
    for (to--; from < to; from++, to--) {
        unsigned t = a[from];
        a[from] = a[to];
        a[to] = t;
    }
}

/*
 * A small subset of the Unicode bidi algorithm, enough for names and short
 * labels: runs of right-to-left letters (with the spaces and marks between
 * them) are reversed, and if the text starts right-to-left, so is the order
 * of the runs. Arabic letters aren't joined, since Xft doesn't shape.
 */
static void to_display_order(char *text, size_t size) {
    unsigned cps[SF_LABEL_MAX * 2];
    int n = utf8_decode(text, cps, (int)(sizeof cps / sizeof cps[0]));
    int any = 0, base_rtl = -1;
    for (int i = 0; i < n; i++) {
        if (is_rtl(cps[i])) any = 1;
        if (base_rtl < 0 && (is_rtl(cps[i]) || is_strong_ltr(cps[i]))) base_rtl = is_rtl(cps[i]);
    }
    if (!any) return;
    if (base_rtl == 1) {
        /* Reverse everything, then turn left-to-right runs (letters and digits) back around. */
        reverse(cps, 0, n);
        for (int i = 0; i < n;) {
            if (!is_rtl(cps[i]) && cps[i] != ' ' && (is_strong_ltr(cps[i]) || (cps[i] >= '0' && cps[i] <= '9'))) {
                int j = i;
                while (j < n && !is_rtl(cps[j])) j++;
                int end = j;
                while (end > i && (cps[end - 1] == ' ' || !(is_strong_ltr(cps[end - 1]) || (cps[end - 1] >= '0' && cps[end - 1] <= '9')))) end--;
                reverse(cps, i, end);
                i = j;
            } else {
                i++;
            }
        }
    } else {
        /* Left-to-right text: reverse each right-to-left run in place. */
        for (int i = 0; i < n;) {
            if (is_rtl(cps[i])) {
                int j = i;
                while (j < n && (is_rtl(cps[j]) || (cps[j] == ' ' && j + 1 < n && is_rtl(cps[j + 1])))) j++;
                reverse(cps, i, j);
                i = j;
            } else {
                i++;
            }
        }
    }
    utf8_encode(cps, n, text, size);
}

typedef struct {
    unsigned char *coverage;
    int w, h;
} text_mask;

/* Renders UTF-8 `text` in the desktop's sans-serif font, `px` pixels tall, as a coverage mask. */
static text_mask render_text(Display *dpy, Window win, const XWindowAttributes *wa, const char *text, double px) {
    text_mask m = {NULL, 0, 0};
    if (!text[0] || px < 4) return m;
    XftFont *font = XftFontOpen(dpy, DefaultScreen(dpy), XFT_FAMILY, XftTypeString, "sans-serif", XFT_PIXEL_SIZE,
                                XftTypeDouble, px, XFT_ANTIALIAS, XftTypeBool, True, NULL);
    if (!font) return m;
    XGlyphInfo ext;
    XftTextExtentsUtf8(dpy, font, (const FcChar8 *)text, (int)strlen(text), &ext);
    int w = ext.xOff + 2, h = font->ascent + font->descent;
    if (w > 2 && h > 0 && w < 8192) {
        Pixmap pm = XCreatePixmap(dpy, win, (unsigned)w, (unsigned)h, (unsigned)wa->depth);
        GC gc = XCreateGC(dpy, pm, 0, NULL);
        XSetForeground(dpy, gc, 0);
        XFillRectangle(dpy, pm, gc, 0, 0, (unsigned)w, (unsigned)h);
        XftDraw *draw = XftDrawCreate(dpy, pm, wa->visual, wa->colormap);
        XRenderColor white = {0xffff, 0xffff, 0xffff, 0xffff};
        XftColor ink;
        if (draw && XftColorAllocValue(dpy, wa->visual, wa->colormap, &white, &ink)) {
            XftDrawStringUtf8(draw, &ink, font, 1, font->ascent, (const FcChar8 *)text, (int)strlen(text));
            XImage *img = XGetImage(dpy, pm, 0, 0, (unsigned)w, (unsigned)h, AllPlanes, ZPixmap);
            unsigned long gm = wa->visual->green_mask;
            int shift = 0;
            while (gm && !(gm >> shift & 1)) shift++;
            unsigned long top = gm >> shift;
            m.coverage = img ? malloc((size_t)w * (size_t)h) : NULL;
            if (m.coverage) {
                for (int y = 0; y < h; y++)
                    for (int x = 0; x < w; x++) {
                        unsigned long v = (XGetPixel(img, x, y) & gm) >> shift;
                        m.coverage[(size_t)y * w + x] = (unsigned char)(top ? v * 255 / top : 0);
                    }
                m.w = w;
                m.h = h;
            }
            if (img) XDestroyImage(img);
            XftColorFree(dpy, wa->visual, wa->colormap, &ink);
        }
        if (draw) XftDrawDestroy(draw);
        XFreeGC(dpy, gc);
        XFreePixmap(dpy, pm);
    }
    XftFontClose(dpy, font);
    return m;
}

/* ---------- main ---------- */

static float xft_scale(Display *dpy) {
    char *db_str = XResourceManagerString(dpy);
    float scale = 1;
    if (!db_str) return scale;
    XrmInitialize();
    XrmDatabase db = XrmGetStringDatabase(db_str);
    char *type = NULL;
    XrmValue value;
    if (db && XrmGetResource(db, "Xft.dpi", "Xft.Dpi", &type, &value) && value.addr) {
        float dpi = (float)atof(value.addr);
        if (dpi >= 96) scale = dpi / 96.0f;
    }
    if (db) XrmDestroyDatabase(db);
    return scale;
}

static double now_s(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

static void sleep_s(double s) {
    if (s <= 0) return;
    struct timespec t = {(time_t)s, (long)((s - (double)(time_t)s) * 1e9)};
    while (nanosleep(&t, &t) == -1 && errno == EINTR) {}
}

static sf_color color_of(const char *hex) {
    sf_color c;
    sf_parse_color(hex, &c);
    return c;
}

int main(int argc, char **argv) {
    options opt;
    if (!parse_args(argc, argv, &opt)) return 1;

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "screensaver-starfield: can't open display\n");
        return 1;
    }
    int screen = DefaultScreen(dpy);

    /* Which window: $XSCREENSAVER_WINDOW, -window-id, -root, or our own. */
    Window win = opt.window_id;
    const char *env = getenv("XSCREENSAVER_WINDOW");
    if (!win && env && *env) win = (Window)strtoul(env, NULL, 0);
    if (!win && opt.root && !opt.own_window) win = RootWindow(dpy, screen);
    Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    int own = 0;
    if (!win) {
        own = 1;
        win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), 0, 0, 1280, 720, 0, 0, BlackPixel(dpy, screen));
        XStoreName(dpy, win, "Starfield");
        XSetWMProtocols(dpy, win, &wm_delete, 1);
        XSelectInput(dpy, win, StructureNotifyMask | ExposureMask | KeyPressMask);
        XMapWindow(dpy, win);
    } else {
        XSelectInput(dpy, win, StructureNotifyMask | ExposureMask);
    }

    XWindowAttributes wa;
    if (!XGetWindowAttributes(dpy, win, &wa)) {
        fprintf(stderr, "screensaver-starfield: no such window 0x%lx\n", (unsigned long)win);
        return 1;
    }

    surface s;
    memset(&s, 0, sizeof s);
    s.dpy = dpy;
    s.win = win;
    s.visual = wa.visual;
    s.depth = wa.depth;
    s.gc = XCreateGC(dpy, win, 0, NULL);

    float scale = opt.scale > 0 ? opt.scale : xft_scale(dpy);
    starfield *sf = sf_create((unsigned)time(NULL) ^ (unsigned)win);
    sf_color background = color_of(opt.background);

    char label[SF_LABEL_MAX * 4 + 1];
    label_text(&opt, label, sizeof label);
    to_display_order(label, sizeof label);
    text_mask label_mask = {NULL, 0, 0};

    int width = 0, height = 0, compact = 0;
    double last = now_s();
    int frame = 0;
    for (;;) {
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            if (ev.type == ConfigureNotify && ev.xconfigure.window == win) {
                wa.width = ev.xconfigure.width;
                wa.height = ev.xconfigure.height;
            } else if (ev.type == DestroyNotify && ev.xdestroywindow.window == win) {
                return 0;
            } else if (own && ev.type == KeyPress) {
                return 0;
            } else if (own && ev.type == ClientMessage && (Atom)ev.xclient.data.l[0] == wm_delete) {
                return 0;
            }
        }
        /* The root window doesn't always report resizes. */
        if (++frame % 60 == 0 && !own && !XGetWindowAttributes(dpy, win, &wa)) return 0;

        if (wa.width != width || wa.height != height) {
            width = wa.width;
            height = wa.height;
            /* XScreenSaver's preview is small; draw the thumbnail version there. */
            compact = width < 600 && height < 600;
            float k = compact ? 1 : scale;
            if (!make_image(&s, width, height, k)) {
                fprintf(stderr, "screensaver-starfield: out of memory\n");
                return 1;
            }
            sf_options o = sf_default_options();
            o.background = background;
            o.star = color_of(opt.stars);
            o.accent = color_of(opt.accent);
            o.accent_share = (float)opt.accent_percent / 100.0f;
            o.speed = (float)opt.speed_percent / 100.0f;
            o.trails = (float)opt.trails_percent / 100.0f;
            o.reduced_motion = opt.reduced_motion;
            o.compact = compact;
            sf_set_options(sf, &o);
            sf_resize(sf, (float)width / k, (float)height / k);
            sf_canvas_clear(&s.trail, background);
            (void)sf_take_needs_clear(sf);

            free(label_mask.coverage);
            float ck = compact ? ((float)height / 900.0f > 0.25f ? (float)height / 900.0f : 0.25f) : 1;
            label_mask = render_text(dpy, win, &wa, label, 14.0 * ck * k);
        }

        double t = now_s();
        float dt = (float)(t - last);
        last = t;
        sf_step(sf, dt);
        if (sf_take_needs_clear(sf)) sf_canvas_clear(&s.trail, background);
        sf_renderer r = sf_canvas_renderer(&s.trail);
        sf_render(sf, &r);
        sf_canvas_copy(&s.frame, &s.trail);
        /* Bottom-left: the time in large type, with the optional label under it. */
        float fh = (float)height / s.frame.scale;
        float ck = compact ? (fh / 900.0f > 0.25f ? fh / 900.0f : 0.25f) : 1;
        float bottom = fh - 32 * ck;
        sf_color ink = color_of(opt.stars);
        if (label_mask.coverage) {
            ink.a = 0.5f;
            int top = (int)(bottom * s.frame.scale + 0.5f) - label_mask.h;
            sf_canvas_mask(&s.frame, label_mask.coverage, label_mask.w, label_mask.h, (int)(32 * ck * s.frame.scale + 0.5f), top, ink);
            bottom = (float)top / s.frame.scale - 6 * ck;
        }
        if (opt.show_clock) {
            char text[16];
            sf_format_time(text, sizeof text, opt.use_24h);
            ink.a = 0.8f;
            sf_canvas_text(&s.frame, text, 32 * ck, bottom, 34 * ck, ink);
        }
        put_image(&s);

        sleep_s(1.0 / 60.0 - (now_s() - t));
    }
}
