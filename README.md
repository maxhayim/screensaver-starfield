# Starfield — a screen saver

A 90s flight through space. Stars stream out from the center of the screen, leaving short streaks that fade like a CRT's afterglow, and about one in fourteen glows orange. It started life on [maxhayim.com](https://maxhayim.com).

## Platforms

| OS | Status |
| --- | --- |
| macOS 11 and later (Apple silicon and Intel) | ✅ `Starfield.saver` |
| Windows (`.scr`) | Planned |
| Linux (XScreenSaver) | Planned |

## Install on macOS

1. Download `Starfield.saver.zip` from [Releases](../../releases) and unzip it.
2. Double-click `Starfield.saver` and choose to install it for this user.
3. The saver isn't notarized by Apple, so macOS blocks it the first time. Open **System Settings → Privacy & Security**, scroll down, and click **Open Anyway**. Or run this in Terminal:
   ```sh
   xattr -d com.apple.quarantine ~/Library/Screen\ Savers/Starfield.saver
   ```
4. Choose **Starfield** in **System Settings → Screen Saver**.

## Settings

Click **Options…** next to Starfield in Screen Saver settings.

- **Colors:** background, stars, and accent stars, each with its own color picker. Presets: Original, Classic, Deep space, Green terminal, Amber terminal, Synthwave, and Paper.
- **Accent stars:** how many stars get the accent color, from 0 to 50%. The site uses 7%.
- **Speed:** from a quarter of the site's speed up to 3×.
- **Trails:** how long the streaks linger.
- **Clock:** the time in the bottom-left corner. Show it or hide it, in 12- or 24-hour time. It takes the star color.
- **Reset to defaults** puts everything back to the original look.

With **Reduce motion** on in macOS Accessibility settings, the flight is slower.

## How it works

- `core/`: the drawing core in plain C99, ported from `StarfieldCanvas` on the site. It moves the stars and draws through three callbacks (`fill`, `line`, `circle`), so each OS only plugs in its own 2D drawing. The screen is never cleared. Each frame first lays a see-through coat of the background color over the last one, which leaves the trails, so each OS keeps its own drawing surface between frames.
- `macos/`: the Swift `ScreenSaverView`, which draws into an offscreen bitmap, and the Options sheet.

## Build

You need only the Xcode command-line tools (`xcode-select --install`), not Xcode itself.

```sh
macos/build.sh                         # -> build/Starfield.saver (universal, ad-hoc signed)
```

Test without installing:

```sh
cc -std=c99 -Icore core/starfield.c tests/test_starfield.c -lm -o build/test_starfield && build/test_starfield
swiftc tools/preview.swift -o build/preview -framework ScreenSaver
build/preview build/Starfield.saver shot.png 5           # render 5 seconds to a PNG
```

## License

MIT. See [LICENSE](LICENSE).
