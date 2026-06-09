# 01. Product And Boundaries

## Product Identity

Project name:

```text
cardputer-zero-gba
```

APPLaunch display name:

```text
GBE
```

Positioning:

```text
A Cardputer Zero focused GBA emulator frontend using libmgba as the emulator
core, SDL2 as the runtime adapter, and a fixed 320x170 Framed Pixel UI.
```

The project is not a new GBA emulator core. CPU, PPU, APU, cartridge behavior,
save media behavior, RTC, DMA, timers, interrupts, and BIOS behavior are owned
by libmgba.

## User Experience

The user should open a handheld-style GBA app tailored to the 320x170 screen.

Required experience:

- GBA video is shown at native 240x160 pixels.
- The viewport is not scaled, cropped, stretched, or filtered by default.
- Left and right 40 px panels carry status, hints, and theme framing.
- The bottom 10 px band carries context command labels.
- The main playing screen stays quiet and does not cover the GBA viewport.
- Pause, settings, and cheat menus may intentionally overlay the game.
- PC SDL simulation and Cardputer Zero runtime both use the same fixed 320x170
  borderless, undecorated window.

Layout name:

```text
Framed Pixel
```

Meaning:

```text
The center 240x160 GBA viewport is preserved exactly. The side and bottom bands
belong to emulator UI, not to the GBA image.
```

## Required Project-Owned Work

The project owns:

- SDL2 platform adapter;
- libmgba adapter;
- 320x170 XRGB8888 internal canvas;
- Framed Pixel renderer;
- theme/bezel rendering;
- bottom command bar;
- side panels;
- ROM Browser;
- pause menu;
- settings menu;
- cheat menu;
- input mapping;
- SDL queued-audio output;
- SRAM path management;
- save-state path management;
- cheat file loading and enable/disable state;
- config file loading/writing where implemented;
- user-data directory management;
- Debian packaging;
- APPLaunch `.desktop` integration;
- icon and basic theme assets.

## Forbidden Project-Owned Work

The project must not implement:

- GBA CPU emulation;
- GBA PPU emulation;
- GBA APU emulation;
- GBA BIOS behavior emulation;
- cartridge mapper behavior;
- flash/SRAM/EEPROM low-level behavior;
- RTC low-level behavior;
- GBA interrupts, DMA, or timers;
- GBA link cable emulation;
- a native GBA debug engine.

## Legal And Content Boundaries

The project license is MIT, but bundled third-party projects keep their own
licenses.

The project must not:

- bundle commercial GBA ROMs;
- bundle a GBA BIOS;
- provide ROM downloads;
- provide links to pirated ROMs.

README and runtime messaging must tell users to provide legally obtained `.gba`
files. When no ROMs are found, the message should point to:

```text
~/.local/share/cardputer-zero-gba/roms
```

## Product Prohibitions

Do not:

- turn this into a RetroArch frontend;
- default to fullscreen stretching;
- scale the GBA image to 320x170;
- enable bilinear filtering by default;
- add online ROM download features;
- build a cheat editor in the first version;
- write input directly into the mGBA adapter;
- let UI include mGBA headers;
- scatter render coordinates outside the layout/renderer boundary;
- read PNGs every frame;
- scan ROM directories every frame;
- log every frame;
- bind to private `cardputer-zero-shell` APIs;
- bypass Wayland just because an internal canvas exists;
- force native GBA speed to 60.000 FPS;
- show realtime FPS as the playing-screen status number.

## Visual References

Images under `design/` are visual references, not behavior specs.

Allowed inspiration:

- dark pixel-device frame;
- low-noise side panels;
- white primary text;
- purple/green/blue accent usage;
- pixel borders, corner cuts, and thin separators;
- overlay dimming when menus are open;
- list and dialog tone.

Not allowed as hard requirements:

- changing the 320x170 internal canvas;
- changing the 240x160 viewport;
- changing the fixed left/right/bottom layout;
- requiring preview art, favorites, last-played time, play duration, category
  metadata, or a wide detail panel in the current ROM Browser;
- pixel-perfect reproduction of high-resolution mockups.

The current ROM Browser remains compact because the physical screen width cannot
support the richer mockup layout without hurting readability.

## Open Decisions

Current watch items:

- final verification of core-side cheat application;
- whether future ROM metadata can be added without widening the ROM Browser;
- whether future audio options need user-facing configuration after real-device
  testing stabilizes.
