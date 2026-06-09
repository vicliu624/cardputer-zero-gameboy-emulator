#!/usr/bin/env bash
set -euo pipefail

deb=${1:?usage: tools/cardputer_zero_gba_deb_package_smoke.sh path/to/cardputer-zero-gba_*.deb}

contents=$(mktemp)
extract=$(mktemp -d)
trap 'rm -f "$contents"; rm -rf "$extract"' EXIT

dpkg-deb -c "$deb" | tee "$contents"

if awk '
    substr($1, 1, 10) == "-rwxrwxrwx" || substr($1, 1, 10) == "drwxrwxrwx" {
        print "bad package permission: " $0
        bad = 1
    }
    END { exit bad ? 1 : 0 }
' "$contents"; then
    :
else
    printf 'Refusing package with unsafe package permissions.\n' >&2
    exit 1
fi

grep -q './usr/bin/cardputer-zero-gba$' "$contents"
grep -q './usr/lib/cardputer-zero-gba/cardputer-zero-gba$' "$contents"
grep -q './usr/lib/cardputer-zero-gba/cardputer-zero-gba-applaunch$' "$contents"
grep -q './usr/share/APPLaunch/applications/cardputer-zero-gba.desktop$' "$contents"
grep -q './usr/share/APPLaunch/share/images/cardputer-zero-gba.png$' "$contents"
grep -q './usr/share/icons/hicolor/64x64/apps/cardputer-zero-gba.png$' "$contents"
grep -q './usr/share/icons/hicolor/128x128/apps/cardputer-zero-gba.png$' "$contents"
grep -q './usr/share/cardputer-zero-gba/themes/minimal/theme.json$' "$contents"

dpkg-deb -x "$deb" "$extract"
desktop="$extract/usr/share/APPLaunch/applications/cardputer-zero-gba.desktop"
launcher="$extract/usr/lib/cardputer-zero-gba/cardputer-zero-gba-applaunch"

test -x "$extract/usr/bin/cardputer-zero-gba"
test -x "$extract/usr/lib/cardputer-zero-gba/cardputer-zero-gba"
test -x "$launcher"
grep -q '^Name=GBE$' "$desktop"
grep -q '^Exec=/usr/lib/cardputer-zero-gba/cardputer-zero-gba-applaunch$' "$desktop"
grep -q '^Icon=share/images/cardputer-zero-gba.png$' "$desktop"
grep -q '^X-Zero-AppId=io.github.vicliu624.cardputer-zero-gba$' "$desktop"
grep -q '^X-Zero-Display=wayland$' "$desktop"
grep -q 'CARDPUTER_ZERO_GBA_DISPLAY_BACKEND' "$launcher"
grep -q 'SDL_VIDEODRIVER:=wayland' "$launcher"
grep -q 'SDL_VIDEO_WAYLAND_WMCLASS=io.github.vicliu624.cardputer-zero-gba' "$launcher"
grep -q -- '--kiosk' "$launcher"
dpkg-deb -f "$deb" Depends | grep -q 'libsdl2-ttf-2.0-0'
dpkg-deb -f "$deb" Depends | grep -Eq 'fonts-noto-cjk|fonts-droid-fallback'

dpkg-deb -f "$deb" Package Version Architecture Depends
