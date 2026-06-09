#include "app/command_bar.hpp"

namespace czgba::app {

std::string CommandBar::text_for(AppMode mode) const
{
    switch (mode) {
    case AppMode::RomBrowser:
        return "ENT:RUN 4:QUIT UP:SEL";
    case AppMode::Playing:
        return "MENU|SAVE|LOAD|FAST|CHEATS";
    case AppMode::Paused:
        return "ENT:OK 4:BK Q:QUIT";
    case AppMode::CheatMenu:
        return "ENT:TOG 4:BK";
    case AppMode::SaveStateMenu:
        return "ENT:OK 4:BK";
    case AppMode::Settings:
        return "4:BK";
    case AppMode::ConfirmQuit:
        return "ENT:QUIT 4:BK";
    case AppMode::Error:
        return "ENT:OK 4:BK";
    default:
        return "";
    }
}

} // namespace czgba::app
