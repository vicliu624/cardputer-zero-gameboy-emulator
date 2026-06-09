# 04. Input, Storage, Config

## Input Layering

输入必须分层：

```text
Physical Key
  -> Input Mapper
  -> GbaInputState + AppAction
```

不得让 `SDL_Keycode` 直接进入 mGBA adapter。

## GbaInputState

```cpp
struct GbaInputState {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;

    bool a = false;
    bool b = false;
    bool l = false;
    bool r = false;

    bool start = false;
    bool select = false;
};
```

## Default Key Mapping

默认 GBA 映射：

```text
W             -> GBA Up
S             -> GBA Down
A             -> GBA Left
D             -> GBA Right
Arrow Up      -> GBA Up
Arrow Down    -> GBA Down
Arrow Left    -> GBA Left
Arrow Right   -> GBA Right

J             -> GBA A
K             -> GBA B
U             -> GBA L
I             -> GBA R

Enter         -> GBA Start
Space         -> GBA Select
```

默认 App 映射：

```text
4             -> App Menu/Pause
5             -> Save State
6             -> Load State
7             -> Fast Forward
8             -> Cheat Menu
```

## AppAction

```cpp
enum class AppAction {
    None,

    Confirm,
    Back,
    Up,
    Down,
    Left,
    Right,

    OpenMenu,
    SaveState,
    LoadState,
    ToggleFastForward,
    OpenCheatMenu,

    Quit
};
```

不同 AppMode 下，同一个键可以触发不同逻辑：

- `Playing`: `4 -> Paused`, `5 -> SaveState`, `6 -> LoadState`, `7 -> ToggleFastForward`, `8 -> CheatMenu` if cheat is enabled in this version/build。
- `Paused`: `4 -> Playing`, `Enter -> Confirm/Resume`, `Q -> Quit to ROM Browser`, `W/S` or arrows move selection。
- `CheatMenu`: `Enter -> Toggle cheat`, `4 -> Back`, `W/S` or arrows move selection。

如果当前 ROM 没有可用 cheat，`8` action 显示 unavailable toast 或忽略；不得映射回旧的 `C` 快捷键。

## Configurable Input

配置文件必须允许改键：

```json
{
  "input": {
    "gba": {
      "up": ["KEY_W", "KEY_UP"],
      "down": ["KEY_S", "KEY_DOWN"],
      "left": ["KEY_A", "KEY_LEFT"],
      "right": ["KEY_D", "KEY_RIGHT"],
      "a": ["KEY_J"],
      "b": ["KEY_K"],
      "l": ["KEY_U"],
      "r": ["KEY_I"],
      "start": ["KEY_ENTER"],
      "select": ["KEY_SPACE"]
    },
    "app": {
      "menu": ["KEY_4"],
      "save_state": ["KEY_5"],
      "load_state": ["KEY_6"],
      "fast_forward": ["KEY_7"],
      "cheat": ["KEY_8"]
    }
  }
}
```

## ROM Browser

默认 ROM 目录：

```text
~/.local/share/cardputer-zero-gba/roms/
```

仓库开发环境可额外扫描：

```text
./rom/
./roms/
```

这是本地开发和自动测试便利，不改变安装后默认用户数据目录。

第一版支持扩展名：

```text
.gba
```

第二阶段再支持：

```text
.zip
.7z
```

第一版不得实现复杂压缩包扫描。

ROM Browser 使用 320x170 全屏 UI，不需要坚持中间 240x160，因为此时没有游戏画面。

第一版显示内容：

- Title: `GBA`。
- ROM list。
- Bottom bar。

底部栏：

```text
ENT:RUN  4:QUIT  UP:SEL
```

列表排序：

```text
默认按文件名升序
```

第一版 ROM 信息只显示：

```text
文件名，不含扩展名
```

第一版不要解析 ROM header、封面、数据库、类型、最近游玩时间或游玩时长。

当前 ROM Browser 使用适配 320x170 的紧凑布局。设计图中的大预览、详情栏和最近游玩信息不进入当前布局要求。

空目录提示：

```text
No ROMs found.
Place your legally obtained .gba files in:
~/.local/share/cardputer-zero-gba/roms
```

## User Data Directories

用户数据根目录：

```text
~/.local/share/cardputer-zero-gba/
```

目录结构：

```text
~/.local/share/cardputer-zero-gba/
  roms/
  saves/
  states/
  cheats/
  screenshots/
  themes/
```

配置目录：

```text
~/.config/cardputer-zero-gba/
  config.json
```

缓存目录：

```text
~/.cache/cardputer-zero-gba/
```

程序启动时应确保：

- `roms`
- `saves`
- `states`
- `cheats`
- `themes`

这些目录存在。

## File Naming

给定 ROM：

```text
roms/Pokemon.gba
```

对应文件：

