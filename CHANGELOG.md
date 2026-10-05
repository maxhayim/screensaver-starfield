# Changelog

## 0.2.0

- **Text under the clock:** show your name, your username, or text of your own under the time, in smaller type. It works in any language, including right-to-left scripts like Hebrew, and it can show even with the clock hidden. Pick it in the saver's settings on all three systems. It's off unless you turn it on.
- Linux: building from source now also needs `libxft-dev` (the release builds already include it).

## 0.1.0

The first version of Starfield, a 90s flight through space, as a native screen saver for macOS, Windows, and Linux.

- **macOS:** `Starfield.saver` for macOS 11 and later, on Apple silicon and Intel.
- **Windows:** `Starfield.scr` for Windows 10 and 11. It covers every monitor, draws the small preview in Screen Saver Settings, and stays sharp on high-resolution displays.
- **Linux:** a saver for XScreenSaver, built for x86_64 and arm64, with its own panel in `xscreensaver-settings`.
- **Colors you choose:** background, stars, and accent stars, with seven presets (Original, Classic, Deep space, Green terminal, Amber terminal, Synthwave, Paper).
- **Flight settings:** how many stars get the accent color, speed, and how long the streaks linger.
- **Clock:** the time in the bottom-left corner, in 12- or 24-hour format, or hidden.
- Slows down when your system asks for reduced motion.
