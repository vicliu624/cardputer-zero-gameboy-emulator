# 09. Traceability Matrix

This matrix traces current decisions and implemented contracts to their owning
spec files and code evidence. It intentionally does not reference deleted
historical guides as an authority.

## Decision To Spec

| Decision / Contract | Owning spec | Implementation evidence |
|---|---|---|
| Authoritative spec entry moved to `docs/SPEC.md` | `docs/SPEC.md`, `00-spec-index.md` | `docs/SPEC.md` |
| Visual mockups are style references, not geometry requirements | `docs/SPEC.md`, `01-product-boundaries.md`, `03-display-render-theme-ui.md` | `design/`, profile-specific renderer layouts |
| Cardputer Zero ROM Browser stays compact, with no last-played time or wide preview/detail layout | `01-product-boundaries.md`, `03-display-render-theme-ui.md`, `04-input-storage-config.md` | `src/render/renderer.cpp`, `src/storage/rom_scanner.cpp` |
| Cardputer Zero uses a 320x170 canvas; TDVP K230 uses a 410x189 canvas and DRM/KMS presents it at 3x as 1230x567 on the 1232x568 panel | `08-tdvp-k230.md`, `10-tdvp-k230-large-screen-ui.md` | `src/render/layout.hpp`, `src/main.cpp`, `src/platform/sdl_platform.cpp`, `src/platform/tdvp_k230_drm.cpp`, `tests/renderer_layout_smoke.cpp` |
| The native GBA source is 240x160; it is at `x=40,y=0` on Cardputer Zero and `x=85,y=3`, physically 720x480, on TDVP K230 | `03-display-render-theme-ui.md`, `10-tdvp-k230-large-screen-ui.md` | `src/render/layout.hpp`, `src/render/renderer.cpp`, `tests/renderer_layout_smoke.cpp` |
| Left status shows BAT, FAST ratio, and save slot, not FPS | `03-display-render-theme-ui.md` | `src/render/renderer.cpp` |
| Right hints are centered A/B/L/R labels | `03-display-render-theme-ui.md` | `src/render/renderer.cpp` |
| Bottom bar uses five physical-key-aligned slots | `03-display-render-theme-ui.md` | `src/render/renderer.cpp` |
| Playing bottom labels are `MENU | SAVE | LOAD | FAST | CHEATS` | `03-display-render-theme-ui.md`, `04-input-storage-config.md` | `src/app/command_bar.cpp` |
| App shortcuts are `4/5/6/7/8`; old `Esc/1/2/F/C` app shortcuts are removed | `04-input-storage-config.md` | `src/input/input_mapper.cpp` |
| ROM scanner uses real `.gba` filenames from `rom/`, `roms/`, and user ROM dir | `04-input-storage-config.md` | `src/storage/rom_scanner.cpp` |
| SRAM and state paths are user-data paths, not package-owned paths | `04-input-storage-config.md`, `07-build-package-release.md` | `src/storage/paths.cpp`, `src/app/app.cpp` |
| Active audio playback uses SDL queued audio with bounded prebuffer | `05-core-audio-timing.md` | `src/platform/sdl_audio.cpp`, `src/main.cpp`, `tests/audio_core_smoke.cpp` |
| GBA timing remains native approximately 59.7275Hz, not hard-locked to 60.000 FPS | `05-core-audio-timing.md`, `06-cli-errors-logging-docs.md` | `src/main.cpp` |
| APPLaunch desktop display name is `GBE` | `docs/SPEC.md`, `07-build-package-release.md` | `desktop/cardputer-zero-gba.desktop`, `tests/packaging_contract_smoke.cpp` |
| APPLaunch desktop path is `/usr/share/APPLaunch/applications/cardputer-zero-gba.desktop` | `docs/SPEC.md`, `07-build-package-release.md` | `CMakeLists.txt`, `README.md`, package smoke test |
| APPLaunch icon path is `/usr/share/APPLaunch/share/images/cardputer-zero-gba.png` | `docs/SPEC.md`, `07-build-package-release.md` | `CMakeLists.txt`, `packaging/cardputer-zero-gba.png` |
| Debian package is built through CPack and Docker Compose for arm64 | `07-build-package-release.md` | `docker-compose.yml`, `packaging/docker/arm64/package-arm64.sh` |
| TDVP K230 package is an ABI-gated riscv64 opkg payload built from the exact Buildroot SDK | `07-build-package-release.md`, `08-tdvp-k230.md` | `embedded-opkg-feed/packages/tdvp-cardputer-zero-gba/build.sh` |
| Project license is MIT; bundled mGBA and SDL keep their own licenses | `07-build-package-release.md`, `06-cli-errors-logging-docs.md` | `LICENSE`, `packaging/debian/copyright` |

## Current Gap Watchlist

| Gap | Spec owner | Notes |
|---|---|---|
| Cheat UI can read/toggle `.cht` entries, but core-side cheat application still needs final verification | `04-input-storage-config.md`, `05-core-audio-timing.md` | `MgbaCore::load_cheats` and `set_cheat_enabled` are the boundary to finish |
| Cheat enabled state persistence is not yet a proven contract | `04-input-storage-config.md` | Save a per-ROM cheat state file before claiming persistence |
| Config file read/write is specified but current implementation mostly uses defaults/runtime paths | `04-input-storage-config.md`, `06-cli-errors-logging-docs.md` | Keep CLI `--config` marked as reserved until implemented |
| Future ROM metadata must remain constrained by the selected device profile | `01-product-boundaries.md`, `03-display-render-theme-ui.md`, `04-input-storage-config.md`, `10-tdvp-k230-large-screen-ui.md` | Cardputer Zero stays compact; K230 uses its documented larger layout |
