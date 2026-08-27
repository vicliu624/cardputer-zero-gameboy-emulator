# 03. Display, Render, Theme, UI

> **Profile scope:** the compact `320x170` canvas and fixed coordinates in
> this document are the Cardputer Zero contract. TDVP K230 has a distinct
> `410x189` large-screen application layout defined by
> `10-tdvp-k230-large-screen-ui.md`; the native GBA frame remains `240x160`.

## Internal Canvas

项目内部必须维护固定大小的软件画布：

```text
width:  320
height: 170
format: XRGB8888
```

推荐表示：

```cpp
static constexpr int SCREEN_W = 320;
static constexpr int SCREEN_H = 170;
std::array<uint32_t, SCREEN_W * SCREEN_H> frame;
```

这里的 `frame` 是应用内部用于合成最终画面的 320x170 软件画布，不是 Linux framebuffer，也不是 `/dev/fb0`。

显示链路：

```text
mGBA 240x160 video frame
  -> internal 320x170 canvas
  -> SDL_Texture
  -> SDL2 Wayland backend
  -> Wayland surface
  -> compositor
  -> DRM/KMS
  -> ST7789 display
```

所有 UI、主题、GBA 画面都应先合成到 internal canvas，然后一次上传给 SDL texture。

不得直接在 SDL renderer 上分别绘制 UI、主题和 GBA 画面。

## Layout

布局常量必须集中定义，不得散落 magic number。

```cpp
struct Layout {
    static constexpr int ScreenW = 320;
    static constexpr int ScreenH = 170;

    static constexpr int GameX = 40;
    static constexpr int GameY = 0;
    static constexpr int GameW = 240;
    static constexpr int GameH = 160;

    static constexpr int LeftX = 0;
    static constexpr int LeftY = 0;
    static constexpr int SideW = 40;
    static constexpr int SideH = 160;

    static constexpr int RightX = 280;
    static constexpr int RightY = 0;

    static constexpr int BarX = 0;
    static constexpr int BarY = 160;
    static constexpr int BarW = 320;
    static constexpr int BarH = 10;
};
```

固定布局：

```text
Cardputer Zero screen: 320x170
GBA native video:      240x160

GBA viewport:
  x=40 y=0 width=240 height=160

Left side panel:
  x=0 y=0 width=40 height=160

Right side panel:
  x=280 y=0 width=40 height=160

Bottom command bar:
  x=0 y=160 width=320 height=10
```

## Display Modes

第一版默认模式：

```text
framed_pixel
```

语义：

- 中间 240x160 原始 GBA 图像。
- 左右 40px 边框/状态/提示。
- 底部 10px command bar。

第一版不得默认提供 fullscreen stretch。

未来可选但非默认：

- `clean_pixel`: 240x160 原始像素 + 纯黑边框。
- `fit_height`: 非默认，会产生非整数缩放。
- `stretch`: 非默认，必须标记为 not recommended。

## TDVP K230 Physical Presentation

`tdvp-k230` maps to a dedicated K230 application layout as well as a physical
output profile. The K230 renderer owns a `410x189` canvas, preserves the GBA
source rectangle at `240x160`, and places it at `(85,3)` between wider UI
rails. The application copies the canvas into CPU-owned XRGB `wl_shm` buffers
at a 3x integer scale: `1230x567` at `(1,0)` on the 1232x568 landscape output.
The final game viewport is thus `720x480` physical pixels. Labwc owns DRM/KMS
and the renderer does not issue DRM operations or invoke SDL/GLES rendering.
See `08-tdvp-k230.md` for the
ABI/presentation boundary and
`10-tdvp-k230-large-screen-ui.md` for K230 UI geometry.

## Rendering Order

每帧渲染顺序固定：

1. 清空 320x170 internal canvas。
2. 绘制 theme background / bezel。
3. 绘制 GBA video frame 到 `x=40,y=0,w=240,h=160`。
4. 绘制 left side panel。
5. 绘制 right side panel。
6. 绘制 bottom command bar。
7. 绘制 toast。
8. 如果当前处于 `Paused` / `CheatMenu` / `Settings` 等状态，绘制 overlay。
9. 上传到 SDL_Texture。
10. `SDL_RenderPresent`。

## SDL Texture

第一版使用：

```cpp
SDL_Texture* texture = SDL_CreateTexture(
    renderer,
    SDL_PIXELFORMAT_XRGB8888,
    SDL_TEXTUREACCESS_STREAMING,
    320,
    170
);
```

每帧上传：

```cpp
SDL_UpdateTexture(texture, nullptr, frame.data(), 320 * sizeof(uint32_t));
SDL_RenderClear(renderer);
SDL_RenderCopy(renderer, texture, nullptr, nullptr);
SDL_RenderPresent(renderer);
```

当前实现中，本机 SDL 模拟和真机保持一致：窗口固定 320x170，且必须无边框、无标题栏、无菜单栏、无状态栏。`--scale 1` 仅作为兼容参数保留；旧的本机放大模拟不是当前默认交付规格。

```text
window size => 320x170
logical size => 320x170
```

## Theme

第一版只需要支持静态背景图：

```text
bezel.png
size: 320x170
```

渲染时先画整张 `bezel.png`，再把 GBA 画面覆盖到：

```text
x=40,y=0,w=240,h=160
```

