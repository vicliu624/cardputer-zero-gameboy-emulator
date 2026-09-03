# TDVP K230 Large-Screen Application UI Contract

## Status

This document supersedes the former TDVP UI assumption that the K230 only
physically enlarged the Cardputer Zero's 320x170 application canvas. It does
not change the K230 ABI, package identity, DRM/KMS ownership, or mGBA's GBA
video format.

## The distinctions that must remain true

| Concern | Contract |
| --- | --- |
| Emulator core | mGBA continues to produce an unmodified 240x160 GBA frame. Game coordinates and input semantics do not change. |
| Application layout | Cardputer Zero keeps its 320x170 layout. TDVP K230 gets its own 410x189 layout, with larger information and command regions. |
| Physical presentation | The user-visible UI coordinate system is 1232x568 landscape. Labwc owns the verified RM69A10 connector's 568x1232 native DRM scanout and panel transform. The application owns a standard native Wayland `xdg_toplevel`, its client event queue, and `wl_seat` keyboard. Static XRGB `wl_shm` chrome is committed on that root surface only when it changes; the game rectangle is a desynchronised child `wl_subsurface` at `(256,9)`, copied at 3x nearest-neighbour scaling to 720x480 with three buffers. |
| Device system UI | Firmware shell chrome is owned by the device shell, not this application, and is not reproduced by the emulator. |
| Release boundary | The package remains `tdvp-cardputer-zero-gba` for `tdvp-k230-br2025.02.1-glibc2.33-rv64-lp64d-k6.6.36-r1`. |

## TDVP K230 logical layout

The canvas is `410x189`, so all chrome has a clean 3x physical scale while the
game remains a native-pixel 240x160 source rectangle.

| Region | Logical rectangle | Physical result at 3x |
| --- | --- | --- |
| Left information rail | `(3, 3) 78x160` | 234x480 |
| GBA viewport | `(85, 3) 240x160` | 720x480 |
| Right control rail | `(329, 3) 78x160` | 234x480 |
| Command bar | `(3, 166) 404x20` | 1212x60 |

The information rail shows battery, save-slot, and speed state. The control
rail presents the physical keyboard's game controls. The command bar names
the top-row F1--F5 actions visible on the device: menu, save, load, fast
forward, and cheats. Existing numeric action mappings remain compatibility
aliases; the displayed primary labels are F1--F5.

## Non-goals

- No fractional scaling or smoothing.
- No fbdev fallback as the device implementation path.
- No scaling, cropping, or re-timing of the native GBA frame.
- No modification of device system settings, CRTC ownership, or display
  configuration by the application.