```text
saves/Pokemon.sav
states/Pokemon.slot1.state
states/Pokemon.slot2.state
states/Pokemon.slot3.state
cheats/Pokemon.cht
screenshots/Pokemon-YYYYMMDD-HHMMSS.png
```

第一版至少支持 3 个 state slot：

```text
slot1
slot2
slot3
```

## SRAM

必须实现：

- ROM 加载后，尝试加载同名 `.sav`。
- 游戏运行中可以定期 auto-save SRAM。
- 暂停时保存 SRAM。
- 退出游戏时保存 SRAM。
- 应用崩溃时不保证保存，但应尽量减少数据丢失。

建议 auto-save 间隔：

```text
30 秒
```

## Save State

默认：

```text
5 -> save current slot
6 -> load current slot
```

后续可加：

```text
[ -> previous slot
] -> next slot
```

底部/左侧显示当前 slot：

```text
S1
S2
S3
```

Toast：

```text
保存成功: SAVED S1
读取成功: LOADED S1
失败:     LOAD FAIL
```

## Cheat Files

版本裁决：

- cheat 是项目必须拥有的能力。
- 当前版本已经提供 `8 -> CHEATS` 入口、`.cht` 文件读取和 CheatMenu 列表。
- 当前 `MgbaCore::load_cheats` / `set_cheat_enabled` 尚需独立验收；UI 可显示列表和切换状态，但不得假装核心应用 cheat 已完全可用。
- `C` 不再是 CheatMenu app shortcut。

cheat 功能当前最小目标：

- 读取 `.cht` 文件。
- 展示 cheat 列表。
- 启用/禁用 cheat。
- 调用 `MgbaCore` 应用 cheat。

cheat 功能后续增强目标：

- 保存启用状态。
- 支持更严格的 `type` / `enabled` / `code` 字段。

cheat 功能第一次实现时不得做：

- 设备端编辑金手指代码。
- 在线下载金手指。
- 复杂数据库匹配。
- 长文本编辑器。

给定 ROM：

```text
roms/Pokemon.gba
```

对应 cheat：

```text
cheats/Pokemon.cht
```

当前最小 cheat 文件格式：

```ini
[Infinite Money]
820257BC 423F
820257BE 000F

[Max Items]
82025BCC 0063
```

字段：

- `[Name]`: 必须。
- section 下每个非空、非注释行按 cheat code 文本处理。
- `type`、`enabled`、`code=` 这类显式字段属于后续增强，不是当前解析器契约。

CheatMenu 行为：

```text
Up/Down      选择
Enter        启用/禁用
4            返回
```

cheat 启用状态应保存到用户状态文件，不建议直接覆盖原始 `.cht`：

```text
~/.local/share/cardputer-zero-gba/cheats/Pokemon.state.json
```

示例：

```json
{
  "Infinite Money": false,
  "Max Items": true
}
```

## Config

配置文件路径：

```text
~/.config/cardputer-zero-gba/config.json
```

默认配置：

```json
{
  "display": {
    "mode": "framed_pixel",
    "theme": "minimal",
    "show_left_status": true,
    "show_right_hints": true,
    "show_bottom_bar": true
  },
  "audio": {
    "volume": 80,
    "mute_when_paused": false,
    "sample_rate": 48000
  },
  "emulation": {
    "bios": "",
    "skip_bios": true,
    "fast_forward_speed": 2
  },
  "state": {
    "current_slot": 1,
    "auto_save_sram": true,
    "auto_save_interval_seconds": 30
  },
  "paths": {
    "roms": "~/.local/share/cardputer-zero-gba/roms",
    "saves": "~/.local/share/cardputer-zero-gba/saves",
    "states": "~/.local/share/cardputer-zero-gba/states",
    "cheats": "~/.local/share/cardputer-zero-gba/cheats",
    "themes": "~/.local/share/cardputer-zero-gba/themes"
  },
  "input": {
    "gba": {
      "up": ["KEY_W", "KEY_UP"],
      "down": ["KEY_S", "KEY_DOWN"],
      "left": ["KEY_A", "KEY_LEFT"],
      "right": ["KEY_D", "KEY_RIGHT"],
      "a": ["KEY_J"],
      "b": ["KEY_K"],
      "l": ["KEY_U"],
      "r": ["KEY_I"],
      "start": ["KEY_ENTER"],
      "select": ["KEY_SPACE"]
    },
    "app": {
      "menu": ["KEY_4"],
      "save_state": ["KEY_5"],
      "load_state": ["KEY_6"],
      "fast_forward": ["KEY_7"],
      "cheat": ["KEY_8"]
    }
  }
}
```

配置规则：

- 如果配置文件不存在，程序必须创建默认配置。
- 如果配置字段缺失，必须使用默认值补齐。
- 如果配置文件格式错误，程序不得崩溃，应显示错误并回退默认配置。
- 程序启动时应确保用户数据目录存在。
