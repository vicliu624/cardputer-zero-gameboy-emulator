# 07. Build, Package, Release

## Cardputer Zero APPLaunch Contract

`cardputer-zero-shell` scans APPLaunch entries, so the package must install
the primary desktop launcher here:

```text
/usr/share/APPLaunch/applications/cardputer-zero-gba.desktop
```

The APPLaunch launcher must be backed by a private runtime path:

```text
/usr/lib/cardputer-zero-gba/cardputer-zero-gba
/usr/lib/cardputer-zero-gba/cardputer-zero-gba-applaunch
```

The desktop file must use the same APPLaunch image convention as the
Cardputer Zero reference package:

```ini
Name=GBE
Exec=/usr/lib/cardputer-zero-gba/cardputer-zero-gba-applaunch
Icon=share/images/cardputer-zero-gba.png
X-Zero-AppId=io.github.vicliu624.cardputer-zero-gba
X-Zero-Display=wayland
```

The icon path is resolved relative to the APPLaunch data root and must exist:

```text
/usr/share/APPLaunch/share/images/cardputer-zero-gba.png
```

`/usr/share/applications/cardputer-zero-gba.desktop` is optional compatibility
metadata. It must not replace the APPLaunch path above.

## CMake

建议构建选项：

```cmake
option(CZ_GBA_FETCH_SDL2 "Fetch SDL2 if find_package cannot locate it" ON)
option(CZ_GBA_BUNDLE_SDL2 "Build bundled SDL2 from extern/SDL when needed" ON)
option(CZ_GBA_USE_SYSTEM_MGBA "Use system libmgba instead of bundled mGBA" OFF)
option(CZ_GBA_BUNDLE_MGBA "Build bundled mGBA from extern/mgba" ON)
option(CZ_GBA_ENABLE_DEB_PACKAGE "Enable CPack Debian package generation" OFF)
set(CZ_GBA_PACKAGE_ARCHITECTURE "" CACHE STRING "Debian package architecture override")
set(CZ_GBA_PRIVATE_LIBDIR "lib/cardputer-zero-gba" CACHE STRING "Private APPLaunch runtime directory")
```

第一版依赖：

```text
SDL2
libmgba
标准 C++ filesystem/config parsing helpers
```

当前打包策略优先使用 bundled mGBA 和 bundled SDL2 生成可部署 arm64 包，`debian/copyright` 必须声明项目 MIT、mGBA MPL-2.0、SDL Zlib。

## Debian Package Identity

最终必须能够被打包为标准 Debian package，并通过 `apt` / `dpkg` 安装到 Cardputer Zero 系统中。

包名固定：

```text
cardputer-zero-gba
```

目标不是只生成一个可执行文件，而是完整系统应用包，包含：

- 可执行程序。
- `.desktop` 启动入口。
- 图标。
- 默认主题。
- 默认配置模板或默认配置生成逻辑。
- 文档。
- 依赖声明。
- 用户数据目录说明。
- 卸载行为约定。

## Package Install Goals

必须支持：

```bash
sudo apt install ./cardputer-zero-gba_<version>_<arch>.deb
```

或：

```bash
sudo dpkg -i cardputer-zero-gba_<version>_<arch>.deb
sudo apt -f install
```

安装后必须能够执行：

```bash
cardputer-zero-gba
```

并且能够被 `cardputer-zero-shell` 通过标准 `.desktop` 文件扫描到。

不得要求用户手动复制二进制文件到 `/usr/bin`。

不得要求 `cardputer-zero-shell` 对本应用做特殊适配。

## Architecture Targets

Cardputer Zero 目标平台优先：

```text
arm64
```

开发机可选：

```text
amd64
```

可能产物：

```text
cardputer-zero-gba_0.1.0_arm64.deb
cardputer-zero-gba_0.1.0_amd64.deb
```

最小要求：

```text
arm64 deb package
```

