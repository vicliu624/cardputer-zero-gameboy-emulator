# 02. Architecture And Runtime

## Cardputer Zero APPLaunch Boundary

Cardputer Zero shell discovery is an APPLaunch packaging boundary, not an App
runtime boundary. The scanned desktop file is installed under
`/usr/share/APPLaunch/applications/`, while emulation, input, storage, and
rendering remain behind the existing App/Core/Platform/Renderer interfaces.

The APPLaunch package-owned roots are:

```text
desktop/cardputer-zero-gba.desktop
packaging/cardputer-zero-gba
packaging/cardputer-zero-gba-applaunch
packaging/cardputer-zero-gba.png
packaging/debian/
```

The desktop entry installed under APPLaunch must keep:

```text
Name=GBE
Exec=/usr/lib/cardputer-zero-gba/cardputer-zero-gba-applaunch
Icon=share/images/cardputer-zero-gba.png
X-Zero-AppId=io.github.vicliu624.cardputer-zero-gba
X-Zero-Display=wayland
```

The window created by the runtime is always the app surface: 320x170,
borderless, undecorated, and without title/status/menu chrome. `--kiosk` uses
that same surface as the shell-launched path.

## 总体架构

目标结构�?
```text
cardputer-zero-gba
  App Runtime
    AppMode state machine
    Command bar
    Toast / overlay

  UI
    ROM Browser
    Pause Menu
    Cheat Menu
    Settings Menu

  Render
    320x170 XRGB8888 internal canvas
    Framed Pixel layout
    5x7 font
    Theme/bezel
    Side panels / bottom bar

  Platform
    SDL video
    SDL input
    SDL queued audio

  Core
    GbaCore interface
    MgbaCore adapter

  Storage
    Config
    ROM scanner
    Save manager
    Paths

  Cheat
    .cht parser
    Cheat state manager

  Packaging
    Debian package
    .desktop
    icon/theme assets
```

## 推荐源码结构

实现应贴近以下结构，允许小幅调整，但不得破坏边界�?
```text
src/
  main.cpp
  app/
    app.hpp / app.cpp
    app_mode.hpp
    app_context.hpp
    command_bar.hpp / command_bar.cpp
    toast.hpp / toast.cpp
  core/
    gba_core.hpp
    gba_input.hpp
    gba_video_frame.hpp
    mgba_core.hpp / mgba_core.cpp
  platform/
    platform.hpp
    sdl_platform.hpp / sdl_platform.cpp
    sdl_video.hpp / sdl_video.cpp
    sdl_audio.hpp / sdl_audio.cpp
    sdl_input.hpp / sdl_input.cpp
  render/
    layout.hpp
    canvas.hpp / canvas.cpp
    renderer.hpp / renderer.cpp
    font_5x7.hpp / font_5x7.cpp
    theme.hpp / theme.cpp
  ui/
    ui_screen.hpp
    rom_browser.hpp / rom_browser.cpp
    pause_menu.hpp / pause_menu.cpp
    cheat_menu.hpp / cheat_menu.cpp
    settings_menu.hpp / settings_menu.cpp
  storage/
    paths.hpp / paths.cpp
    config.hpp / config.cpp
    save_manager.hpp / save_manager.cpp
    rom_scanner.hpp / rom_scanner.cpp
  cheat/
    cheat.hpp
    cheat_parser.hpp / cheat_parser.cpp
    cheat_manager.hpp / cheat_manager.cpp
  util/
    log.hpp
    file.hpp / file.cpp
    time.hpp
```

Asset and packaging roots:

```text
assets/
  themes/minimal/theme.json
  themes/minimal/bezel.png
  fonts/font_5x7.png or font_5x7.bin
  icons/cardputer-zero-gba-64.png
  icons/cardputer-zero-gba-128.png

desktop/cardputer-zero-gba.desktop
packaging/debian/
```

## AppMode

必须定义应用状态：

```cpp
enum class AppMode {
    RomBrowser,
    Playing,
    Paused,
    CheatMenu,
    SaveStateMenu,
    Settings,
    ConfirmQuit,
    Error
};
```

