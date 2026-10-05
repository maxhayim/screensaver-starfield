# Starfield for XScreenSaver. Needs a C compiler and the X11 headers:
#   Debian/Ubuntu: sudo apt install build-essential libx11-dev libxext-dev libxft-dev
#   Fedora:        sudo dnf install gcc make libX11-devel libXext-devel libXft-devel
#   Arch:          sudo pacman -S base-devel libx11 libxext libxft
# Then: make && sudo make install

VERSION := $(shell cat ../VERSION)
CC ?= cc
CFLAGS ?= -O2
PREFIX ?= /usr
# Where XScreenSaver looks for hacks and their settings.
HACKDIR ?= $(shell for d in $(PREFIX)/libexec/xscreensaver $(PREFIX)/lib/xscreensaver $(PREFIX)/lib64/xscreensaver; do [ -d $$d ] && echo $$d && break; done)
CONFIGDIR ?= $(PREFIX)/share/xscreensaver/config

SRC = ../core/starfield.c ../core/canvas.c starfield_x11.c

starfield: $(SRC) ../core/*.h
	$(CC) -std=c99 $(CFLAGS) -Wall -Wextra -DVERSION_STR='"$(VERSION)"' -I../core $(shell pkg-config --cflags xft 2>/dev/null || echo -I/usr/include/freetype2) $(SRC) -o $@ -lXft -lX11 -lXext -lm $(LDFLAGS)

install: starfield
	@test -n "$(HACKDIR)" || { echo "XScreenSaver's hack directory wasn't found. Is xscreensaver installed? Or set HACKDIR=..."; exit 1; }
	install -D -m 755 starfield $(DESTDIR)$(HACKDIR)/starfield
	install -D -m 644 starfield.xml $(DESTDIR)$(CONFIGDIR)/starfield.xml

uninstall:
	rm -f $(DESTDIR)$(HACKDIR)/starfield $(DESTDIR)$(CONFIGDIR)/starfield.xml

clean:
	rm -f starfield

.PHONY: install uninstall clean