## Install Paths

安装后文件必须放置：

```text
/usr/bin/cardputer-zero-gba

/usr/lib/cardputer-zero-gba/cardputer-zero-gba
/usr/lib/cardputer-zero-gba/cardputer-zero-gba-applaunch

/usr/share/APPLaunch/applications/cardputer-zero-gba.desktop
/usr/share/APPLaunch/share/images/cardputer-zero-gba.png

/usr/share/icons/hicolor/64x64/apps/cardputer-zero-gba.png
/usr/share/icons/hicolor/128x128/apps/cardputer-zero-gba.png

/usr/share/cardputer-zero-gba/
  themes/
    minimal/
      theme.json
      bezel.png
  fonts/
    font_5x7.bin or font_5x7.png
  assets/

/usr/share/doc/cardputer-zero-gba/
  README.md
  LICENSE
  changelog.Debian.gz
```

不得安装可执行程序到：

```text
/usr/local/bin
```

Debian package 管理文件必须安装到 `/usr` 下，不是 `/usr/local`。

## User Data Packaging Rule

Debian package 不应该直接创建某个具体用户的 home 目录内容。

打包时不得安装文件到：

```text
/home/pi/.local/share/cardputer-zero-gba/
/home/pi/.config/cardputer-zero-gba/
```

这些目录必须由程序首次运行时自动创建：

```text
~/.local/share/cardputer-zero-gba/
  roms/
  saves/
  states/
  cheats/
  screenshots/
  themes/

~/.config/cardputer-zero-gba/
  config.json

~/.cache/cardputer-zero-gba/
```

原因：

- Linux 系统可能不是 `pi` 用户。
- 应用应支持任意普通用户运行。
- Debian package 不应假设用户 home 路径。
- 卸载 package 时不应删除用户存档。

## Uninstall Rule

卸载应用：

```bash
sudo apt remove cardputer-zero-gba
```

必须删除系统文件：

```text
/usr/bin/cardputer-zero-gba
/usr/lib/cardputer-zero-gba/
/usr/share/APPLaunch/applications/cardputer-zero-gba.desktop
/usr/share/APPLaunch/share/images/cardputer-zero-gba.png
/usr/share/icons/...
/usr/share/cardputer-zero-gba/
```

但不得删除用户数据：

```text
~/.local/share/cardputer-zero-gba/roms/
~/.local/share/cardputer-zero-gba/saves/
~/.local/share/cardputer-zero-gba/states/
~/.local/share/cardputer-zero-gba/cheats/
~/.config/cardputer-zero-gba/config.json
```

即使执行：

```bash
sudo apt purge cardputer-zero-gba
```

也不应该主动删除普通用户 home 下的 ROM、存档、即时存档和配置。

未来如果提供清理工具，应单独提供：

```bash
cardputer-zero-gba --reset-user-data
```

并且必须要求用户确认。

## Desktop File

必须安装主入口：

```text
/usr/share/APPLaunch/applications/cardputer-zero-gba.desktop
```

内容：

```ini
[Desktop Entry]
Type=Application
Name=GBE
Comment=Game Boy Advance emulator for the Cardputer Zero
Exec=/usr/lib/cardputer-zero-gba/cardputer-zero-gba-applaunch
Icon=share/images/cardputer-zero-gba.png
Terminal=false
Categories=Game;Emulator;
StartupNotify=false
X-Zero-AppId=io.github.vicliu624.cardputer-zero-gba
X-Zero-Display=wayland
```

要求：

- `Type` 必须是 `Application`。
- `Name` 必须是 `GBE`。
- `Exec` 必须是 `/usr/lib/cardputer-zero-gba/cardputer-zero-gba-applaunch`。
- `Icon` 必须是 `share/images/cardputer-zero-gba.png`。
- `X-Zero-AppId` 必须是 `io.github.vicliu624.cardputer-zero-gba`。
- `X-Zero-Display` 必须是 `wayland`。
- `Terminal` 必须是 `false`。
- `Categories` 至少包含 `Game` 和 `Emulator`。

