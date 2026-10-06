#!/bin/sh
# Installs Starfield for the current user's XScreenSaver.
set -eu
cd "$(dirname "$0")"

# The hack directory differs by distribution.
for dir in /usr/libexec/xscreensaver /usr/lib/xscreensaver /usr/lib64/xscreensaver /usr/local/libexec/xscreensaver; do
  [ -d "$dir" ] && HACKDIR=$dir && break
done
for dir in /usr/share/xscreensaver/config /usr/local/share/xscreensaver/config; do
  [ -d "$dir" ] && CONFDIR=$dir && break
done
: "${HACKDIR:?XScreenSaver not found. Install it first (e.g. sudo apt install xscreensaver).}"
: "${CONFDIR:=/usr/share/xscreensaver/config}"

echo "Installing to $HACKDIR and $CONFDIR (needs sudo)"
sudo install -m 755 screensaver-starfield "$HACKDIR/screensaver-starfield"
sudo install -D -m 644 screensaver-starfield.xml "$CONFDIR/screensaver-starfield.xml"

# Versions before 0.3.0 installed the program as "starfield".
if [ -f "$HACKDIR/starfield" ] || [ -f "$CONFDIR/starfield.xml" ]; then
  sudo rm -f "$HACKDIR/starfield" "$CONFDIR/starfield.xml"
  echo "Removed the old \"starfield\" program from an earlier version."
fi

XS="$HOME/.xscreensaver"
if [ -f "$XS" ] && grep -q "[[:space:]]starfield -root" "$XS" && ! grep -q "screensaver-starfield" "$XS"; then
  cp "$XS" "$XS.bak"
  sed 's/\([[:space:]]\)starfield -root/\1screensaver-starfield -root/' "$XS.bak" > "$XS"
  echo "Renamed starfield to screensaver-starfield in ~/.xscreensaver (the old file is ~/.xscreensaver.bak)."
elif [ -f "$XS" ] && ! grep -q screensaver-starfield "$XS"; then
  echo
  echo "Add this line to the programs: list in ~/.xscreensaver, then pick Starfield in xscreensaver-settings:"
  echo "  screensaver-starfield -root \\n\\"
fi
echo "Done."
