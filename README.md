# Starfield — a screen saver

A 90s flight through space. Stars stream out from the center of the screen, leaving short streaks that fade like a CRT's afterglow, and about one in fourteen glows orange. It started life on [maxhayim.com](https://maxhayim.com).

## Platforms

| OS | Status |
| --- | --- |
| macOS 11 and later (Apple silicon and Intel) | ✅ `Starfield.saver` |
| Windows 10 and 11 (64-bit) | ✅ `Starfield.scr` |
| Linux and other Unix desktops, with XScreenSaver (x86_64 and arm64) | ✅ `starfield` |

## Install on macOS

1. Download `Starfield.saver.zip` from [Releases](../../releases) and unzip it.
2. Double-click `Starfield.saver` and choose to install it for this user.
3. The saver isn't notarized by Apple, so macOS blocks it the first time. Open **System Settings → Privacy & Security**, scroll down, and click **Open Anyway**. Or run this in Terminal:
   ```sh
   xattr -d com.apple.quarantine ~/Library/Screen\ Savers/Starfield.saver
   ```
4. Choose **Starfield** in **System Settings → Screen Saver**.

## Install on Windows

1. Download `Starfield-windows.zip` from [Releases](../../releases) and unzip it.
2. Right-click `Starfield.scr`, choose **Properties**, tick **Unblock**, and click **OK**. Windows marks downloaded files, and screen savers from marked files don't start.
3. Move `Starfield.scr` to a folder where it can stay. To offer it to every user, put it in `C:\Windows\System32` (this needs an administrator).
4. Right-click it and choose **Install**. Screen Saver Settings opens with Starfield picked.

It isn't code-signed, so SmartScreen may say "Windows protected your PC". Click **More info**, then **Run anyway**.

## Install on Linux

Starfield runs under [XScreenSaver](https://www.jwz.org/xscreensaver/). GNOME and KDE don't support third-party screen savers, so install XScreenSaver first if you don't have it (`sudo apt install xscreensaver`, or your distribution's package).

1. Download the tarball for your machine from [Releases](../../releases): `x86_64` for most PCs, `arm64` for a Raspberry Pi 4 or 5 on a 64-bit OS, or other 64-bit ARM.
2. Install it:
   ```sh
   tar xzf starfield-linux-*.tar.gz && cd starfield
   sudo ./install.sh     # copies the saver and its settings into place
   ./install.sh --add    # adds it to your XScreenSaver list
   ```
3. Choose **Starfield** in `xscreensaver-settings`.

To try it in a window first: `./starfield -window`. Run `./starfield -help` for every option. Building it yourself needs `libx11-dev`, `libxext-dev`, and `libxft-dev`.

## Settings

On macOS, click **Options…** next to Starfield in Screen Saver settings. On Windows, click **Settings…** in Screen Saver Settings. On Linux, use the settings panel in `xscreensaver-settings`. All three offer:

- **Colors:** background, stars, and accent stars, each with its own color picker. Presets: Original, Classic, Deep space, Green terminal, Amber terminal, Synthwave, and Paper.
- **Accent stars:** how many stars get the accent color, from 0 to 50%. The site uses 7%.
- **Speed:** from a quarter of the site's speed up to 3×.
- **Trails:** how long the streaks linger.
- **Clock:** the time in the bottom-left corner. Show it or hide it, in 12- or 24-hour time. It takes the star color.
- **Under the clock:** nothing, your name, your username, or text of your own (up to 80 characters, in any language, including right-to-left scripts like Hebrew). It shows in smaller, dimmer type, even with the clock hidden.
- **Reset to defaults** puts everything back to the original look.

The flight slows down with **Reduce motion** on in macOS Accessibility settings, with **Show animations in Windows** off on Windows, and with the **Reduce motion** box on Linux.

## How it works

- `core/`: the drawing core in plain C99, ported from `StarfieldCanvas` on the site. It moves the stars and draws through three callbacks (`fill`, `line`, `circle`), so each OS only plugs in its own 2D drawing. The screen is never cleared. Each frame first lays a see-through coat of the background color over the last one, which leaves the trails, so each OS keeps its own drawing surface between frames.
- `core/canvas.c`: a small software renderer for Windows and Linux. It draws anti-aliased streaks into a pixel buffer, and draws the clock digits with its own stroke font. The text under the clock comes from the OS (GDI on Windows, Xft on Linux), so any language works.
- `macos/`: the Swift `ScreenSaverView`, which draws with Core Graphics into an offscreen bitmap, and the Options sheet.
- `windows/`: a plain Win32 program in C. It handles `/s` (one window per monitor), `/p` (the preview), and `/c` (the Settings dialog), and keeps its settings in the registry under `HKEY_CURRENT_USER\Software\maxhayim\Starfield`.
- `linux/`: an XScreenSaver hack in C on plain Xlib (with shared memory when it can), plus its settings XML.

## Build

```sh
macos/build.sh            # build/Starfield.saver, universal, ad-hoc signed (needs: xcode-select --install)
windows/build.sh          # build/Starfield.scr, with MinGW-w64 (brew install mingw-w64, or apt install mingw-w64)
make -C linux             # linux/starfield (needs libx11-dev, libxext-dev, and libxft-dev)
```

Test without installing:

```sh
cc -std=c99 -Icore core/starfield.c core/canvas.c tests/test_starfield.c -lm -o build/test_starfield && build/test_starfield
swiftc tools/preview.swift -o build/preview -framework ScreenSaver
build/preview build/Starfield.saver shot.png 5           # macOS: render 5 seconds to a PNG
cc -std=c99 -O2 -Icore core/starfield.c core/canvas.c tools/render.c -lm -o build/render
build/render shot.bmp 5                                  # Windows/Linux drawing, on any OS
```

GitHub Actions builds all three on every push, runs the tests, and runs the Windows and Linux savers to take screenshots. Pushing a tag like `v0.1.0` publishes a release with the downloads.

## License

MIT. See [LICENSE](LICENSE).
