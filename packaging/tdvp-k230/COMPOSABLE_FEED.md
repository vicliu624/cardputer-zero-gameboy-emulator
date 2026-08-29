# TDVP K230 composable feed contract

`tdvp-gba` is a leaf application in the ABI-matched TDVP K230 opkg feed. It
must never be used as a vehicle for copying SDL2, SDL2_ttf, libmGBA, or another
general runtime into `/opt/tdvp-gba`.

For a TDVP release, configure this repository with:

```sh
-DCZ_GBA_TDVP_COMPOSABLE_FEED=ON
-DCZ_GBA_SDL2_ROOT=<release staging prefix owned by the sdl2 package>
-DCZ_GBA_MGBA_ROOT=<release staging prefix owned by the libmgba package>
```

The mode fails closed when either prefix is missing and forces all of the
following off:

- SDL FetchContent;
- bundled SDL2 from `extern/SDL`;
- bundled/static mGBA from `extern/mgba`.

The feed recipe is responsible for declaring exact runtime dependencies:

```text
sdl2 (= <release version>)
sdl2-ttf (= <release version>)
libmgba (= <release version>)
```

The feed's ELF closure check supplies any remaining direct runtime dependencies
as exact versioned IPKs. A new general-purpose library must be introduced as
its own feed package before an application uses it; it must not be statically
bundled or borrowed implicitly from the base image.
