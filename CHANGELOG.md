# Changelog

All notable changes to the Starfield screen saver are documented here.

## [0.3.0] - 2026-10-06

### Added
- **macOS: a picture in the screen saver list** in System Settings, instead of a blank icon.
- **Install notes in every download:** `INSTALL.txt` in the Windows zip and the Linux tarball.
- **Community files:** `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`, and `SECURITY.md`.
- **Developer guide** in `docs/GUIDE.md`, and `docs/assets/` for the logo and screenshot.

### Changed
- **Linux: the program is now `screensaver-starfield`** (it was `starfield`), so its name can't clash with another program. `install.sh` removes the old one and updates `~/.xscreensaver`.
- **Release downloads** are named after the repository and version: `screensaver-starfield-<version>-macos.zip`, `-windows.zip`, `-linux-x86_64.tar.gz`, and `-linux-arm64.tar.gz`.
- Release notes follow the changelog, with a Compatibility section.
- README rewritten: what's on screen, install and update steps, settings, privacy, repository layout, and versioning.

## [0.2.0] - 2026-10-05

### Added
- **Text under the clock:** your name, your username, or text of your own, in smaller type under the time. It works in any language, including right-to-left scripts like Hebrew, and shows even with the clock hidden. Pick it in the settings on all three systems. It's off unless you turn it on.

### Changed
- Linux: building from source also needs `libxft-dev` (the release downloads already include it).

## [0.1.0] - 2026-10-03

### Added
- **The Starfield screen saver for macOS, Windows, and Linux:** a `.saver` for macOS 11 and later (Apple silicon and Intel), a `.scr` for Windows 10 and 11, and an XScreenSaver hack for Linux (x86_64 and arm64).
- **A 90s flight through space:** stars stream out from the center, leaving short streaks that fade like a CRT's afterglow, and about one in fourteen glows orange.
- **Colors** for the background, stars, and accent stars, with presets: Original, Classic, Deep space, Green terminal, Amber terminal, Synthwave, and Paper.
- **Flight settings:** how many stars get the accent color, speed, and how long the streaks linger.
- **Clock** in the bottom-left corner, in 12- or 24-hour time, or turned off.
- Slows down when the system asks for reduced motion.
