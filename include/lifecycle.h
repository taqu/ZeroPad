#pragma once
#include "settings.h"
namespace zeropad {
enum class TrayAction { None, Settings, Toggle, Exit };
inline constexpr unsigned tray_settings = 4001, tray_toggle = 4002, tray_exit = 4003;
TrayAction tray_action(unsigned command) noexcept;
std::wstring_view tray_tooltip(MappingMode mode) noexcept;
// Window lifetime is independent of engine lifetime. The native host uses
// this model to reject duplicate editors and new actions during shutdown.
struct AppLifecycle {
    bool settings_open = false;
    bool stopping = false;
    bool open_settings() noexcept {
        if (stopping || settings_open) return false;
        settings_open = true; return true;
    }
    void settings_closed() noexcept { settings_open = false; }
    bool exit() noexcept { if (stopping) return false; stopping = true; return true; }
};
}
