#!/bin/sh
# Builds build/Starfield.scr (64-bit Windows) with the MinGW-w64 cross
# compiler, on macOS (brew install mingw-w64), Linux, or in MSYS2.
# Unsigned: there is no paid code-signing certificate.
set -eu
cd "$(dirname "$0")/.."

VERSION=$(cat VERSION)
NUM=$(echo "$VERSION" | tr . ,),0
CC=${CC:-x86_64-w64-mingw32-gcc}
WINDRES=${WINDRES:-x86_64-w64-mingw32-windres}
mkdir -p build/win

$WINDRES -DVERSION_STR="\\\"$VERSION\\\"" -DVERSION_NUM="$NUM" -I windows windows/starfield.rc -O coff -o build/win/starfield.res
$CC -std=c99 -O2 -Wall -Wextra -mwindows -DVERSION_STR="\"$VERSION\"" -Icore -Iwindows \
  core/starfield.c core/canvas.c windows/starfield_win.c build/win/starfield.res \
  -o build/Starfield.scr -static -lcomctl32 -lcomdlg32 -lgdi32 -luser32 -ladvapi32 -lsecur32 -lnetapi32 -lm

echo "Built build/Starfield.scr ($VERSION)"
