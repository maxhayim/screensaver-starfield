#!/bin/sh
# Installs Starfield for XScreenSaver from the release tarball:
#   sudo ./install.sh          copy the hack and its settings into place
#   ./install.sh --add         add it to your own ~/.xscreensaver list (no sudo)
set -eu
cd "$(dirname "$0")"

add_to_list() {
  f="$HOME/.xscreensaver"
  if [ ! -f "$f" ]; then
    echo "No ~/.xscreensaver yet. Open xscreensaver-settings once, close it, and run ./install.sh --add again."
    exit 1
  fi
  if grep -q "starfield -root" "$f"; then
    echo "Starfield is already in ~/.xscreensaver."
    return
  fi
  cp "$f" "$f.bak"
  # The list of hacks follows the "programs:" line, one per line, each ending in \n\.
  awk '{ print } /^programs:/ && !done { print "\t\t\t\t  starfield -root\t\t\t    \\n\\"; done = 1 }' "$f.bak" > "$f"
  echo "Added Starfield to ~/.xscreensaver (the old file is ~/.xscreensaver.bak)."
}

if [ "${1:-}" = "--add" ]; then
  add_to_list
  exit 0
fi

for d in /usr/libexec/xscreensaver /usr/lib/xscreensaver /usr/lib64/xscreensaver; do
  if [ -d "$d" ]; then HACKDIR=$d; break; fi
done
if [ -z "${HACKDIR:-}" ]; then
  echo "XScreenSaver isn't installed (no hack directory in /usr/libexec or /usr/lib)."
  echo "Install it first, e.g. sudo apt install xscreensaver"
  exit 1
fi
install -D -m 755 starfield "$HACKDIR/starfield"
install -D -m 644 starfield.xml /usr/share/xscreensaver/config/starfield.xml
echo "Installed $HACKDIR/starfield"
echo "Now run ./install.sh --add (without sudo) to add it to your screen saver list."
