# 06. CLI, Errors, Logging, Docs

## Command Line

必须支持：

```text
--help
--version
--scale 1
--fullscreen
--kiosk
--no-audio
--rom <path>
--config <path>
--theme <name>
--log-level <debug|info|warn|error>
```

行为：

- `--scale 1`: 兼容参数；当前窗口始终固定 320x170。
- `--fullscreen`: 在 Zero 上使用，PC 上也可用。
- `--kiosk`: 使用固定 320x170 无框无头 presentation surface，是 APPLaunch 默认路径。
- `--no-audio`: 不打开 SDL audio device，供诊断使用。
- `--rom`: 直接启动指定 ROM，跳过 ROM Browser。
- `--config`: 使用指定配置文件，方便测试。
- `--theme`: 覆盖配置中的主题。
- `--log-level`: 控制日志输出。

示例：

```bash
cardputer-zero-gba --scale 1
cardputer-zero-gba --rom ~/Games/GBA/test.gba
SDL_VIDEODRIVER=wayland cardputer-zero-gba --fullscreen
```

## SDL Platform

必须封装到 `SdlPlatform`：

```cpp
class SdlPlatform {
public:
    bool init(const PlatformConfig& config);
    void shutdown();

    void poll_events(InputFrame& input);
    void present(const uint32_t* canvas_xrgb8888);

    bool should_quit() const;

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
};
```

内部尺寸：

```text
320x170
```

窗口尺寸：

```text
window_width = 320
window_height = 170
```

Zero 和本机 SDL 模拟：

```text
scale = 1
borderless = true
decorated = false
```

第一版渲染器使用：

```cpp
SDL_RENDERER_ACCELERATED
```

不请求 renderer VSYNC；主循环按 GBA 原生帧率节奏推进：

```text
approximately 59.727500569606 Hz
```

如果某平台的显示提交阻塞，音频稳定性优先于显示同步。

## Error Handling

必须处理：

- ROM 文件不存在。
- ROM 加载失败。
- mGBA 初始化失败。
- SDL 初始化失败。
- 音频设备打开失败。
- 配置文件损坏。
- 主题文件缺失。
- `bezel.png` 尺寸错误。
- save/state 写入失败。
- cheat 文件格式错误。

错误显示原则：

- 不能直接崩溃。
- 应显示用户可理解的错误。
- 详细错误写日志。
- 如果主题失败，回退 minimal。
- 如果音频失败，允许静音继续运行。
- 如果 cheat 失败，忽略 cheat，不影响游戏运行。

## Logging

日志等级：

```text
debug
info
warn
error
```

默认：

```text
info
```

运行中不得每帧输出日志。

必须记录：

- app version。
- SDL video driver。
- SDL audio device。
- config path。
- data path。
- theme name。
- ROM path。
- mGBA load result。
- save/load state result。
- cheat load result。
- target emulation rate: native GBA timing / approximately 59.7275Hz。

## README Requirements

README 至少包含：

- 项目定位。
- 截图。
- Framed Pixel 布局说明。
- SDL2 / Wayland / internal canvas 的关系。
- 构建方法。
- 运行方法。
- ROM 目录。
- 存档目录。
- 默认按键。
- 主题格式。
- cheat 文件格式。
- Cardputer Zero 上运行方式。
- Debian package 安装方式。
- 法律声明。

项目简介建议：

```text
cardputer-zero-gba is a pixel-perfect GBA emulator frontend designed for Cardputer Zero. It uses libmgba as the emulation core and provides a 320x170 Framed Pixel interface with side panels and a 10px command bar.
```

## README Default Key Table

README 中必须有：

```text
Gameplay:
  W / Up       Up
  S / Down     Down
  A / Left     Left
  D / Right    Right
  J            GBA A
  K            GBA B
  U            GBA L
  I            GBA R
  Enter        Start
  Space        Select

App:
  4            Pause/Menu
  5            Save State
  6            Load State
  7            Fast
  8            Cheats
```

## Legal Notice

README 和无 ROM 提示必须明确：

- 用户需要使用自己合法获得的 ROM。
- 项目不附带 BIOS。
- 项目不附带商业 ROM。
- 项目不提供 ROM 下载。
