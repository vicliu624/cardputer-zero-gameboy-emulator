#include "emulation/emulation_runtime.hpp"

#include <algorithm>
#include <iostream>
#include <utility>

#include "storage/paths.hpp"

namespace czgba::emulation {
namespace {

constexpr std::size_t kMaxRenderTimeline = 12;
// A capped producer turn keeps a cold prebuffer quick while still allowing
// the audio callback and Wayland input/compositor work to run promptly on a
// single C908 core.
constexpr int kMaxCatchUpFramesPerTurn = 3;
constexpr double kGbaFramesPerSecond = 59.727500569606;

bool playing_with_audio(const app::RenderState& state)
{
    return state.mode == app::AppMode::Playing && !state.status.fast_forward;
}

} // namespace

EmulationRuntime::EmulationRuntime(RuntimeConfig config, audio::PcmRing& pcm_ring, CoreFactory core_factory)
    : config_(std::move(config))
    , pcm_ring_(pcm_ring)
    , core_factory_(std::move(core_factory))
{
    config_.sample_rate = std::max(8000, config_.sample_rate);
    config_.target_audio_frames = std::min(config_.target_audio_frames, pcm_ring_.capacity_frames());
    config_.start_audio_frames = std::min(config_.start_audio_frames, config_.target_audio_frames);
}

EmulationRuntime::~EmulationRuntime()
{
    stop();
}

void EmulationRuntime::start()
{
    if (worker_.joinable()) {
        return;
    }
    worker_ = std::jthread([this](std::stop_token stop_token) { run(stop_token); });
}

void EmulationRuntime::stop()
{
    if (!worker_.joinable()) {
        return;
    }
    worker_.request_stop();
    wake_.notify_all();
    worker_.join();
    set_audio_ready(false);
}

void EmulationRuntime::submit_input(const input::InputFrame& input)
{
    {
        std::lock_guard lock(input_mutex_);
        latest_input_ = input.gba;
        pending_actions_.insert(pending_actions_.end(), input.actions.begin(), input.actions.end());
        quit_requested_ = quit_requested_ || input.quit_requested;
    }
    wake_.notify_all();
}

void EmulationRuntime::notify_audio_paused()
{
    audio_reset_requested_.store(true, std::memory_order_release);
    wake_.notify_all();
}

bool EmulationRuntime::audio_ready() const
{
    return audio_ready_.load(std::memory_order_acquire);
}

RuntimeMetrics EmulationRuntime::metrics() const
{
    return {
        emulated_frames_.load(std::memory_order_relaxed),
        audio_produced_frames_.load(std::memory_order_relaxed),
        audio_write_failures_.load(std::memory_order_relaxed),
        video_frames_dropped_.load(std::memory_order_relaxed),
    };
}

std::shared_ptr<const RenderSnapshot> EmulationRuntime::take_latest_snapshot()
{
    std::lock_guard lock(snapshot_mutex_);
    if (render_timeline_.empty()) {
        return latest_snapshot_;
    }

    auto result = render_timeline_.back();
    if (render_timeline_.size() > 1) {
        video_frames_dropped_.fetch_add(render_timeline_.size() - 1, std::memory_order_relaxed);
    }
    render_timeline_.clear();
    return result;
}

std::shared_ptr<const RenderSnapshot> EmulationRuntime::take_snapshot_for_audio(
    std::uint64_t playback_frame,
    std::size_t presentation_lead_frames)
{
    std::lock_guard lock(snapshot_mutex_);
    if (render_timeline_.empty()) {
        return latest_snapshot_;
    }

    const auto target_frame = playback_frame + presentation_lead_frames;
    auto selected = render_timeline_.end();
    for (auto candidate = render_timeline_.begin(); candidate != render_timeline_.end(); ++candidate) {
        if ((*candidate)->audio_frame_end > target_frame) {
            break;
        }
        selected = candidate;
    }

    if (selected == render_timeline_.end()) {
        // Initial prebuffering has produced audio ahead of the first callback.
        // Showing the oldest frame is a small, bounded lead and avoids jumping
        // directly to a video frame that is a whole PCM target ahead.
        return render_timeline_.front();
    }

    auto result = *selected;
    const auto discarded = static_cast<std::uint64_t>(std::distance(render_timeline_.begin(), selected));
    if (discarded != 0) {
        video_frames_dropped_.fetch_add(discarded, std::memory_order_relaxed);
    }
    render_timeline_.erase(render_timeline_.begin(), std::next(selected));
    return result;
}

void EmulationRuntime::run(std::stop_token stop_token)
{
    auto core = core_factory_ ? core_factory_() : nullptr;
    if (!core) {
        std::cerr << "emulation runtime has no core instance\n";
        return;
    }
    app::App app(std::move(core), config_.working_directory);
    if (!config_.rom_path.empty()) {
        app.start_with_rom(storage::path_from_utf8(config_.rom_path));
    }

    input::InputFrame held_input;
    auto previous_tick = std::chrono::steady_clock::now();
    const auto frame_duration = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(1.0 / kGbaFramesPerSecond));
    auto next_silent_frame = previous_tick;
    publish_snapshot(app);

