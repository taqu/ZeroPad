#include "tray.h"
#include "single_instance.h"
#include "../resources/resource.h"
#include <wtsapi32.h>
#include <windowsx.h>
namespace zeropad {
namespace {
void diagnostic(const wchar_t* text) { OutputDebugStringW(L"[ZeroPad] "); OutputDebugStringW(text); OutputDebugStringW(L"\n"); }
struct Host {
    EngineService& engine;
    std::optional<std::filesystem::path> path;
    std::string startup_diagnostic;
    AppLifecycle lifecycle;
    TrayIcon icon{};
    HWND window = nullptr;
    HICON resource_icon = nullptr;
    UINT taskbar_created = 0, open_message = 0;
    int registration_attempts = 0;
    bool sessions = false;
    std::optional<MappingMode> last_mode;
    std::optional<bool> connected;
    bool injection_failure = false;
    int exit_code = 0;
};
void close_editors(HWND host) {
    EnumThreadWindows(GetCurrentThreadId(), [](HWND popup, LPARAM value) -> BOOL {
        const auto owner = reinterpret_cast<HWND>(value);
        if (popup == owner || GetAncestor(popup, GA_ROOTOWNER) != owner) return TRUE;
        SendMessageW(popup, settings_force_close, 0, 0);
        wchar_t title[80]{}; GetWindowTextW(popup, title, 80);
        if (std::wstring_view(title) == L"ZeroPad") EndDialog(popup, IDCANCEL);
        return TRUE;
    }, reinterpret_cast<LPARAM>(host));
}
void begin_exit(Host& host) {
    if (!host.lifecycle.exit()) return;
    diagnostic(L"Shutdown requested");
    host.engine.request_stop(); // Gates key-downs before closing any dialogs.
    close_editors(host.window);
}
void register_icon(Host& host, bool explorer_restart = false) {
    const auto mode = host.engine.status().engine.mode;
    const bool ok = explorer_restart ? host.icon.restore(mode) : host.icon.add(host.window, host.resource_icon, mode);
    if (ok) {
        host.registration_attempts = 0; KillTimer(host.window, 2);
        ShowWindow(host.window, SW_HIDE);
    } else {
        diagnostic(L"Tray icon registration failed; fallback window available");
        ShowWindow(host.window, SW_SHOWNOACTIVATE);
        if (++host.registration_attempts < 3) SetTimer(host.window, 2, 5000, nullptr);
        else KillTimer(host.window, 2);
    }
}
void show_settings(Host& host) {
    if (host.lifecycle.stopping) return;
    if (!host.lifecycle.open_settings()) {
        HWND existing = GetLastActivePopup(host.window);
        for (int depth = 0; depth < 8; ++depth) {
            const HWND next = GetLastActivePopup(existing);
            if (next == existing || !IsWindow(next)) break;
            existing = next;
        }
        if (existing != host.window && IsWindow(existing)) {
            ShowWindow(existing, SW_RESTORE); SetForegroundWindow(existing);
        }
        return;
    }
    const auto result = run_settings_window(host.engine, host.path, host.startup_diagnostic, host.window);
    host.startup_diagnostic.clear();
    host.lifecycle.settings_closed();
    if (result == -1) {
        diagnostic(L"Cannot create settings dialog");
        MessageBoxW(host.window, L"Cannot open Settings. ZeroPad will exit safely.", L"ZeroPad", MB_OK | MB_ICONERROR);
        begin_exit(host);
    }
}
void dispatch(Host& host, TrayAction action) {
    if (host.lifecycle.stopping) return;
    switch (action) {
    case TrayAction::Settings: PostMessageW(host.window, open_settings_request, 0, 0); break;
    case TrayAction::Exit: begin_exit(host); break;
    case TrayAction::Toggle: {
        const auto mode = host.engine.status().engine.mode;
        if (mode == MappingMode::Running || mode == MappingMode::Disabled)
            host.engine.runtime_enabled(mode == MappingMode::Disabled);
        break;
    }
    default: break;
    }
}
void menu(Host& host) {
    HMENU popup = CreatePopupMenu();
    if (!popup) { diagnostic(L"Cannot create tray menu"); return; }
    const auto mode = host.engine.status().engine.mode;
    AppendMenuW(popup, MF_STRING | MF_GRAYED, 0, L"ZeroPad");
    AppendMenuW(popup, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(popup, MF_STRING, tray_settings, L"Settings");
    AppendMenuW(popup, MF_SEPARATOR, 0, nullptr);
    const UINT flags = MF_STRING | ((mode == MappingMode::Running || mode == MappingMode::Disabled) ? 0u : MF_GRAYED);
    AppendMenuW(popup, flags, tray_toggle, mode == MappingMode::Running ? L"Disable Mapping" : L"Enable Mapping");
    AppendMenuW(popup, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(popup, MF_STRING, tray_exit, L"Exit");
    POINT point{};
    GetCursorPos(&point); // Version-4 WM_CONTEXTMENU has no guaranteed anchor.
    HWND foreground = GetLastActivePopup(host.window);
    if (foreground == host.window) { EnableWindow(host.window, TRUE); foreground = host.window; }
    SetForegroundWindow(foreground);
    const auto command = TrackPopupMenuEx(popup, TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, foreground, nullptr);
    PostMessageW(host.window, WM_NULL, 0, 0);
    DestroyMenu(popup);
    dispatch(host, tray_action(command));
}
LRESULT CALLBACK host_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* host = reinterpret_cast<Host*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        host = static_cast<Host*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        host->window = window; SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(host));
    }
    if (!host) return DefWindowProcW(window, message, wparam, lparam);
    if (message == host->open_message) {
        if (host->lifecycle.stopping) return 0;
        PostMessageW(window, open_settings_request, 0, 0); return 1;
    }
    if (message == host->taskbar_created) {
        if (!host->lifecycle.stopping) { host->registration_attempts = 0; register_icon(*host, true); }
        return 0;
    }
    switch (message) {
    case open_settings_request: show_settings(*host); return 0;
    case tray_callback:
        if (HIWORD(lparam) != 1) return 0;
        if (LOWORD(lparam) == WM_LBUTTONDBLCLK || LOWORD(lparam) == NIN_KEYSELECT) dispatch(*host, TrayAction::Settings);
        else if (LOWORD(lparam) == WM_CONTEXTMENU) menu(*host);
        return 0;
    case WM_COMMAND: dispatch(*host, tray_action(LOWORD(wparam))); return 0;
    case WM_POWERBROADCAST:
        if (wparam == PBT_APMSUSPEND) { diagnostic(L"System suspend"); host->engine.power(true); }
        else if (wparam == PBT_APMRESUMEAUTOMATIC || wparam == PBT_APMRESUMESUSPEND) { diagnostic(L"System resume"); host->engine.power(false); }
        return TRUE;
    case WM_WTSSESSION_CHANGE:
        if (wparam == WTS_SESSION_LOCK || wparam == WTS_CONSOLE_DISCONNECT || wparam == WTS_REMOTE_DISCONNECT) host->engine.session_locked(true);
        else if (wparam == WTS_SESSION_UNLOCK || wparam == WTS_CONSOLE_CONNECT || wparam == WTS_REMOTE_CONNECT) host->engine.session_locked(false);
        return 0;
    case WM_QUERYENDSESSION: begin_exit(*host); return TRUE;
    case WM_ENDSESSION: if (wparam) begin_exit(*host); return 0;
    case WM_CLOSE: begin_exit(*host); return 0;
    case WM_TIMER: {
        if (wparam == 2) { if (!host->lifecycle.stopping) register_icon(*host); return 0; }
        const auto status = host->engine.status();
        if (status.worker_failed && !host->lifecycle.stopping) { host->exit_code = 1; diagnostic(L"Input worker failed"); begin_exit(*host); }
        if (host->lifecycle.stopping && status.stopped) {
            host->engine.stop(); // Join the acknowledged worker before GUI/tray teardown.
            if (status.engine.cleanup_failed) { diagnostic(L"Unresolved key releases at shutdown"); host->exit_code = 1; }
            if (!host->icon.remove()) diagnostic(L"Tray icon removal failed");
            if (host->sessions) WTSUnRegisterSessionNotification(window);
            DestroyWindow(window); return 0;
        }
        if (host->connected != status.engine.connected) {
            host->connected = status.engine.connected;
            diagnostic(*host->connected ? L"Controller connected" : L"Controller disconnected");
        }
        if (host->last_mode != status.engine.mode) {
            host->last_mode = status.engine.mode;
            diagnostic(tray_tooltip(status.engine.mode).data());
            if (host->icon.registered() && !host->icon.update(status.engine.mode)) {
                host->icon.remove(); host->registration_attempts = 0; register_icon(*host);
            }
        }
        if (status.engine.injection_failed && !host->injection_failure) diagnostic(L"Input injection failed; state retained for retries");
        host->injection_failure = status.engine.injection_failed;
        return 0;
    }
    case WM_DESTROY: KillTimer(window, 1); KillTimer(window, 2); PostQuitMessage(host->exit_code); return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
}
int run_tray_app(EngineService& engine, const std::optional<std::filesystem::path>& path, const std::string& warning) {
    Host host{engine, path, warning};
    host.taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");
    host.open_message = RegisterWindowMessageW(open_settings_message);
    host.resource_icon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_ZEROPAD), IMAGE_ICON, 16, 16, LR_SHARED));
    if (!host.taskbar_created || !host.open_message || !host.resource_icon) return 1;
    WNDCLASSW type{}; type.lpfnWndProc = host_proc; type.hInstance = GetModuleHandleW(nullptr);
    type.lpszClassName = host_class; type.hIcon = host.resource_icon; type.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    type.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    if (!RegisterClassW(&type) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return 1;
    const HWND window = CreateWindowExW(0, host_class, L"ZeroPad - Tray unavailable", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                                       CW_USEDEFAULT, CW_USEDEFAULT, 430, 155, nullptr, nullptr, type.hInstance, &host);
    if (!window) return 1;
    CreateWindowExW(0, L"STATIC", L"Tray unavailable. Use Settings or Exit.\nRegistration retries are bounded.", WS_CHILD | WS_VISIBLE, 16, 12, 380, 40, window, nullptr, type.hInstance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Settings", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 16, 65, 150, 30, window, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(tray_settings)), type.hInstance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Exit", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 220, 65, 150, 30, window, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(tray_exit)), type.hInstance, nullptr);
    host.sessions = WTSRegisterSessionNotification(window, NOTIFY_FOR_THIS_SESSION) != FALSE;
    if (!host.sessions) { diagnostic(L"Cannot register session notifications; refusing unattended input"); engine.session_locked(true); }
    // Detect launch into an already locked/noninteractive desktop. The worker
    // also checks desktop availability before each physical sample.
    diagnostic(L"Application startup");
    if (!warning.empty()) diagnostic(L"Configuration startup warning (shown in Settings)");
    register_icon(host);
    if (!SetTimer(window, 1, 500, nullptr)) {
        diagnostic(L"Cannot create lifecycle timer"); engine.stop(); host.icon.remove();
        if (host.sessions) WTSUnRegisterSessionNotification(window);
        DestroyWindow(window); return 1;
    }
    if (host.sessions) PostMessageW(window, open_settings_request, 0, 0);
    else { host.exit_code = 1; begin_exit(host); }
    MSG message{};
    int result;
    while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
        if (!IsDialogMessageW(window, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    engine.stop();
    host.icon.remove();
    if (IsWindow(window)) { if (host.sessions) WTSUnRegisterSessionNotification(window); DestroyWindow(window); }
    diagnostic(L"Application shutdown");
    return result == -1 ? 1 : host.exit_code;
}
}