`/usr/share/applications/cardputer-zero-gba.desktop` 可作为普通桌面兼容入口存在，但不能替代 APPLaunch 主入口。

## Icons

至少提供：

```text
64x64
128x128
```

推荐路径：

```text
/usr/share/icons/hicolor/64x64/apps/cardputer-zero-gba.png
/usr/share/icons/hicolor/128x128/apps/cardputer-zero-gba.png
```

如果提供 SVG，也可以安装到：

```text
/usr/share/icons/hicolor/scalable/apps/cardputer-zero-gba.svg
```

安装或卸载后，`postinst/postrm` 可以尝试刷新图标缓存，但不能因为该命令不存在而导致安装失败。

## Debian Control

`packaging/debian/control` 示例：

```debcontrol
Source: cardputer-zero-gba
Section: games
Priority: optional
Maintainer: Cardputer Zero Community <dev@example.com>
Standards-Version: 4.7.0
Build-Depends:
 debhelper-compat (= 13),
 cmake,
 ninja-build,
 pkg-config,
 libsdl2-dev
Rules-Requires-Root: no

Package: cardputer-zero-gba
Architecture: any
Depends:
 ${shlibs:Depends},
 ${misc:Depends},
 libc6,
 libstdc++6,
 libgcc-s1,
 libsdl2-2.0-0,
 libsdl2-ttf-2.0-0,
 fonts-noto-cjk | fonts-droid-fallback
Description: Game Boy Advance emulator frontend for Cardputer Zero
 A Game Boy Advance emulator frontend designed for the Cardputer Zero
 320x170 display. It uses libmgba as the emulation core and provides
 an APPLaunch desktop entry for the Cardputer Zero shell.
```

当前 arm64 package 使用 bundled mGBA；因此运行时依赖不应要求目标系统安装 `libmgba`。

## mGBA Build Strategy

必须明确支持两种构建策略。

方案 A：系统 libmgba。

```text
CZ_GBA_USE_SYSTEM_MGBA=ON
```

优点：

- 包体更小。
- 安全更新由系统包管理。
- 符合 Debian 常规打包方式。

缺点：

- 目标系统必须提供 libmgba / libmgba-dev。

方案 B：随源码构建 mGBA，当前 arm64 package 默认方案。

```text
CZ_GBA_BUNDLE_MGBA=ON
```

优点：

- 不依赖系统是否提供 libmgba。
- 可以锁定 mGBA 版本。
- Cardputer Zero 仓库可以获得更稳定的行为。

缺点：

- 打包更复杂。
- 需要处理 mGBA license。
- 安全更新责任转移到本项目。
- 包体更大。

必须在 README 和 `debian/copyright` 中说明 mGBA 的来源和许可证。

## CMake Install Rules

`CMakeLists.txt` 必须包含安装规则。

示例：

```cmake
install(TARGETS cardputer-zero-gba
    RUNTIME DESTINATION "${CZ_GBA_PRIVATE_LIBDIR}"
)

install(PROGRAMS packaging/cardputer-zero-gba
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(PROGRAMS packaging/cardputer-zero-gba-applaunch
    DESTINATION "${CZ_GBA_PRIVATE_LIBDIR}"
)

install(FILES desktop/cardputer-zero-gba.desktop
    DESTINATION share/APPLaunch/applications
)

install(FILES packaging/cardputer-zero-gba.png
    DESTINATION share/APPLaunch/share/images
)

install(FILES assets/icons/cardputer-zero-gba-64.png
    DESTINATION share/icons/hicolor/64x64/apps
    RENAME cardputer-zero-gba.png
)

install(FILES assets/icons/cardputer-zero-gba-128.png
    DESTINATION share/icons/hicolor/128x128/apps
    RENAME cardputer-zero-gba.png
)

install(DIRECTORY assets/themes/
    DESTINATION share/cardputer-zero-gba/themes
)

install(DIRECTORY assets/fonts/
    DESTINATION share/cardputer-zero-gba/fonts
)

install(FILES README.md LICENSE
    DESTINATION share/doc/cardputer-zero-gba
)
```

