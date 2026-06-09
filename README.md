# Cardputer Zero GBA

Cardputer Zero GBA is a Game Boy Advance emulator frontend for the Cardputer
Zero portable Linux device. It uses libmgba for emulation, SDL2 for the Linux
runtime adapter, and a fixed 320x170 XRGB8888 internal canvas for the framed
pixel UI.

The authoritative project specification is [docs/SPEC.md](docs/SPEC.md), with
detailed specs under [docs/spec/](docs/spec/).

## Run

```sh
cardputer-zero-gba
cardputer-zero-gba --kiosk
cardputer-zero-gba --rom ./roms/game.gba
```

The SDL window is fixed at 320x170 and borderless. `--kiosk` uses the same
fixed 320x170 undecorated presentation surface, which is the default APPLaunch
path on the Cardputer Zero. `--fullscreen` is reserved for explicit manual
fullscreen testing. `--scale 1` is accepted only as a compatibility option.

The ROM browser scans local `rom/`, local `roms/`, and the user data ROM
directory:

```text
~/.local/share/cardputer-zero-gba/roms/
```

Saves, states, cheats, themes, screenshots, config, and cache are created in
the current user's data/config/cache directories at runtime. Debian packaging
does not install or remove user ROMs or saves.

While playing, the five physical keys under the screen map left-to-right to
Menu, Save, Load, Fast, and Cheats. On a keyboard those are `4`, `5`,
`6`, `7`, and `8`.

## Cardputer Zero Package Layout

The Debian package installs the executable and APPLaunch metadata under `/usr`:

```text
/usr/bin/cardputer-zero-gba
/usr/lib/cardputer-zero-gba/cardputer-zero-gba
/usr/lib/cardputer-zero-gba/cardputer-zero-gba-applaunch
/usr/share/APPLaunch/applications/cardputer-zero-gba.desktop
/usr/share/APPLaunch/share/images/cardputer-zero-gba.png
/usr/share/icons/hicolor/64x64/apps/cardputer-zero-gba.png
/usr/share/icons/hicolor/128x128/apps/cardputer-zero-gba.png
/usr/share/cardputer-zero-gba/themes/minimal/theme.json
/usr/share/cardputer-zero-gba/themes/minimal/bezel.png
/usr/share/cardputer-zero-gba/fonts/font_5x7.txt
```

The APPLaunch desktop entry intentionally lives in
`/usr/share/APPLaunch/applications`, and its icon uses the relative
`share/images/cardputer-zero-gba.png` path expected by the Cardputer Zero
shell.

Runtime package dependencies include `libsdl2-ttf-2.0-0` plus a CJK-capable
font package (`fonts-noto-cjk` or `fonts-droid-fallback`) so the ROM browser
can render real UTF-8 ROM filenames instead of placeholder names.

## Build

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

To generate a `.deb` with CPack:

```sh
cmake -S . -B build-deb -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCZ_GBA_ENABLE_DEB_PACKAGE=ON \
  -DCZ_GBA_PACKAGE_ARCHITECTURE=arm64
cmake --build build-deb --target cardputer-zero-gba-deb
```

## Reproducible ARM64 Debian Package

Use Docker Compose on the development machine to build the Cardputer Zero
`arm64` package in a Debian bookworm container with the `aarch64-linux-gnu`
cross toolchain:

```sh
docker compose run --rm package-arm64
```

The Compose job writes the verified package to:

```text
out/cardputer-zero-gba_0.1.0_arm64.deb
```

The container copies the source tree to an internal Linux filesystem before
building, normalizes file permissions, excludes local ROMs, and runs
`tools/cardputer_zero_gba_deb_package_smoke.sh` against the generated package.
This keeps the package contents independent of Windows bind-mount permissions
and checks the Cardputer Zero APPLaunch paths:

```text
/usr/share/APPLaunch/applications/cardputer-zero-gba.desktop
/usr/share/APPLaunch/share/images/cardputer-zero-gba.png
```

The Compose entry uses the local `debian:bookworm` image and installs the
cross-build dependencies inside the container when they are missing. A matching
Dockerfile is kept under `packaging/docker/arm64/` for prebuilt CI images or
future caching.
