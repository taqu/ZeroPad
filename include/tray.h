#pragma once
#include "settings_window.h"
#include <shellapi.h>
#include "lifecycle.h"
namespace zeropad {
inline constexpr UINT tray_callback = WM_APP + 41;
inline constexpr UINT open_settings_request = WM_APP + 42;
using NotifyIconFunction = BOOL (WINAPI*)(DWORD, PNOTIFYICONDATAW);
class TrayIcon {
public:
    explicit TrayIcon(NotifyIconFunction notify = Shell_NotifyIconW) noexcept : notify_(notify) {}
    ~TrayIcon() { remove(); }
    bool add(HWND owner, HICON icon, MappingMode mode) noexcept;
    bool update(MappingMode mode) noexcept;
    bool remove() noexcept;
    bool restore(MappingMode mode) noexcept;
    bool registered() const noexcept { return registered_; }
private:
    NotifyIconFunction notify_;
    NOTIFYICONDATAW data_{};
    bool registered_ = false;
};
int run_tray_app(EngineService& engine, const std::optional<std::filesystem::path>& path,
                 const std::string& diagnostic);
}