执行：

```bash
cmake --install build --prefix /usr
```

必须生成与 deb 安装路径一致的文件布局。

## Debian Build Commands

本机推荐使用 Docker Compose 固化 arm64 编译和打包：

```bash
docker compose run --rm package-arm64
```

输出文件：

```text
out/cardputer-zero-gba_0.1.0_arm64.deb
```

Compose job 必须复制源码到容器内部 Linux filesystem、排除 build/out/rom、本地权限归一化，并运行 package smoke check。

手工 CPack 路径：

```bash
cmake -S . -B build-deb -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCZ_GBA_ENABLE_DEB_PACKAGE=ON \
  -DCZ_GBA_PACKAGE_ARCHITECTURE=arm64
cmake --build build-deb --target cardputer-zero-gba-deb
```

生成文件：

```text
build-deb/cardputer-zero-gba_0.1.0_arm64.deb
```

本地安装：

```bash
sudo apt install ./out/cardputer-zero-gba_0.1.0_arm64.deb
```

检查：

```bash
dpkg -L cardputer-zero-gba
```

## CI

如果项目使用 GitHub Actions 或类似 CI，至少应有两个 job：

```text
build-amd64
build-arm64
```

最低要求：

- 能编译。
- 能运行单元测试。
- 能生成 deb。
- 能检查 `dpkg-deb --info`。
- 能检查 `dpkg-deb --contents`。

检查命令：

```bash
dpkg-deb --info cardputer-zero-gba_*.deb
dpkg-deb --contents cardputer-zero-gba_*.deb
lintian cardputer-zero-gba_*.deb || true
```

`lintian` 可以先作为 warning，不必第一版强制全部通过。

## Deb Acceptance

生成的 `.deb` 必须满足：

1. 可以通过 `apt install ./xxx.deb` 安装。
2. 安装后 `/usr/bin/cardputer-zero-gba` 存在。
3. 安装后 `/usr/share/APPLaunch/applications/cardputer-zero-gba.desktop` 存在。
4. 安装后图标存在。
5. 安装后默认主题存在。
6. `dpkg -L cardputer-zero-gba` 能列出所有文件。
7. 普通用户运行 `cardputer-zero-gba` 不需要 sudo。
8. 首次运行能自动创建用户数据目录。
9. 首次运行不会向 `/usr` 写入运行时数据。
10. `cardputer-zero-shell` 能通过 APPLaunch `.desktop` 发现它。
11. `apt remove` 后系统文件被删除。
12. `apt remove` 后用户 ROM、存档、配置仍然保留。

最终验收命令：

```bash
sudo apt install ./cardputer-zero-gba_0.1.0_arm64.deb

which cardputer-zero-gba
dpkg -L cardputer-zero-gba
ls /usr/share/APPLaunch/applications/cardputer-zero-gba.desktop
ls /usr/share/APPLaunch/share/images/cardputer-zero-gba.png
ls /usr/share/cardputer-zero-gba/themes/minimal/theme.json

cardputer-zero-gba --version
cardputer-zero-gba --help

sudo apt remove cardputer-zero-gba
```

## Release Artifacts

正式 release 至少包含：

- Source tarball。
- `cardputer-zero-gba_0.1.0_arm64.deb`。
- `checksums.txt`。
- README。
- changelog。

推荐 release 文件：

```text
cardputer-zero-gba-0.1.0.tar.gz
cardputer-zero-gba_0.1.0_arm64.deb
cardputer-zero-gba_0.1.0_amd64.deb
SHA256SUMS
```
