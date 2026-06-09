#pragma once

namespace czgba::app {

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

} // namespace czgba::app
