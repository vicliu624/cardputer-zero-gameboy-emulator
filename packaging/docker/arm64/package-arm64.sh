#!/usr/bin/env bash
set -euo pipefail

source_dir=${CZ_GBA_SOURCE_DIR:-/workspace}
work_dir=${CZ_GBA_WORK_DIR:-/tmp/cardputer-zero-gba-arm64-build}
build_dir=${CZ_GBA_BUILD_DIR:-build-arm64}
out_dir=${CZ_GBA_OUT_DIR:-/workspace/out}
jobs=${CZ_GBA_BUILD_JOBS:-2}
toolchain_file=${CZ_GBA_TOOLCHAIN_FILE:-$source_dir/packaging/docker/arm64/aarch64-linux-gnu.cmake}

package_name=cardputer-zero-gba_0.1.0_arm64.deb

export PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig
export PKG_CONFIG_SYSROOT_DIR=/

if ! command -v aarch64-linux-gnu-g++ >/dev/null 2>&1 \
    || ! command -v cmake >/dev/null 2>&1 \
    || ! command -v ninja >/dev/null 2>&1; then
    apt-get update
    apt-get install -y --no-install-recommends \
        ca-certificates \
        cmake \
        dpkg-dev \
        file \
        g++-aarch64-linux-gnu \
        gcc-aarch64-linux-gnu \
        libc6-dev-arm64-cross \
        ninja-build \
        pkg-config
    rm -rf /var/lib/apt/lists/*
fi

rm -rf "$work_dir"
mkdir -p "$work_dir" "$out_dir"

tar -C "$source_dir" \
    --exclude=build \
    --exclude=build-msvc \
    --exclude=build-wsl-deb \
    --exclude=build-arm64 \
    --exclude=out \
    --exclude=rom \
    --exclude=.git \
    -cf - . | tar -C "$work_dir" -xf -

find "$work_dir" -type d -exec chmod 0755 {} +
find "$work_dir" -type f -exec chmod 0644 {} +
chmod 0755 \
    "$work_dir/packaging/cardputer-zero-gba" \
    "$work_dir/packaging/cardputer-zero-gba-applaunch" \
    "$work_dir/tools/cardputer_zero_gba_deb_package_smoke.sh"

cd "$work_dir"

cmake -S . -B "$build_dir" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$toolchain_file" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_FLAGS_RELEASE="-O1 -DNDEBUG" \
    -DCMAKE_CXX_FLAGS_RELEASE="-O2 -DNDEBUG" \
    -DCZ_GBA_ENABLE_DEB_PACKAGE:BOOL=ON \
    -DCZ_GBA_PACKAGE_ARCHITECTURE:STRING=arm64 \
    -DCZ_GBA_BUNDLE_SDL2:BOOL=ON \
    -DCZ_GBA_FETCH_SDL2:BOOL=OFF \
    -DSDL_SHARED:BOOL=ON \
    -DSDL_STATIC:BOOL=OFF \
    -DSDL_TEST:BOOL=OFF \
    -DSDL_TESTS:BOOL=OFF \
    -DSDL2_DISABLE_INSTALL:BOOL=ON \
    -DSDL_ALSA:BOOL=OFF \
    -DSDL_PULSEAUDIO:BOOL=OFF \
    -DSDL_PIPEWIRE:BOOL=OFF \
    -DSDL_JACK:BOOL=OFF \
    -DSDL_SNDIO:BOOL=OFF \
    -DSDL_OSS:BOOL=OFF \
    -DSDL_LIBSAMPLERATE:BOOL=OFF \
    -DSDL_X11:BOOL=OFF \
    -DSDL_WAYLAND:BOOL=OFF \
    -DSDL_KMSDRM:BOOL=OFF \
    -DSDL_OPENGL:BOOL=OFF \
    -DSDL_OPENGLES:BOOL=OFF \
    -DSDL_VULKAN:BOOL=OFF \
    -DSDL_DBUS:BOOL=OFF \
    -DSDL_IBUS:BOOL=OFF \
    -DSDL_LIBUDEV:BOOL=OFF \
    -DSDL_HIDAPI:BOOL=OFF \
    -DSDL_JOYSTICK:BOOL=OFF \
    -DSDL_HAPTIC:BOOL=OFF \
    -DSDL_SENSOR:BOOL=OFF \
    -DSDL_POWER:BOOL=OFF

grep -E 'CZ_GBA_ENABLE_DEB_PACKAGE|CZ_GBA_PACKAGE_ARCHITECTURE|CZ_GBA_BUNDLE_SDL2|CZ_GBA_FETCH_SDL2' \
    "$build_dir/CMakeCache.txt"
grep -E 'CPACK_PACKAGE_FILE_NAME|CPACK_DEBIAN_PACKAGE_ARCHITECTURE|CPACK_PACKAGING_INSTALL_PREFIX|CPACK_CMAKE_GENERATOR' \
    "$build_dir/CardputerZeroGbaCPackConfig.cmake"

cmake --build "$build_dir" --target cardputer-zero-gba-deb -j "$jobs"
file "$build_dir/cardputer-zero-gba"
bash tools/cardputer_zero_gba_deb_package_smoke.sh "$build_dir/$package_name"

cp "$build_dir/$package_name" "$out_dir/$package_name"
ls -lh "$out_dir/$package_name"
