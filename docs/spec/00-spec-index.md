# 00. Spec Index

## Purpose

The spec set splits requirements into several independent dimensions so UI,
architecture, packaging, emulator integration, timing, audio, and legal
constraints do not collapse into one vague checklist.

The specs answer:

- whether a concept belongs in this project;
- which boundary owns it;
- which modules may know about it;
- which behaviors are required, forbidden, or deferred;
- how the behavior is verified.

## Spec Dimensions

Product and boundary decisions:

- `01-product-boundaries.md`

Architecture and runtime flow:

- `02-architecture-runtime.md`

Display, rendering, theme, and UI:

- `03-display-render-theme-ui.md`

Input, storage, ROMs, saves, config, and cheats:

- `04-input-storage-config.md`

Core adapter, video conversion, audio playback, and timing:

- `05-core-audio-timing.md`

CLI, errors, logging, README, and legal notices:

- `06-cli-errors-logging-docs.md`

Build, install, Debian package, desktop file, Docker Compose, and release:

- `07-build-package-release.md`

Traceability:

- `09-traceability-matrix.md`

## Arbitration Rules

1. If a visual reference conflicts with fixed coordinates, fixed coordinates
   win.
2. If implementation convenience conflicts with module boundaries, module
   boundaries win.
3. If an SDL convenience API conflicts with the internal canvas model, the
   internal canvas model wins.
4. If a 60 Hz display backend conflicts with native GBA cadence, native GBA
   cadence wins.
5. If a Debian packaging shortcut writes into user home, user-data boundaries
   win.
6. If a visual mockup requires a wider or richer ROM Browser than 320x170 can
   support, the compact ROM Browser wins.
7. If shortcut docs conflict with the current input contract, use
   `04-input-storage-config.md` and `InputMapper`'s `4/5/6/7/8` mapping.
8. If audio notes conflict with the current SDL queued-audio playback contract,
   use `05-core-audio-timing.md`.

## Spec Change Rules

Classify every new requirement before changing code:

- Supplement: fills in details without changing concepts or boundaries.
- Clarification: makes an existing boundary more explicit.
- Local correction: changes one module's behavior and requires adjacent specs
  to be checked.
- Structural change: changes product identity, core boundaries, display model,
  input model, packaging model, or audio/timing model and requires reviewing the
  whole spec set.

Local discussion must not silently rewrite the system-level spec.