主题作者可以直接画完整 320x170 背景图，但主题不得改变 viewport。

主题目录：

```text
/usr/share/cardputer-zero-gba/themes/
~/.local/share/cardputer-zero-gba/themes/
```

主题结构：

```text
minimal/
  theme.json
  bezel.png
```

`theme.json` 第一版结构：

```json
{
  "name": "Minimal Dark",
  "screen": {
    "width": 320,
    "height": 170
  },
  "viewport": {
    "x": 40,
    "y": 0,
    "width": 240,
    "height": 160
  },
  "background": "bezel.png",
  "font": "font_5x7",
  "show_left_status": true,
  "show_right_hints": true,
  "show_bottom_bar": true
}
```

必须校验：

- `bezel.png` 存在。
- `bezel.png` 是 320x170。
- viewport 是 `40,0,240,160`。
- 第一版不得允许主题改变 viewport 坐标。

## Visual Style From Design References

设计图可作为风格参考：

- 深色像素金属边框。
- 边框有细线、角钉、小装饰块。
- 侧栏低饱和、低对比，不抢游戏画面。
- 主文本白色。
- 强调文字和选中边框使用紫色/薰衣草色。
- 成功态、电量可使用绿色。
- FAST 倍率数字可使用蓝色/青色。
- overlay 可压暗游戏画面并居中显示菜单。
- command bar 使用高对比小像素字体。
- ROM Browser 可借鉴列表、图标、选中框、弹窗列表的视觉语言，但不采用宽屏大预览/详情分栏布局。

设计图不得变成：

- 新的 viewport 几何。
- 第一版 ROM Browser metadata 必需项。
- 第一版收藏/最近游玩必需项。
- 当前版本的 ROM Browser 大预览图、最近游玩时间、游玩时长或宽详情栏。
- 对高分辨率 mockup 的逐像素复刻要求。

## Side Panels

Left side panel 建议职责：

- 电量。
- FAST 倍率，例如 `1X` 或 `2X`。
- 当前 save slot。
- SRAM 状态。
- 快进状态。
- 当前游戏短标题。

第一版最小显示：

```text
BAT
FAST
1X
S1
```

左侧状态文字必须在 40px panel 内居中，不能贴近中间 viewport 分割线。运行中不显示 FPS 数字；FAST 状态用 `1X` / `2X` 这类倍率表达。

Right side panel 建议职责：

- A/B/L/R 映射提示。
- 主题装饰。
- 快进提示。
- 音量提示。

第一版最小显示：

```text
A:J
B:K
L:U
R:I
```

右侧按键提示必须在 40px panel 内居中，不能遮挡或压到分割线。

## Bottom Command Bar

底部 320x10 是上下文功能提示条。

Playing：

```text
MENU  SAVE  LOAD  FAST  CHEATS
```

Playing command bar 按屏幕下方五个物理键分段，而不是按文字平均分布。槽位固定为：

```text
{0, 79}, {79, 54}, {133, 54}, {187, 54}, {241, 79}
```

每个 label 在自己的槽内居中绘制。分割线绘制在槽边界上，文字不得遮挡分割线。

含义：

- `MENU`: Pause/Menu, bound to physical key `4`。
- `SAVE`: Save State, bound to physical key `5`。
- `LOAD`: Load State, bound to physical key `6`。
- `FAST`: Fast Forward, bound to physical key `7`。
- `CHEATS`: Cheat Menu, bound to physical key `8`。

版本注意：

- `CHEATS` 是当前五键提示之一，绑定物理键 `8`。
- 当前实现可以进入 CheatMenu 并读取 `.cht` 列表，但 `MgbaCore` 内真正应用 cheat 仍需独立验收。
- 不得因为 bottom bar 出现 `CHEATS` 就跳过 cheat parser、cheat state、`MgbaCore` adapter 规格。

Paused：

```text
ENT:OK  4:BK  Q:QUIT
```

CheatMenu：

```text
ENT:TOG  4:BK
```

RomBrowser：

```text
ENT:RUN  4:QUIT  UP:SEL
```

## Font

底部栏只有 10px 高，必须使用小像素字体。

推荐：

```text
5x7
or
6x8
```

底部文本绘制建议：

```text
bar_y = 160
text_y = 161
font_height = 7
```

第一版不得使用系统字体动态渲染作为默认方案。第一版应内置 bitmap pixel font，避免字体依赖、抗锯齿、缩放模糊和布局不确定。

## Toast

Toast 是短暂提示，不得长期遮挡游戏。

使用场景：

- `Saved S1`
- `Loaded S1`
- `Save failed`
- `Fast forward ON`
- `Fast forward OFF`
- `Cheat ON`
- `Cheat OFF`

显示位置优先：

- left panel。
- right panel。
- bottom bar。

不要默认覆盖 GBA viewport。

如果必须覆盖 viewport，应短暂显示，不超过 1 秒。

## Pause Menu

进入：

```text
Playing 状态按 4
```

退出：

```text
Paused 状态按 4
或选择 Resume
```

第一版菜单项：

```text
Resume
Save State
Load State
Cheats
Settings
Quit Game
```

暂停时允许在 GBA viewport 上覆盖半透明或深色菜单，因为游戏已经暂停。

必须满足：

- 菜单文字要大且清晰。
- 操作提示仍显示在 bottom command bar。
- `4` 必须能返回游戏。
