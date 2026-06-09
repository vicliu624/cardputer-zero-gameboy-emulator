# 05. Core Adapter, Audio, Timing

## GbaCore Boundary

UI, App, Storage, and Input modules must not depend directly on mGBA internal
types. The project-owned emulator boundary is `GbaCore`.

```cpp
struct GbaVideoFrame {
    const uint32_t* pixels_xrgb8888 = nullptr;
    int width = 240;
    int height = 160;
    int pitch_pixels = 240;
};

class GbaCore {
public:
    virtual ~GbaCore() = default;

    virtual bool load_rom(const std::string& rom_path) = 0;
    virtual void unload_rom() = 0;
    virtual void reset() = 0;
    virtual void run_until_next_frame() = 0;
    virtual void set_input(const GbaInputState& input) = 0;
    virtual GbaVideoFrame video_frame() const = 0;
    virtual audio::AudioSampleBatch read_audio_samples(int sample_rate, int max_frames) = 0;

    virtual bool load_sram(const std::string& path) = 0;
    virtual bool save_sram(const std::string& path) = 0;
    virtual bool load_state(const std::string& path) = 0;
    virtual bool save_state(const std::string& path) = 0;
    virtual bool load_cheats(const std::string& path) = 0;
    virtual bool set_cheat_enabled(const std::string& cheat_id, bool enabled) = 0;

    virtual std::string game_title() const = 0;
    virtual std::string game_code() const = 0;
};
```

`MgbaCore` is the only implementation that may include mGBA headers or call mGBA
APIs. If the mGBA API changes, only `MgbaCore` should need to change.

## mGBA Integration Rules

1. All mGBA API calls live inside `MgbaCore`.
2. Non-core modules must not include mGBA headers.
3. `MgbaCore` exposes only stable project-owned types.
4. `MgbaCore` receives `GbaInputState`, never SDL keycodes.
5. SRAM, save states, and cheats are accessed through the `GbaCore` boundary.
6. `MgbaCore` converts mGBA video output to 240x160 XRGB8888.
7. `MgbaCore` configures mGBA audio after ROM load/reset, because reset clears
   mGBA's blip buffers.

## Video Format Boundary

The first version uses XRGB8888 internally:

```text
0x00RRGGBB
```

The video path is:

```text
mGBA native video format
  -> MgbaCore conversion
  -> GbaVideoFrame XRGB8888 240x160
  -> Renderer copies to internal canvas x=40,y=0
```

Renderer code must not depend on mGBA native pixel formats.

## Audio Requirements

The active SDL audio path uses SDL's queued-audio device. This is the current
stable contract after the callback ring-buffer experiment produced audible
underruns on the target device:

```text
libmgba audio samples
  -> MgbaCore
  -> SdlAudio::write
  -> SDL_QueueAudio
  -> SDL audio device
  -> system audio backend
```

Required behavior:

- The main loop reads pending samples from `MgbaCore` once per frame and writes
  them into `SdlAudio`.
- `SdlAudio` opens the SDL output device with `desired.callback = nullptr` and
  writes samples with `SDL_QueueAudio`.
- `SdlAudio` uses `SDL_GetQueuedAudioSize` to cap queued samples and prevent
  unbounded latency.
- Playback starts only after a small prebuffer threshold.
- The active playback path must not pause/restart the audio device as an
  underrun recovery strategy.
- When the queue is full, `SdlAudio` skips new overflow samples instead of
  clearing the existing queue, keeping playback continuous and bounded.
- `SDL_ClearQueuedAudio` is allowed during shutdown and explicit pause only, not
  during steady playback.

FAST mode advances more than one emulated frame per main-loop tick. It must not
feed every generated sample into the same wall-clock interval. The current app
layer drains generated FAST audio from mGBA and does not queue it to SDL,
prioritizing stable normal-speed audio over compressed FAST playback.

Current audio format:

```text
sample rate: 48000 Hz preferred, obtained SDL frequency accepted
format: S16
channels: 2
```

`MgbaCore::read_audio_samples(sample_rate, max_frames)` must reconfigure mGBA's
blip rates when the obtained SDL sample rate differs from the current mGBA audio
rate, even if no pending samples are available on that exact call.

## Timing

GBA native refresh is approximately:

```text
59.727500569606 Hz
```

Implementation rules:

1. The core is advanced at native GBA frame cadence, not hard-locked to
   60.000 FPS.
2. Audio synchronization takes priority over synthetic FPS display.
3. The SDL renderer must not request `SDL_RENDERER_PRESENTVSYNC`; the main loop
   owns pacing at native GBA cadence.
4. If rendering occasionally runs late, it is better to skip or simplify video
   presentation than to let audio underrun continuously.
5. FAST mode is an explicit user action and should display a ratio such as `2X`
   or `1X`, not realtime FPS.

## Performance Budget

The first version's internal canvas is XRGB8888:

```text
320 x 170 x 4 bytes = 217,600 bytes
```

This upload size is acceptable. The performance bottleneck is more likely to be
emulation or audio scheduling than the 320x170 texture upload.

Potential future optimization:

```text
320 x 170 x 2 bytes = 108,800 bytes
```

RGB565 is a future optimization only; it must not complicate first-version SDL,
Wayland, theme, or UI code.

## First-Version Prohibitions

- shader;
- CRT filter;
- bilinear filter;
- rewind;
- runahead;
- heavy overlay animation;
- per-frame PNG loading;
- per-frame large heap allocation;
- per-frame logging;
- per-frame ROM directory scanning.

## Resource Management

Startup may load config, selected theme, font, and icon assets.

Per-frame code must not:

- read `theme.json`;
- read `bezel.png`;
- scan ROM directories;
- write config files;
- write save states;
- create SDL textures dynamically.

File writes are allowed for SRAM autosave, pause/exit saves, manual save state,
settings changes, and cheat enable/disable state.
