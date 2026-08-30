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
must not override the documented geometry of the selected device profile. The
K230's physical shell chrome is not part of this application UI.

## Current Product Baseline

- App identity: `cardputer-zero-gba`
- APPLaunch display name: `GBE`
- License: MIT for this project
- Target devices: Cardputer Zero (arm64 Linux) and TDVP K230 (`tdvp-k230-r1`, RISC-V 64 LP64D)
- Emulator core: libmgba behind the project-owned `GbaCore` / `MgbaCore`
  boundary
- Runtime adapter: SDL2
- Internal canvas: Cardputer Zero `320x170` XRGB8888; TDVP K230 `410x189`
  XRGB8888
- GBA viewport: native 240x160; Cardputer Zero at `x=40,y=0`, TDVP K230 at
  `x=85,y=3`
- Window policy: Cardputer Zero uses a fixed 320x170 borderless surface;
  TDVP K230 is a labwc Wayland client. SDL owns only its Wayland window and
  input events; the application submits CPU-owned XRGB `wl_shm` buffers and
  integer-scales the `410x189` canvas to `1230x567` at `(1,0)`, while labwc
  retains the DRM/KMS CRTC
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

The following compact geometry applies only to Cardputer Zero.

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

For the TDVP K230's distinct large-screen geometry, expanded information
rails, and F1--F5 command bar, see `docs/spec/10-tdvp-k230-large-screen-ui.md`.

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

The TDVP K230 function row supplements, rather than replaces, these actions:

```text
F1 / F2 / F3 / F4 / F5 -> MENU / SAVE / LOAD / FAST / CHEATS
```

## Audio Contract

Audio is the realtime master clock. mGBA and `App` are owned by one emulation
worker; the SDL/Wayland event loop is a separate presentation consumer:

```text
libmgba audio samples
  -> MgbaCore / emulation worker
  -> whole-batch PcmRing write (SPSC)
  -> SDL audio callback
  -> PulseAudio (primary) / ALSA (runtime fallback)
```

The ring is preallocated and accepts every interleaved S16 stereo batch in
full or rejects it with a visible metric; partial PCM writes are forbidden. SDL
starts paused, then begins only after a complete prebuffer. The callback fills
only a true shortage with silence and increments an underrun counter. The UI
notices an underrun on its next iteration and pauses the callback. The sole
producer, the emulation worker, then discards the old ring epoch and creates a
new complete prebuffer. It does not wait for a once-per-second telemetry tick
to recover.

The same state transition is used for a playback-device removal or a failed
PulseAudio session: the UI closes/reopens SDL outside the callback, the worker
applies the obtained sample rate only at its own frame boundary, stale PCM and
video timestamps are discarded, and a complete prebuffer is rebuilt. If no
sink can be reopened, the worker switches to explicit muted pacing instead of
filling an unconsumed PCM ring; it retries at a bounded cadence until an audio
device returns. Telemetry includes queue low/current/high watermarks, recovery
and reopen counts, plus callback-jitter P50/P95/P99 buckets. The callback
itself remains allocation-, logging-, lock-, and mGBA-free.

Render snapshots have audio-frame presentation timestamps. The UI chooses the
newest snapshot no later than the callback playback position plus one callback
quantum. A late compositor drops a video submission or reuses the last frame;
it never stalls mGBA or the PCM producer. FAST mode drains generated audio
instead of compressing it into normal-speed playback.

`--present-delay-ms <0..1000>` is a deliberate UI/presentation stress hook.
It may be used at 20 ms and 50 ms on K230 to demonstrate that compositor delay
causes latest-wins video loss rather than PCM underflow; it is not a normal
runtime tuning option.

## ROM Browser Contract

Cardputer Zero's ROM browser remains compact for 320x170. It should show real
ROM filenames from `rom/`, `roms/`, and
`~/.local/share/cardputer-zero-gba/roms/`, but it must not require last-played
time, play duration, large preview art, or a wide detail layout. TDVP K230 uses
its own `410x189` library layout with six visible rows and expanded control
rails; see `docs/spec/10-tdvp-k230-large-screen-ui.md`. The richer design
images remain style references only.

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
- [08-tdvp-k230.md](spec/08-tdvp-k230.md): TDVP K230 presentation, input, ABI,
  and opkg delivery boundary
- [10-tdvp-k230-large-screen-ui.md](spec/10-tdvp-k230-large-screen-ui.md):
  TDVP K230 large-screen application layout
- [09-traceability-matrix.md](spec/09-traceability-matrix.md): decision and
  implementation traceability
