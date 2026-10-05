/*
 * Starfield for Windows: a .scr is an .exe that Windows starts with
 *   /s            run full screen (every monitor)
 *   /p <HWND>     draw in the small preview in Screen Saver Settings
 *   /c[:<HWND>]   show the Options dialog (also with no arguments)
 * The stars are drawn by the shared core into a software canvas and copied
 * to each window.
 */
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#define SECURITY_WIN32
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <lm.h>
#include <security.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "canvas.h"
#include "presets.h"
#include "resource.h"
#include "starfield.h"

#ifndef VERSION_STR
#define VERSION_STR "0.0.0"
#endif

/* ---------- settings ---------- */

#define REG_KEY L"Software\\maxhayim\\Starfield"

typedef struct {
    char background[8], stars[8], accent[8];
    int accent_percent; /* 0..50 */
    int speed_percent;  /* 25..300 */
    int trails_percent; /* 0..95 */
    int show_clock, use_24h;
    int label_mode;                      /* SF_LABEL_* */
    wchar_t label_text[SF_LABEL_MAX + 1]; /* for SF_LABEL_CUSTOM */
} settings;

static void default_settings(settings *s) {
    strcpy(s->background, SF_PRESETS[0].background);
    strcpy(s->stars, SF_PRESETS[0].stars);
    strcpy(s->accent, SF_PRESETS[0].accent);
    s->accent_percent = SF_DEFAULT_ACCENT_PERCENT;
    s->speed_percent = SF_DEFAULT_SPEED_PERCENT;
    s->trails_percent = SF_DEFAULT_TRAILS_PERCENT;
    s->show_clock = 1;
    s->use_24h = 0;
    s->label_mode = SF_LABEL_NONE;
    s->label_text[0] = 0;
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static void read_color(HKEY key, const char *name, char *out) {
    char buf[16];
    DWORD size = sizeof buf - 1, type = 0;
    sf_color c;
    if (RegQueryValueExA(key, name, NULL, &type, (BYTE *)buf, &size) == ERROR_SUCCESS && type == REG_SZ) {
        buf[size < sizeof buf ? size : sizeof buf - 1] = 0;
        if (sf_parse_color(buf, &c) && strlen(buf) == 7) strcpy(out, buf);
    }
}

static void read_int(HKEY key, const char *name, int *out) {
    DWORD v = 0, size = sizeof v, type = 0;
    if (RegQueryValueExA(key, name, NULL, &type, (BYTE *)&v, &size) == ERROR_SUCCESS && type == REG_DWORD) *out = (int)v;
}

static void load_settings(settings *s) {
    default_settings(s);
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, KEY_READ, &key) != ERROR_SUCCESS) return;
    read_color(key, "Background", s->background);
    read_color(key, "Stars", s->stars);
    read_color(key, "Accent", s->accent);
    read_int(key, "AccentPercent", &s->accent_percent);
    read_int(key, "SpeedPercent", &s->speed_percent);
    read_int(key, "TrailsPercent", &s->trails_percent);
    read_int(key, "ShowClock", &s->show_clock);
    read_int(key, "Use24Hour", &s->use_24h);
    read_int(key, "LabelMode", &s->label_mode);
    DWORD size = sizeof s->label_text - sizeof(wchar_t), type = 0;
    if (RegQueryValueExW(key, L"LabelText", NULL, &type, (BYTE *)s->label_text, &size) == ERROR_SUCCESS && type == REG_SZ)
        s->label_text[size / sizeof(wchar_t)] = 0;
    else
        s->label_text[0] = 0;
    RegCloseKey(key);
    s->label_mode = clampi(s->label_mode, SF_LABEL_NONE, SF_LABEL_CUSTOM);
    s->accent_percent = clampi(s->accent_percent, 0, 50);
    s->speed_percent = clampi(s->speed_percent, 25, 300);
    s->trails_percent = clampi(s->trails_percent, 0, 95);
}

static void write_int(HKEY key, const char *name, int v) {
    DWORD d = (DWORD)v;
    RegSetValueExA(key, name, 0, REG_DWORD, (const BYTE *)&d, sizeof d);
}