    while (!stop_token.stop_requested() && !app.should_quit()) {
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - previous_tick);
        previous_tick = now;

        if (audio_reset_requested_.exchange(false, std::memory_order_acq_rel)) {
            // The UI thread has already paused and locked the SDL callback.
            // It is now safe for the sole producer to discard stale PCM.
            pcm_ring_.discard_all_while_paused();
            // Both endpoints are paused at this point. Restart the media
            // timeline at zero so the next SDL callback and newly published
            // video snapshots share one explicit epoch.
            audio_produced_frames_.store(0, std::memory_order_relaxed);
            {
                std::lock_guard lock(snapshot_mutex_);
                render_timeline_.clear();
            }
            set_audio_ready(false);
        }

        auto input = take_input();
        held_input.gba = input.gba;
        held_input.actions = std::move(input.actions);
        held_input.quit_requested = input.quit_requested;

        app.tick(elapsed);
        // Apply UI actions even when the audio target is full. This operation
        // never advances mGBA unless the second argument is true.
        app.update(held_input, false);
        held_input.actions.clear();
        held_input.quit_requested = false;

        if (!config_.use_audio_clock) {
            const auto state = app.render_state();
            if (state.mode == app::AppMode::Playing) {
                app.update(held_input, true);
                // Keep mGBA's internal pending vector bounded in explicit
                // no-audio mode. This is intentional discard, unlike the
                // normal realtime audio path.
                (void)app.read_audio_samples(config_.sample_rate, config_.max_audio_read_frames);
                emulated_frames_.fetch_add(1, std::memory_order_relaxed);
            }
            set_audio_ready(false);
            publish_snapshot(app);
            next_silent_frame += frame_duration;
            std::this_thread::sleep_until(next_silent_frame);
            if (std::chrono::steady_clock::now() > next_silent_frame + std::chrono::milliseconds(100)) {
                next_silent_frame = std::chrono::steady_clock::now();
            }
            continue;
        }

        int catch_up_frames = 0;
        while (!stop_token.stop_requested() && catch_up_frames < kMaxCatchUpFramesPerTurn) {
            const auto state = app.render_state();
            if (state.mode != app::AppMode::Playing) {
                set_audio_ready(false);
                break;
            }

            if (state.status.fast_forward) {
                // Fast-forward intentionally discards generated audio, but it
                // must first mute and clear the normal realtime pipeline.
                set_audio_ready(false);
                app.update(held_input, true);
                (void)app.read_audio_samples(config_.sample_rate, config_.max_audio_read_frames);
                ++catch_up_frames;
                publish_snapshot(app);
                break;
            }

            if (pcm_ring_.queued_frames() >= config_.target_audio_frames) {
                break;
            }

            app.update(held_input, true);
            ++catch_up_frames;
            emulated_frames_.fetch_add(1, std::memory_order_relaxed);
            drain_pending_audio(app);
            publish_snapshot(app);

            if (!audio_ready() && pcm_ring_.queued_frames() >= config_.start_audio_frames) {
                set_audio_ready(true);
            }
        }

