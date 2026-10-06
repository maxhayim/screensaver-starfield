# Security Policy

Starfield doesn't connect to the internet or to any other computer. It only draws on your screen and reads its own settings.

## What it keeps

Only its settings: colors, flight settings, the clock, and the text under it.

- **macOS:** the screen saver's preferences, in your user Library (under the `legacyScreenSaver` container).
- **Windows:** the registry, under `HKEY_CURRENT_USER\Software\maxhayim\Starfield`.
- **Linux:** XScreenSaver's own settings in `~/.xscreensaver`.

If you put your name or other text under the clock, anyone who can see your screen can read it. On Linux, the text is passed on the command line, where other users of the computer can see it.

## Do not publish

Don't include personal details from your screen, such as your name or username under the clock, in GitHub issues, pull requests, or screenshots unless you mean to.

## Reporting a vulnerability

For security-sensitive reports, contact the repository owner privately through an appropriate GitHub contact method instead of opening a public issue.