static void save_settings(const settings *s) {
    HKEY key;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, NULL, 0, KEY_WRITE, NULL, &key, NULL) != ERROR_SUCCESS) return;
    RegSetValueExA(key, "Background", 0, REG_SZ, (const BYTE *)s->background, (DWORD)strlen(s->background) + 1);
    RegSetValueExA(key, "Stars", 0, REG_SZ, (const BYTE *)s->stars, (DWORD)strlen(s->stars) + 1);
    RegSetValueExA(key, "Accent", 0, REG_SZ, (const BYTE *)s->accent, (DWORD)strlen(s->accent) + 1);
    write_int(key, "AccentPercent", s->accent_percent);
    write_int(key, "SpeedPercent", s->speed_percent);
    write_int(key, "TrailsPercent", s->trails_percent);
    write_int(key, "ShowClock", s->show_clock);
    write_int(key, "Use24Hour", s->use_24h);
    write_int(key, "LabelMode", s->label_mode);
    RegSetValueExW(key, L"LabelText", 0, REG_SZ, (const BYTE *)s->label_text, (DWORD)((wcslen(s->label_text) + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
}

static COLORREF to_colorref(const char *hex) {
    sf_color c;
    if (!sf_parse_color(hex, &c)) return RGB(255, 255, 255);
    return RGB((int)(c.r * 255 + 0.5f), (int)(c.g * 255 + 0.5f), (int)(c.b * 255 + 0.5f));
}

static void from_colorref(COLORREF c, char *out) {
    snprintf(out, 8, "#%02x%02x%02x", GetRValue(c), GetGValue(c), GetBValue(c));
}

static int reduced_motion(void) {
    /* "Show animations in Windows" off is Windows' reduce-motion switch. */
    BOOL animations = TRUE;
    if (!SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0)) return 0;
    return !animations;
}

/* The text under the clock, or an empty string for none. */
static void label_text(const settings *s, wchar_t *out, int size) {
    out[0] = 0;
    DWORD n = (DWORD)size;
    if (s->label_mode == SF_LABEL_CUSTOM) {
        wcsncpy(out, s->label_text, (size_t)size - 1);
        out[size - 1] = 0;
    } else if (s->label_mode == SF_LABEL_USERNAME) {
        if (!GetUserNameW(out, &n)) out[0] = 0;
    } else if (s->label_mode == SF_LABEL_NAME) {
        /* The display name: from the domain, else the local account's full name, else the login. */
        if (!GetUserNameExW(NameDisplay, out, &n) || !out[0]) {
            wchar_t user[257];
            DWORD un = 257;
            out[0] = 0;
            if (GetUserNameW(user, &un)) {
                USER_INFO_10 *info = NULL;
                if (NetUserGetInfo(NULL, user, 10, (BYTE **)&info) == NERR_Success && info) {
                    if (info->usri10_full_name && info->usri10_full_name[0]) wcsncpy(out, info->usri10_full_name, (size_t)size - 1);
                    NetApiBufferFree(info);
                }
                if (!out[0]) wcsncpy(out, user, (size_t)size - 1);
                out[size - 1] = 0;
            }
        }
    }
    /* Trim spaces, and keep it to one line of SF_LABEL_MAX characters. */
    wchar_t *p = out;
    while (*p == L' ' || *p == L'\t') p++;
    memmove(out, p, (wcslen(p) + 1) * sizeof(wchar_t));
    for (wchar_t *q = out; *q; q++)
        if (*q == L'\r' || *q == L'\n' || *q == L'\t') *q = L' ';
    if (wcslen(out) > SF_LABEL_MAX) out[SF_LABEL_MAX] = 0;
    size_t len = wcslen(out);
    while (len && out[len - 1] == L' ') out[--len] = 0;
}

typedef struct {
    unsigned char *coverage;
    int w, h;
} text_mask;

/* Renders `text` in Segoe UI, `px` pixels tall, as a grayscale coverage mask. */
static text_mask render_text(const wchar_t *text, int px) {
    text_mask m = {NULL, 0, 0};
    if (!text[0] || px < 4) return m;
    HDC dc = CreateCompatibleDC(NULL);
    HFONT font = CreateFontW(-px, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HGDIOBJ old_font = SelectObject(dc, font);
    RECT r = {0, 0, 0, 0};
    DrawTextW(dc, text, -1, &r, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
    int w = r.right + 2, h = r.bottom;
    if (w > 4 && h > 0) {
        BITMAPINFO bmi;
        memset(&bmi, 0, sizeof bmi);
        bmi.bmiHeader.biSize = sizeof bmi.bmiHeader;
        bmi.bmiHeader.biWidth = w;
        bmi.bmiHeader.biHeight = -h;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        uint32_t *bits = NULL;
        HBITMAP bmp = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
        if (bmp && bits) {
            HGDIOBJ old_bmp = SelectObject(dc, bmp);
            memset(bits, 0, (size_t)w * (size_t)h * 4);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(255, 255, 255));
            r.left = 1, r.top = 0, r.right = w, r.bottom = h;
            DrawTextW(dc, text, -1, &r, DT_SINGLELINE | DT_NOPREFIX | DT_LEFT);
            GdiFlush();
            m.coverage = malloc((size_t)w * (size_t)h);
            if (m.coverage) {
                for (int i = 0; i < w * h; i++) m.coverage[i] = (unsigned char)(bits[i] >> 8 & 0xff);
                m.w = w;
                m.h = h;
            }
            SelectObject(dc, old_bmp);
        }
        if (bmp) DeleteObject(bmp);
    }
    SelectObject(dc, old_font);
    DeleteObject(font);
    DeleteDC(dc);
    return m;
}

/* ---------- saver windows ---------- */

#define MAX_SAVERS 16

typedef struct {
    HWND hwnd;
    starfield *sf;
    sf_canvas trail, frame;
    int width, height; /* pixels */
    float scale;
    text_mask label;
} saver;

static saver savers[MAX_SAVERS];
static int saver_count;
static settings config;
static int preview_mode;
static POINT start_cursor;
static ULONGLONG start_ms;
static int quitting;

typedef UINT(WINAPI *GetDpiForWindowFn)(HWND);
typedef HRESULT(WINAPI *DwmFlushFn)(void);
static GetDpiForWindowFn get_dpi_for_window;
static DwmFlushFn dwm_flush;

static float window_scale(HWND hwnd) {
    if (get_dpi_for_window) {
        UINT dpi = get_dpi_for_window(hwnd);
        if (dpi) return (float)dpi / 96.0f;
    }
    HDC dc = GetDC(hwnd);
    int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(hwnd, dc);
    return dpi > 0 ? (float)dpi / 96.0f : 1.0f;
}

static sf_color parse_or(const char *hex, const char *fallback) {
    sf_color c;
    if (!sf_parse_color(hex, &c)) sf_parse_color(fallback, &c);
    return c;
}

static void apply_options(saver *v) {
    sf_options o = sf_default_options();
    o.background = parse_or(config.background, SF_PRESETS[0].background);
    o.star = parse_or(config.stars, SF_PRESETS[0].stars);
    o.accent = parse_or(config.accent, SF_PRESETS[0].accent);
    o.accent_share = (float)config.accent_percent / 100.0f;
    o.speed = (float)config.speed_percent / 100.0f;
    o.trails = (float)config.trails_percent / 100.0f;
    o.reduced_motion = reduced_motion();
    o.compact = preview_mode;
    sf_set_options(v->sf, &o);
}

/* The clock and label shrink with the small preview. */
static float clock_scale(const saver *v) {
    if (!preview_mode || v->scale <= 0) return 1;
    float h = (float)v->height / v->scale / 900.0f;
    return h > 0.25f ? h : 0.25f;
}

static void resize_saver(saver *v) {
    RECT rc;
    GetClientRect(v->hwnd, &rc);
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    float scale = preview_mode ? 1.0f : window_scale(v->hwnd);
    if (w <= 0 || h <= 0 || (w == v->width && h == v->height && scale == v->scale)) return;
    sf_canvas_free(&v->trail);
    sf_canvas_free(&v->frame);
    if (!sf_canvas_init(&v->trail, w, h, scale) || !sf_canvas_init(&v->frame, w, h, scale)) {
        sf_canvas_free(&v->trail);
        v->width = v->height = 0;
        return;
    }
    v->width = w;
    v->height = h;
    v->scale = scale;
    sf_resize(v->sf, (float)w / scale, (float)h / scale);
    sf_canvas_clear(&v->trail, parse_or(config.background, SF_PRESETS[0].background));
    (void)sf_take_needs_clear(v->sf);

    free(v->label.coverage);
    wchar_t text[SF_LABEL_MAX + 260];
    label_text(&config, text, (int)(sizeof text / sizeof text[0]));
    v->label = render_text(text, (int)(14.0f * clock_scale(v) * scale + 0.5f));
}

static void present(saver *v, HDC dc) {
    if (!v->frame.pixels) return;
    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof bmi);
    bmi.bmiHeader.biSize = sizeof bmi.bmiHeader;
    bmi.bmiHeader.biWidth = v->frame.width;
    bmi.bmiHeader.biHeight = -v->frame.height; /* top-down */
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    SetDIBitsToDevice(dc, 0, 0, (DWORD)v->frame.width, (DWORD)v->frame.height, 0, 0, 0, (UINT)v->frame.height,
                      v->frame.pixels, &bmi, DIB_RGB_COLORS);
}

static void draw_frame(saver *v, float dt) {
    if (!v->trail.pixels) return;
    sf_step(v->sf, dt);
    if (sf_take_needs_clear(v->sf)) sf_canvas_clear(&v->trail, parse_or(config.background, SF_PRESETS[0].background));
    sf_renderer r = sf_canvas_renderer(&v->trail);
    sf_render(v->sf, &r);
    sf_canvas_copy(&v->frame, &v->trail);

    /* Bottom-left: the time in large type, with the optional label under it. */
    float h = (float)v->height / v->scale;
    float k = clock_scale(v);
    float bottom = h - 32 * k;
    sf_color ink = parse_or(config.stars, SF_PRESETS[0].stars);
    if (v->label.coverage) {
        ink.a = 0.5f;
        int top = (int)(bottom * v->scale + 0.5f) - v->label.h;
        sf_canvas_mask(&v->frame, v->label.coverage, v->label.w, v->label.h, (int)(32 * k * v->scale + 0.5f), top, ink);
        bottom = (float)top / v->scale - 8 * k;
    }
    if (config.show_clock) {
        char text[16];
        sf_format_time(text, sizeof text, config.use_24h);
        ink.a = 0.8f;
        sf_canvas_text(&v->frame, text, 32 * k, bottom, 34 * k, ink);
    }

    HDC dc = GetDC(v->hwnd);
    present(v, dc);
    ReleaseDC(v->hwnd, dc);
}

static void quit(void) {
    if (!quitting) {
        quitting = 1;
        PostQuitMessage(0);
    }
}

static LRESULT CALLBACK saver_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_SETCURSOR:
        if (!preview_mode) {
            SetCursor(NULL);
            return TRUE;
        }
        break;
    case WM_MOUSEMOVE:
        if (!preview_mode && GetTickCount64() - start_ms > 500) {
            POINT p;
            GetCursorPos(&p);
            if (abs(p.x - start_cursor.x) + abs(p.y - start_cursor.y) > 8) quit();
        }
        return 0;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_XBUTTONDOWN:
    case WM_MOUSEWHEEL:
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (!preview_mode) quit();
        return 0;
    case WM_ACTIVATEAPP:
        if (!preview_mode && !wp && GetTickCount64() - start_ms > 500) quit();
        return 0;
    case WM_SYSCOMMAND:
        /* Don't let the screen saver start again or the monitor menu interfere. */
        if (!preview_mode && ((wp & 0xfff0) == SC_SCREENSAVE || (wp & 0xfff0) == SC_CLOSE)) return 0;
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        for (int i = 0; i < saver_count; i++)
            if (savers[i].hwnd == hwnd) present(&savers[i], dc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_SIZE:
    case WM_DPICHANGED:
        for (int i = 0; i < saver_count; i++)
            if (savers[i].hwnd == hwnd) resize_saver(&savers[i]);
        if (msg == WM_DPICHANGED) {
            RECT *r = (RECT *)lp;
            SetWindowPos(hwnd, NULL, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        return 0;
    case WM_CLOSE:
        quit();
        return 0;
    case WM_DESTROY:
        quit();
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static saver *add_saver(HWND hwnd) {
    if (saver_count >= MAX_SAVERS) return NULL;
    saver *v = &savers[saver_count++];
    memset(v, 0, sizeof *v);
    v->hwnd = hwnd;
    v->sf = sf_create((unsigned)GetTickCount64() * 2654435761u + (unsigned)saver_count);
    apply_options(v);
    resize_saver(v);
    return v;
}

static BOOL CALLBACK monitor_proc(HMONITOR mon, HDC dc, LPRECT rect, LPARAM lp) {
    (void)mon, (void)dc;
    HINSTANCE inst = (HINSTANCE)lp;
    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, L"StarfieldSaver", L"Starfield", WS_POPUP | WS_VISIBLE,
                                rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, NULL, NULL, inst,
                                NULL);
    if (hwnd) add_saver(hwnd);
    return saver_count < MAX_SAVERS;
}

static int run_saver(HINSTANCE inst, HWND preview_parent) {
    WNDCLASSW wc;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = saver_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"StarfieldSaver";
    RegisterClassW(&wc);

    load_settings(&config);
    preview_mode = preview_parent != NULL;
    GetCursorPos(&start_cursor);
    start_ms = GetTickCount64();

    if (preview_mode) {
        RECT rc;
        GetClientRect(preview_parent, &rc);
        HWND hwnd = CreateWindowExW(0, L"StarfieldSaver", L"Starfield", WS_CHILD | WS_VISIBLE, 0, 0, rc.right, rc.bottom,
                                    preview_parent, NULL, inst, NULL);
        if (!hwnd) return 1;
        add_saver(hwnd);
    } else {
        EnumDisplayMonitors(NULL, NULL, monitor_proc, (LPARAM)inst);
        if (saver_count == 0) return 1;
        SetForegroundWindow(savers[0].hwnd);
        SetFocus(savers[0].hwnd);
    }

    LARGE_INTEGER freq, last, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last);
    double frame_s = 1.0 / 60.0;
    MSG msg;
    for (;;) {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) goto done;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        /* The preview stops when Screen Saver Settings closes its parent window. */
        if (preview_mode && !IsWindow(preview_parent)) break;

        QueryPerformanceCounter(&now);
        float dt = (float)((double)(now.QuadPart - last.QuadPart) / (double)freq.QuadPart);
        last = now;
        for (int i = 0; i < saver_count; i++) draw_frame(&savers[i], dt);

        /* Wait for the next vertical blank, or sleep out the rest of the frame. */
        if (!(dwm_flush && SUCCEEDED(dwm_flush()))) {
            QueryPerformanceCounter(&now);
            double used = (double)(now.QuadPart - last.QuadPart) / (double)freq.QuadPart;
            if (used < frame_s) Sleep((DWORD)((frame_s - used) * 1000.0));
        }
    }
done:
    for (int i = 0; i < saver_count; i++) {
        sf_canvas_free(&savers[i].trail);
        sf_canvas_free(&savers[i].frame);
        free(savers[i].label.coverage);
        sf_destroy(savers[i].sf);
    }
    return 0;
}

/* ---------- Options dialog ---------- */

static settings editing;
static COLORREF custom_colors[16];

static void set_label(HWND dlg, int id, const wchar_t *text) { SetDlgItemTextW(dlg, id, text); }

static void update_labels(HWND dlg) {
    wchar_t buf[32];
    swprintf(buf, 32, L"%d%%", editing.accent_percent);
    set_label(dlg, IDC_ACCENT_VAL, buf);
    swprintf(buf, 32, L"%g×", editing.speed_percent / 100.0);
    set_label(dlg, IDC_SPEED_VAL, buf);
    int t = editing.trails_percent;
    set_label(dlg, IDC_TRAILS_VAL, t < 30 ? L"Short" : (t < 70 ? L"Medium" : L"Long"));
    EnableWindow(GetDlgItem(dlg, IDC_24H), editing.show_clock);
    ShowWindow(GetDlgItem(dlg, IDC_LABEL_TEXT), editing.label_mode == SF_LABEL_CUSTOM ? SW_SHOW : SW_HIDE);
}

static void select_matching_preset(HWND dlg) {
    int match = SF_PRESET_COUNT; /* "Custom" */
    for (int i = 0; i < SF_PRESET_COUNT; i++)
        if (!_stricmp(SF_PRESETS[i].background, editing.background) && !_stricmp(SF_PRESETS[i].stars, editing.stars) &&
            !_stricmp(SF_PRESETS[i].accent, editing.accent))
            match = i;
    SendDlgItemMessageW(dlg, IDC_PRESET, CB_SETCURSEL, (WPARAM)match, 0);
}

static void load_dialog(HWND dlg) {
    SendDlgItemMessageW(dlg, IDC_ACCENT_PCT, TBM_SETPOS, TRUE, editing.accent_percent);
    SendDlgItemMessageW(dlg, IDC_SPEED, TBM_SETPOS, TRUE, editing.speed_percent / 5);
    SendDlgItemMessageW(dlg, IDC_TRAILS, TBM_SETPOS, TRUE, editing.trails_percent);
    CheckDlgButton(dlg, IDC_CLOCK, editing.show_clock ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dlg, IDC_24H, editing.use_24h ? BST_CHECKED : BST_UNCHECKED);
    SendDlgItemMessageW(dlg, IDC_LABEL_MODE, CB_SETCURSEL, (WPARAM)editing.label_mode, 0);
    SetDlgItemTextW(dlg, IDC_LABEL_TEXT, editing.label_text);
    select_matching_preset(dlg);
    update_labels(dlg);
    InvalidateRect(GetDlgItem(dlg, IDC_BACKGROUND), NULL, TRUE);
    InvalidateRect(GetDlgItem(dlg, IDC_STARS), NULL, TRUE);
    InvalidateRect(GetDlgItem(dlg, IDC_ACCENT), NULL, TRUE);
}

static char *color_for(int id) {
    return id == IDC_BACKGROUND ? editing.background : (id == IDC_STARS ? editing.stars : editing.accent);
}

static void pick_color(HWND dlg, int id) {
    CHOOSECOLORW cc;
    memset(&cc, 0, sizeof cc);
    cc.lStructSize = sizeof cc;
    cc.hwndOwner = dlg;
    cc.rgbResult = to_colorref(color_for(id));
    cc.lpCustColors = custom_colors;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (!ChooseColorW(&cc)) return;
    from_colorref(cc.rgbResult, color_for(id));
    InvalidateRect(GetDlgItem(dlg, id), NULL, TRUE);
    select_matching_preset(dlg);
}

static INT_PTR CALLBACK config_proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG: {
        for (int i = 0; i < SF_PRESET_COUNT; i++) SendDlgItemMessageA(dlg, IDC_PRESET, CB_ADDSTRING, 0, (LPARAM)SF_PRESETS[i].name);
        SendDlgItemMessageW(dlg, IDC_PRESET, CB_ADDSTRING, 0, (LPARAM)L"Custom");
        SendDlgItemMessageW(dlg, IDC_ACCENT_PCT, TBM_SETRANGE, TRUE, MAKELPARAM(0, 50));
        SendDlgItemMessageW(dlg, IDC_SPEED, TBM_SETRANGE, TRUE, MAKELPARAM(5, 60)); /* x5 = 25..300% */
        SendDlgItemMessageW(dlg, IDC_TRAILS, TBM_SETRANGE, TRUE, MAKELPARAM(0, 95));
        SetDlgItemTextA(dlg, IDC_VERSION, "Version " VERSION_STR);
        const wchar_t *modes[] = {L"Nothing", L"Your name", L"Your username", L"Custom text"};
        for (int i = 0; i < 4; i++) SendDlgItemMessageW(dlg, IDC_LABEL_MODE, CB_ADDSTRING, 0, (LPARAM)modes[i]);
        SendDlgItemMessageW(dlg, IDC_LABEL_TEXT, EM_SETLIMITTEXT, SF_LABEL_MAX, 0);
        SendDlgItemMessageW(dlg, IDC_LABEL_TEXT, EM_SETCUEBANNER, TRUE, (LPARAM)L"Text under the clock");
        load_settings(&editing);
        load_dialog(dlg);
        return TRUE;
    }
    case WM_HSCROLL: {
        HWND bar = (HWND)lp;
        int pos = (int)SendMessageW(bar, TBM_GETPOS, 0, 0);
        int id = GetDlgCtrlID(bar);
        if (id == IDC_ACCENT_PCT) editing.accent_percent = pos;
        if (id == IDC_SPEED) editing.speed_percent = pos * 5;
        if (id == IDC_TRAILS) editing.trails_percent = pos;
        update_labels(dlg);
        return TRUE;
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT *d = (DRAWITEMSTRUCT *)lp;
        HBRUSH fill = CreateSolidBrush(to_colorref(color_for((int)d->CtlID)));
        FillRect(d->hDC, &d->rcItem, fill);
        DeleteObject(fill);
        FrameRect(d->hDC, &d->rcItem, (HBRUSH)GetStockObject(GRAY_BRUSH));
        if (d->itemState & ODS_FOCUS) {
            RECT r = d->rcItem;
            InflateRect(&r, -3, -3);
            DrawFocusRect(d->hDC, &r);
        }
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_PRESET:
            if (HIWORD(wp) == CBN_SELCHANGE) {
                int i = (int)SendDlgItemMessageW(dlg, IDC_PRESET, CB_GETCURSEL, 0, 0);
                if (i >= 0 && i < SF_PRESET_COUNT) {
                    strcpy(editing.background, SF_PRESETS[i].background);
                    strcpy(editing.stars, SF_PRESETS[i].stars);
                    strcpy(editing.accent, SF_PRESETS[i].accent);
                    load_dialog(dlg);
                }
            }
            return TRUE;
        case IDC_BACKGROUND:
        case IDC_STARS:
        case IDC_ACCENT:
            pick_color(dlg, LOWORD(wp));
            return TRUE;
        case IDC_CLOCK:
            editing.show_clock = IsDlgButtonChecked(dlg, IDC_CLOCK) == BST_CHECKED;
            update_labels(dlg);
            return TRUE;
        case IDC_24H:
            editing.use_24h = IsDlgButtonChecked(dlg, IDC_24H) == BST_CHECKED;
            return TRUE;
        case IDC_LABEL_MODE:
            if (HIWORD(wp) == CBN_SELCHANGE) {
                int i = (int)SendDlgItemMessageW(dlg, IDC_LABEL_MODE, CB_GETCURSEL, 0, 0);
                editing.label_mode = i >= 0 ? i : SF_LABEL_NONE;
                update_labels(dlg);
                if (editing.label_mode == SF_LABEL_CUSTOM) SetFocus(GetDlgItem(dlg, IDC_LABEL_TEXT));
            }
            return TRUE;
        case IDC_RESET:
            default_settings(&editing);
            load_dialog(dlg);
            return TRUE;
        case IDOK:
            GetDlgItemTextW(dlg, IDC_LABEL_TEXT, editing.label_text, SF_LABEL_MAX + 1);
            save_settings(&editing);
            EndDialog(dlg, IDOK);
            return TRUE;
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

/* ---------- entry ---------- */

static HWND parse_hwnd(const char *s) {
    while (*s == ' ' || *s == ':' || *s == '"') s++;
    return (HWND)(UINT_PTR)_strtoui64(s, NULL, 10);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show) {
    (void)prev, (void)show;
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    typedef BOOL(WINAPI * SetCtxFn)(HANDLE);
    SetCtxFn set_ctx = user32 ? (SetCtxFn)(void *)GetProcAddress(user32, "SetProcessDpiAwarenessContext") : NULL;
    if (!set_ctx || !set_ctx((HANDLE)-4 /* PER_MONITOR_AWARE_V2 */)) SetProcessDPIAware();
    if (user32) get_dpi_for_window = (GetDpiForWindowFn)(void *)GetProcAddress(user32, "GetDpiForWindow");
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    if (dwm) dwm_flush = (DwmFlushFn)(void *)GetProcAddress(dwm, "DwmFlush");

    const char *p = cmd ? cmd : "";
    while (*p == ' ' || *p == '"') p++;
    char mode = 'c';
    if (*p == '/' || *p == '-') mode = (char)(p[1] | 0x20), p += 2;

    if (mode == 's') return run_saver(inst, NULL);
    if (mode == 'p') {
        HWND parent = parse_hwnd(p);
        return IsWindow(parent) ? run_saver(inst, parent) : 1;
    }
    if (mode == 'c') {
        INITCOMMONCONTROLSEX icc = {sizeof icc, ICC_BAR_CLASSES | ICC_STANDARD_CLASSES};
        InitCommonControlsEx(&icc);
        HWND owner = parse_hwnd(p);
        DialogBoxParamW(inst, MAKEINTRESOURCEW(IDD_CONFIG), IsWindow(owner) ? owner : NULL, config_proc, 0);
        return 0;
    }
    return 0; /* /a (change password) is from Windows 9x and has nothing to do. */
}
