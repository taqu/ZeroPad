#include "lifecycle.h"
namespace zeropad {
TrayAction tray_action(unsigned command) noexcept {
    switch (command) {
    case tray_settings: return TrayAction::Settings;
    case tray_toggle: return TrayAction::Toggle;
    case tray_exit: return TrayAction::Exit;
    default: return TrayAction::None;
    }
}
std::wstring_view tray_tooltip(MappingMode mode) noexcept {
    switch (mode) {
    case MappingMode::Running: return L"ZeroPad - Enabled";
    case MappingMode::Disabled: return L"ZeroPad - Disabled";
    case MappingMode::Capture: return L"ZeroPad - Paused for settings";
    case MappingMode::Recovering: return L"ZeroPad - Recovering / paused";
    case MappingMode::Stopping: return L"ZeroPad - Stopping";
    }
    return L"ZeroPad";
}
}
