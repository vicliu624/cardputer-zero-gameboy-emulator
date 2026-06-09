# Cardputer Zero GBA Emulator Specification

This file is the authoritative specification entry for the repository. The
detailed specifications live in `docs/spec/`, and implementation work should be
checked against those files before code, packaging, or UI behavior changes.

## Authority

When documents or implementation details disagree, resolve them in this order:

1. `docs/SPEC.md`
2. The detailed specs in `docs/spec/`
3. Current implementation evidence in `src/`, `desktop/`, `packaging/`, and
   tests
4. Visual references under `design/`

The images in `design/` are visual inspiration only. They may guide the dark
pixel-device look, list styling, icons, menu panels, and dialog tone, but they
must not override the fixed 320x170 geometry or require a richer ROM-browser
layout that the physical screen cannot support.

## Current Product Baseline

- App identity: `cardputer-zero-gba`
- APPLaunch display name: `GBE`
- License: MIT for this project
- Target device: Cardputer Zero, arm64 Linux
- Emulator core: libmgba behind the project-owned `GbaCore` / `MgbaCore`
  boundary
- Runtime adapter: SDL2
- Internal canvas: 320x170 XRGB8888
- GBA viewport: 240x160 at `x=40,y=0`
- Window policy: fixed 320x170, borderless, no title bar, no menu bar, no
  status bar
- Package delivery: Debian `.deb`, built reproducibly through Docker Compose
- ROM policy: user-provided `.gba` files only; no bundled ROMs or BIOS

## APPLaunch Package Contract

Cardputer Zero shell discovery is a packaging contract, not an emulator runtime
concept. The primary scanned desktop file must be installed to:

```text
/usr/share/APPLaunch/applications/cardputer-zero-gba.desktop
```

That desktop file must keep:

```text
Name=GBE
Exec=/usr/lib/cardputer-zero-gba/cardputer-zero-gba-applaunch
Icon=share/images/cardputer-zero-gba.png
X-Zero-AppId=io.github.vicliu624.cardputer-zero-gba
X-Zero-Display=wayland
```

The referenced APPLaunch icon must be installed to:

```text
/usr/share/APPLaunch/share/images/cardputer-zero-gba.png
```

`/usr/bin/cardputer-zero-gba` remains a CLI convenience and must not be treated
as the Cardputer Zero shell discovery surface.

## Runtime UI Contract

The playing screen uses the Framed Pixel layout:

```text
left panel:      x=0   y=0 w=40  h=160
GBA viewport:    x=40  y=0 w=240 h=160
right panel:     x=280 y=0 w=40  h=160
bottom command:  x=0   y=160 w=320 h=10
```

The bottom command bar is aligned to the five physical keys below the screen and
uses these slots:

```text
{0, 79}, {79, 54}, {133, 54}, {187, 54}, {241, 79}
```

Playing mode labels are:

```text
MENU | SAVE | LOAD | FAST | CHEATS
```

The visible labels do not show numeric prefixes because the physical device
already aligns the five keys below the screen.

## Input Contract

GBA controls:

```text
W / Up       -> GBA Up
S / Down     -> GBA Down
A / Left     -> GBA Left
D / Right    -> GBA Right
J            -> GBA A
K            -> GBA B
U            -> GBA L
I            -> GBA R
Enter        -> GBA Start / Confirm
Space        -> GBA Select
```

App controls while playing:

```text
4 -> MENU
5 -> SAVE
6 -> LOAD
7 -> FAST
8 -> CHEATS
```

The old app shortcuts `Esc`, `1`, `2`, `F`, and `C` are not part of the current
app-control contract.

## Audio Contract

The active SDL audio path uses SDL's queued-audio device. This is a deliberate
rollback from the callback ring-buffer experiment because the device showed
audible underruns on the Cardputer Zero target:

```text
libmgba audio samples
  -> MgbaCore
  -> SdlAudio::write
  -> SDL_QueueAudio
  -> SDL audio device
```

The main loop continues to read pending samples from `MgbaCore` once per frame
and writes them into `SdlAudio`. `SdlAudio` must prebuffer briefly before
starting playback and must cap the queued audio length so the game never builds
unbounded audio latency. Audio synchronization takes priority over displaying a
synthetic FPS number. FAST mode must not feed twice as much audio into the same
wall-clock interval; in the current implementation generated FAST audio is
drained from mGBA but not queued to the device.

## ROM Browser Contract

The ROM browser remains compact for 320x170. It should show real ROM filenames
from `rom/`, `roms/`, and `~/.local/share/cardputer-zero-gba/roms/`, but it must
not require last-played time, play duration, large preview art, or a wide detail
layout. The richer design images remain style references only.

## Detailed Specs

- [00-spec-index.md](spec/00-spec-index.md): specification dimensions and
  arbitration rules
- [01-product-boundaries.md](spec/01-product-boundaries.md): product identity,
  user experience, and scope boundaries
- [02-architecture-runtime.md](spec/02-architecture-runtime.md): module
  boundaries, state machine, runtime loop, and dependency direction
- [03-display-render-theme-ui.md](spec/03-display-render-theme-ui.md): canvas,
  layout, rendering order, theme, HUD, and menu visuals
- [04-input-storage-config.md](spec/04-input-storage-config.md): input,
  ROM browser, saves, config, and cheat data
- [05-core-audio-timing.md](spec/05-core-audio-timing.md): core adapter, video
  format, audio, timing, and performance rules
- [06-cli-errors-logging-docs.md](spec/06-cli-errors-logging-docs.md): CLI,
  error handling, logging, README, and legal notices
- [07-build-package-release.md](spec/07-build-package-release.md): CMake,
  install paths, Debian package, desktop file, Docker Compose, and release
  artifacts
- [09-traceability-matrix.md](spec/09-traceability-matrix.md): decision and
  implementation traceability