        const auto state = app.render_state();
        if (!playing_with_audio(state)) {
            set_audio_ready(false);
        }

        if (catch_up_frames == 0) {
            std::unique_lock lock(input_mutex_);
            wake_.wait_for(lock, std::chrono::milliseconds(2));
        }
    }

    set_audio_ready(false);
}

input::InputFrame EmulationRuntime::take_input()
{
    std::lock_guard lock(input_mutex_);
    input::InputFrame result;
    result.gba = latest_input_;
    result.actions.assign(pending_actions_.begin(), pending_actions_.end());
    pending_actions_.clear();
    result.quit_requested = std::exchange(quit_requested_, false);
    return result;
}

void EmulationRuntime::drain_pending_audio(app::App& app)
{
    while (pcm_ring_.writable_frames() != 0) {
        const auto max_frames = static_cast<int>(std::min<std::size_t>(
            pcm_ring_.writable_frames(),
            static_cast<std::size_t>(std::max(1, config_.max_audio_read_frames))));
        auto batch = app.read_audio_samples(config_.sample_rate, max_frames);
        if (batch.interleaved_s16.empty()) {
            break;
        }

        const auto frames = static_cast<std::uint64_t>(batch.frame_count());
        if (!pcm_ring_.write_all(batch.samples())) {
            // This is a contract violation: max_frames was derived from the
            // queue's current writable space. Keep it observable rather than
            // silently truncating PCM like SDL_QueueAudio did before.
            audio_write_failures_.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        audio_produced_frames_.fetch_add(frames, std::memory_order_relaxed);
    }
}

void EmulationRuntime::publish_snapshot(const app::App& app)
{
    auto snapshot = make_snapshot(app);
    std::lock_guard lock(snapshot_mutex_);
    latest_snapshot_ = snapshot;
    render_timeline_.push_back(std::move(snapshot));
    while (render_timeline_.size() > kMaxRenderTimeline) {
        render_timeline_.pop_front();
        video_frames_dropped_.fetch_add(1, std::memory_order_relaxed);
    }
}

std::shared_ptr<RenderSnapshot> EmulationRuntime::make_snapshot(const app::App& app) const
{
    auto snapshot = std::make_shared<RenderSnapshot>();
    snapshot->state = app.render_state();
    snapshot->emulation_frame = emulated_frames_.load(std::memory_order_relaxed);
    snapshot->audio_frame_end = audio_produced_frames_.load(std::memory_order_relaxed);

    if (snapshot->state.game_frame == nullptr || snapshot->state.game_frame->pixels_xrgb8888 == nullptr) {
        snapshot->state.game_frame = nullptr;
        return snapshot;
    }

    const auto source = *snapshot->state.game_frame;
    if (source.width <= 0 || source.height <= 0 || source.pitch_pixels < source.width) {
        snapshot->state.game_frame = nullptr;
        return snapshot;
    }

    snapshot->pixels.resize(static_cast<std::size_t>(source.width) * static_cast<std::size_t>(source.height));
    for (int y = 0; y < source.height; ++y) {
        const auto* source_row = source.pixels_xrgb8888 + static_cast<std::size_t>(y) * source.pitch_pixels;
        auto* target_row = snapshot->pixels.data() + static_cast<std::size_t>(y) * source.width;
        std::copy_n(source_row, source.width, target_row);
    }
    snapshot->video_frame = {snapshot->pixels.data(), source.width, source.height, source.width};
    snapshot->state.game_frame = &snapshot->video_frame;
    return snapshot;
}

void EmulationRuntime::set_audio_ready(bool ready)
{
    audio_ready_.store(ready, std::memory_order_release);
}

} // namespace czgba::emulation