状态语义：

- `RomBrowser`: 显示 ROM 列表，用户选择游戏�?- `Playing`: 正常运行游戏，按 GBA 原始时序推进 core�?- `Paused`: 暂停游戏，显示暂停菜单�?- `CheatMenu`: 显示当前 ROM 对应的金手指列表�?- `SaveStateMenu`: 显示即时存档槽位�?- `Settings`: 显示设置页�?- `ConfirmQuit`: 退出确认�?- `Error`: 显示错误信息，例�?ROM 加载失败�?
## 主循�?
主循环必须遵循：

1. 读取输入事件�?2. 将输入转换为 `AppAction` �?`GbaInputState`�?3. 根据当前 `AppMode` 更新应用状态�?4. 如果 `AppMode == Playing`，按 GBA 原始时序推进 libmgba�?5. 生成当前 UI 状态�?6. 渲染�?320x170 internal canvas�?7. 提交�?SDL_Texture�?8. �?SDL2 / Wayland 输出�?
伪代码语义：

```cpp
while (app.is_running()) {
    platform.poll_events(input);
    app.handle_input(input);

    if (app.mode() == AppMode::Playing) {
        core.set_input(input.gba);
        core.run_until_next_frame();
    }

    audio.write(app.read_audio_samples(audio.sample_rate()));

    renderer.begin_frame();
    renderer.draw_theme_background();
    renderer.draw_gba_frame(core.video_frame(), Layout::GameX, Layout::GameY);
    renderer.draw_left_panel(app.status());
    renderer.draw_right_panel(app.hints());
    renderer.draw_bottom_bar(app.command_bar());

    if (app.has_overlay()) {
        renderer.draw_overlay(app.overlay());
    }

    renderer.end_frame();
    platform.present(renderer.canvas());
}
```

`run_until_next_frame()` 是抽象语义，允许实际实现命名�?`run_frame()`，但必须表示“推进到下一�?GBA video frame”，不是“强制按 60.000 FPS 跑一帧”�?
## 依赖方向

允许依赖�?
- `app` 依赖 `core` 抽象接口、`render` 抽象、`storage`、`ui`�?- `platform` 依赖 SDL2�?- `core/mgba_core.*` 依赖 mGBA�?- `render` 依赖 internal canvas、theme、font�?- `ui` 依赖 app-facing UI model，不依赖 mGBA�?- `storage` 依赖文件系统、JSON�?- `cheat` 依赖 cheat 文件格式和状态，不直接依�?UI�?
禁止依赖�?
- UI include mGBA 头文件�?- App 直接操作 mGBA 内部对象�?- Storage 直接操作 mGBA 内部对象�?- Input mapper 直接调用 mGBA�?- Renderer 理解 mGBA native pixel format�?- SDL_Keycode 进入 `GbaCore` �?`MgbaCore` 对外接口�?
## 概念边界

必须区分�?
- `internal canvas`: 应用内存中的 320x170 XRGB8888 合成画布�?- `SDL_Texture`: internal canvas 的上传目标�?- `Wayland`: SDL2 �?Cardputer Zero 上使用的显示后端�?- `GBA video frame`: libmgba 产生，经 `MgbaCore` 归一化后�?240x160 XRGB8888 帧�?- `theme/bezel`: 320x170 背景，不拥有 viewport 坐标�?- `GbaInputState`: �?GBA core 的输入状态�?- `AppAction`: 应用级动作�?- `PhysicalKey`: 平台输入事件归一化后的物理键�?
## 新模块合法性检�?
新增 model/service/util/adapter/handler 前必须回答：

- 它是否对应真实边界，还是只是临时实现便利�?- 它属�?app/core/platform/render/ui/storage/cheat/util 哪一层�?- 它是否让 mGBA、SDL、theme、UI mockup 泄漏到不该知道的层�?- 它是否把展示结构上升成核心模型�?
如果回答不清，不应新增该模块�?