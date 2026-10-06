# Contributing

Contributions are welcome.

## Before opening a pull request

1. Open an issue describing the proposed change or bug.
2. Change the look in one place: the flight lives in `core/starfield.c`, and every system draws through it. Keep the defaults (`#0b0b0a` background, `#eeebe4` stars, `#f06a2a` accent stars, 7% accent) and the presets in step across `core/presets.h`, `macos/Settings.swift`, and `linux/screensaver-starfield.xml`.
3. Keep all three systems working. A setting added on one belongs on the others: macOS Options, the Windows dialog and registry, and the Linux flags and XScreenSaver XML.
4. Keep it offline. Starfield doesn't connect to anything: no analytics, update checks, or other services.
5. Keep it free: no paid code signing, services, or dependencies.
6. Run the tests and build every system you can (see [docs/GUIDE.md](docs/GUIDE.md)). CI builds and tests all three on every push.
7. Test inside the real screen saver host, not only in the preview tools: System Settings on macOS, Screen Saver Settings on Windows, XScreenSaver on Linux. Each one behaves differently.

## Pull requests

Include:
- A concise description of the change
- Which systems it affects
- How it was tested, and on which systems (Windows, macOS, Linux)
- Screenshots for visual changes
