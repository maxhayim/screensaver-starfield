# Builds the XScreenSaver hack. Needs: libx11-dev libxext-dev libxft-dev
# (Debian/Ubuntu), libX11-devel libXext-devel libXft-devel (Fedora), or
# libx11 libxext libxft (Arch).
PREFIX ?= /usr/local
HACKDIR ?= $(PREFIX)/libexec/xscreensaver
CONFDIR ?= $(PREFIX)/share/xscreensaver/config
VERSION := $(shell cat ../VERSION)
ARCH ?= $(shell uname -m | sed 's/aarch64/arm64/')

CFLAGS ?= -O2
override CFLAGS += -std=c99 -Wall -Wextra -DVERSION_STR='"$(VERSION)"' -I../core $(shell pkg-config --cflags x11 xext xft)
override LDLIBS += $(shell pkg-config --libs x11 xext xft) -lm

SRC = ../core/starfield.c ../core/canvas.c starfield_x11.c
PKG = screensaver-starfield-$(VERSION)-linux-$(ARCH)

../build/screensaver-starfield: $(SRC) ../core/*.h
	mkdir -p ../build
	$(CC) $(CFLAGS) $(SRC) -o $@ $(LDFLAGS) $(LDLIBS)

install: ../build/screensaver-starfield
	install -D -m 755 ../build/screensaver-starfield $(DESTDIR)$(HACKDIR)/screensaver-starfield
	install -D -m 644 screensaver-starfield.xml $(DESTDIR)$(CONFDIR)/screensaver-starfield.xml

tarball: ../build/screensaver-starfield
	rm -rf ../build/$(PKG)
	mkdir -p ../build/$(PKG)
	cp ../build/screensaver-starfield screensaver-starfield.xml install.sh INSTALL.txt ../LICENSE ../build/$(PKG)/
	tar -C ../build -czf ../build/$(PKG).tar.gz $(PKG)

.PHONY: install tarball
