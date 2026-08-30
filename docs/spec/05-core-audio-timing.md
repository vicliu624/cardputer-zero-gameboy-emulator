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

The audio callback is the master clock. There are exactly three realtime
ownership domains:

```text
emulation worker (sole mGBA owner / PCM producer)
    -> PcmRing (preallocated SPSC, whole stereo batches only)
SDL audio callback (sole PCM consumer)
    -> PulseAudio primary sink, ALSA dynamic fallback
UI / Wayland thread (input and best-effort video consumer)
```

Required behavior:

- `App`, `MgbaCore`, mGBA video state, and mGBA audio state are touched only by
  the emulation worker. SDL callbacks never call mGBA and Wayland never waits
  for the worker.
- The worker advances mGBA only while the PCM ring is below its target water
  level. It writes an interleaved S16 batch atomically: the full batch enters
  the ring or none does. A rejected write increments a metric and is a bug to
  investigate, never a hidden truncation policy.
- `SdlAudio` supplies `desired.callback` and receives PCM only in that callback.
  The callback performs no allocation, logging, lock acquisition, mGBA call,
  or backend reconfiguration. If data is short it writes silence for only the
  missing tail and increments its underrun frame counter.
- SDL starts paused. Normal playback begins only after a complete prebuffer;
  on TDVP this is 60 ms, maintained toward an 80 ms target in a 160 ms ring.
  A normal K230 callback quantum is 512 frames.
- An underrun is handled on the next UI iteration: pause and lock the SDL
  device, then notify the emulation worker. That sole ring producer clears its
  own queue and resets the A/V media epoch before playback resumes after a
  fresh prebuffer. This deliberate recovery happens outside the callback and
  prevents a brief scheduling stall becoming a periodic dropout train.
- Pause/resume, SDL playback-device removal, and a PulseAudio reconnect use
  the same state machine. SDL is closed and opened only by the UI thread; the
  emulation worker applies the newly obtained sample rate at a frame boundary,
  clears old PCM/video PTS, then produces a new prebuffer. If reopening fails,
  emulation moves to explicit muted pacing and retries at a bounded interval
  instead of filling an unconsumed ring.
- Render snapshots are deep copies tagged with the PCM frame position at which
  their emulation finished. Presentation selects the newest snapshot at or
  before callback playback plus one callback quantum. Late Wayland buffers are
  dropped/latest-wins rather than blocking audio production.
- FAST mode advances more than one emulated frame per worker turn but drains
  generated audio rather than sending compressed audio to normal-speed output.

Current audio format:

```text
TDVP K230 preferred sample rate: 44100 Hz (matches the active hardware sink)
other profiles preferred sample rate: 48000 Hz
format: S16 native-endian
channels: 2
```

`MgbaCore::read_audio_samples(sample_rate, max_frames)` must reconfigure mGBA's
blip rates when the obtained SDL sample rate differs from the current mGBA audio
rate, even if no pending samples are available on that exact call.

Telemetry is sampled by the ordinary UI thread once a second. It records the
selected SDL driver and obtained rate, PCM queue low/current/high watermarks,
underrun/rejected/recovery/reopen counters, and callback schedule-jitter
P50/P95/P99. A callback only writes its timestamp histogram atomically; it
does not print or perform a backend operation. `--present-delay-ms 20` and
`--present-delay-ms 50` are release-test stress injections that stall only the
video consumer. They must preserve `underrun=0`; skipped video is expected.

## Timing

GBA native refresh is approximately:

```text
59.727500569606 Hz
```

Implementation rules:

1. The core is advanced at native GBA frame cadence, not hard-locked to
   60.000 FPS.
2. Audio synchronization takes priority over synthetic FPS display.
3. The SDL renderer must not request `SDL_RENDERER_PRESENTVSYNC`; rendering is
   paced from the audio presentation timestamp rather than governing emulation.
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
