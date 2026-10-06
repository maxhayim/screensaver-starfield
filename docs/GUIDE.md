# Starfield Screensaver — developer guide

Starfield is one screen saver built three times: a `.saver` for macOS, a `.scr` for Windows, and an XScreenSaver hack for Linux. All three move the stars with the same C core, so the flight looks the same everywhere; each system only adds its own window, drawing, and settings.

## Layout

```
core/
  starfield.h, .c         the flight: stars, their streaks, the afterglow fade
  canvas.h, .c            software renderer for Windows and Linux: anti-aliased lines,
                          the fade, the clock's stroke font, OS-rendered text masks
  presets.h               color presets and defaults (Windows, Linux)
  module.modulemap        lets Swift import the core as StarfieldCore
macos/
  StarfieldView.swift     the ScreenSaverView: Core Graphics into a kept bitmap, the clock and label
  ConfigureSheet.swift    the Options sheet
  Settings.swift          settings in ScreenSaverDefaults, presets, the label
  Info.plist, Resources/  the bundle: principal class StarfieldView, thumbnails
  build.sh                builds build/Starfield.saver, universal and ad-hoc signed
windows/
  starfield_win.c         /s, /p, /c; GDI, the registry, the settings dialog
  starfield.rc, resource.h  the dialog, the name in Screen Saver Settings (string 1), version
  starfield.manifest      modern controls and per-monitor DPI awareness
  build.sh                cross-compiles build/Starfield.scr with mingw-w64
  INSTALL.txt             goes in the download
linux/
  starfield_x11.c         the hack: Xlib (+ MIT-SHM), Xft for the label, flags
  screensaver-starfield.xml  the XScreenSaver settings page
  Makefile, install.sh, INSTALL.txt
tests/test_starfield.c    core and canvas tests
tools/
  preview.swift           loads Starfield.saver like the host does and renders PNGs
  render.c                renders the Windows/Linux drawing path to a BMP on any OS
docs/                     this guide; assets/ holds the logo and screenshot
.github/workflows/        build, test, screenshots, and release
```

## The core

`starfield.c` owns everything that moves. A platform creates a `starfield`, sets its options (colors, accent share, speed, trails, reduced motion, compact for previews), tells it the size in points, calls `sf_step(dt)` every frame, and draws with `sf_render`, supplying three callbacks: `fill`, `line`, and `circle`.

- It's the site's `StarfieldCanvas`: stars start at a random spot in a 2.4 × 2.4 square and fly toward the screen (`z` from 1 to 0), projected from the center. Speed is 0.34 of the depth per second, or 0.12 with reduced motion.
- Star count scales with area (width × height / 4200, from 220 to 520); previews use 110.
- **The screen is never cleared.** Each frame begins with `fill` in the background color at partial opacity (42% per 60 fps frame by default, adjusted for the real frame time), so last frame's stars fade into streaks. Hosts must keep their drawing surface between frames, and clear it when `sf_take_needs_clear` says so (after a resize or a new background).
- About 7% of stars use the accent color.

## The canvas (Windows and Linux)

`canvas.c` draws into a 32-bit `0x00RRGGBB` buffer, the layout of a Windows DIB and a TrueColor X image.

- Lines are round-capped and anti-aliased by distance; lines thinner than a pixel fade instead of flickering.
- Fades always move at least one step toward the background, so trails never leave a haze.
- The clock uses a small stroke font (digits, `:`, `AM`, `PM`). Overlapping strokes are merged into one coverage mask first, so joints don't double up.
- The label under the clock is rendered by the OS (GDI on Windows, Xft on Linux) into a grayscale mask and painted with `sf_canvas_mask`, so any language works.
- Each frame: step, render into the kept "trail" canvas, copy it to the "frame" canvas, draw the clock and label there, and show the frame.

## macOS

- Since macOS 14, screen savers run inside `legacyScreenSaver`. It can keep a saver running after the screen saver ends, so Starfield quits on `com.apple.screensaver.willstop`.
- The view draws the stars into its own bitmap (`CGContext`), which keeps the trails, and copies it to the screen in `draw(_:)`. The clock and label are drawn on top with AppKit text.
- Settings are in `ScreenSaverDefaults` for `com.maxhayim.screensaver-starfield`.
- `build.sh` builds arm64 and x86_64, joins them with `lipo`, copies the thumbnails, clears Dropbox's extended attributes (codesign rejects them), and signs ad hoc.

## Windows

- `Starfield.scr /s` opens one window per monitor, each with its own stars. It quits on a key, a click, the wheel, or the mouse moving more than 8 pixels (ignoring the first 0.5 seconds).
- `/p <HWND>` draws inside Screen Saver Settings' preview and ends when that window closes. `/c` (or no argument) opens the settings dialog.
- Windows starts a `.scr` file with `/S` no matter what else you pass, so to test `/c` or `/p` from a script, copy it to `Starfield.exe` first.
- Settings are in `HKEY_CURRENT_USER\Software\maxhayim\Starfield`.
- Frames wait on `DwmFlush` (the display's refresh), or sleep to 60 fps if that isn't available. "Show animations in Windows" off counts as reduced motion.
- Text that starts with a right-to-left letter is drawn with `DT_RTLREADING`, to match macOS and Linux.

## Linux

- XScreenSaver passes its window in `$XSCREENSAVER_WINDOW`; `-root` and `-window-id` work too. `-window` opens a window of its own, which any key closes.
- Settings come from flags (`-preset`, `-background`, `-label`, …); the XScreenSaver XML builds them. `-help` lists them all.
- Frames go to the window through MIT-SHM when the X server is local, and `XPutImage` otherwise (`STARFIELD_NO_SHM=1` forces that). Unusual visuals are converted pixel by pixel.
- HiDPI scale comes from `Xft.dpi`, or `-scale`.
- Xft doesn't reorder right-to-left text, so `to_display_order` does a small part of the Unicode bidi algorithm: enough for names and short labels.

## Building and testing

```
cc -std=c99 -Icore core/starfield.c core/canvas.c tests/test_starfield.c -lm -o build/test_starfield && build/test_starfield
macos/build.sh                           # build/Starfield.saver
windows/build.sh                         # build/Starfield.scr (brew install mingw-w64, or apt install mingw-w64)
make -C linux                            # build/screensaver-starfield (libx11-dev libxext-dev libxft-dev)
swiftc tools/preview.swift -o build/preview -framework ScreenSaver
build/preview build/Starfield.saver shot.png 5 [width height] [--preview]
```

## Releasing

1. Add a section to `CHANGELOG.md` (`## [x.y.z] - date`) and set `VERSION`.
2. Commit and push, and wait for CI to pass.
3. Tag with the release's short name as the message, and push the tag:
   ```
   git tag -a v1.2.3 -m "Short Name" && git push origin v1.2.3
   ```
   The workflow builds everything and publishes the release as "v1.2.3 — Short Name", with this version's changelog section and the install notes.
