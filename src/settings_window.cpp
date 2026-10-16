#include "settings_window.h"
#define NOMINMAX
#include <Windows.h>
#include <CommCtrl.h>
#include <Xinput.h>
#include "../resources/resource.h"
#include <algorithm>
#include <iomanip>
#include <locale>
#include <sstream>

namespace zeropad {
namespace {
std::wstring widen(std::string_view text) {
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
}
std::wstring text(HWND window, int id) {
    const HWND control = GetDlgItem(window, id);
    const int length = GetWindowTextLengthW(control);
    std::wstring result(static_cast<std::size_t>(length) + 1, L'\0');
    GetWindowTextW(control, result.data(), length + 1);
    result.resize(static_cast<std::size_t>(length));
    return result;
}
void set_text(HWND window, int id, const std::wstring& value) {
    if (text(window, id) != value) SetDlgItemTextW(window, id, value.c_str());
}
int px(HWND window, int logical) { return MulDiv(logical, static_cast<int>(GetDpiForWindow(window)), 96); }
void move(HWND window, int id, int x, int y, int width, int height) {
    MoveWindow(GetDlgItem(window, id), px(window, x), px(window, y), px(window, width), px(window, height), TRUE);
}
void update_font(HWND window, HFONT& font) {
    NONCLIENTMETRICSW metrics{sizeof(metrics)};
    SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, GetDpiForWindow(window));
    HFONT next = CreateFontIndirectW(&metrics.lfMessageFont);
    if (!next) return;
    SendMessageW(window, WM_SETFONT, reinterpret_cast<WPARAM>(next), TRUE);
    EnumChildWindows(window, [](HWND child, LPARAM value) -> BOOL {
        SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(value), TRUE); return TRUE;
    }, reinterpret_cast<LPARAM>(next));
    if (font) DeleteObject(font);
    font = next;
}
void size_window(HWND window, int width, int height) {
    RECT rectangle{0, 0, px(window, width), px(window, height)};
    AdjustWindowRectExForDpi(&rectangle, static_cast<DWORD>(GetWindowLongPtrW(window, GWL_STYLE)), FALSE,
                            static_cast<DWORD>(GetWindowLongPtrW(window, GWL_EXSTYLE)), GetDpiForWindow(window));
    MONITORINFO monitor{sizeof(monitor)};
    GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor);
    const int w = std::min(rectangle.right - rectangle.left, monitor.rcWork.right - monitor.rcWork.left);
    const int h = std::min(rectangle.bottom - rectangle.top, monitor.rcWork.bottom - monitor.rcWork.top);
    SetWindowPos(window, nullptr, monitor.rcWork.left + (monitor.rcWork.right - monitor.rcWork.left - w) / 2,
                 monitor.rcWork.top + (monitor.rcWork.bottom - monitor.rcWork.top - h) / 2, w, h, SWP_NOZORDER);
}
void error_box(HWND window, const std::string& error) { MessageBoxW(window, widen(error).c_str(), L"ZeroPad", MB_OK | MB_ICONERROR); }
std::wstring numeric(double value) {
    std::wostringstream out; out.imbue(std::locale::classic()); out << std::setprecision(17) << value; return out.str();
}
struct BindingDialog {
    std::uint16_t original;
    std::optional<std::uint16_t> selected;
    HFONT font = nullptr;
    unsigned capture_vk = 0;
};
void select_key(HWND dialog, BindingDialog& state, std::uint16_t key) {
    state.selected = key;
    set_text(dialog, IDC_SELECTED, L"Selected: " + widen(key_name(key)));
    EnableWindow(GetDlgItem(dialog, IDOK), TRUE);
}
LRESULT CALLBACK capture_proc(HWND control, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR) {
    const HWND dialog = GetParent(control);
    auto* state = reinterpret_cast<BindingDialog*>(GetWindowLongPtrW(dialog, DWLP_USER));
    if (message == WM_GETDLGCODE) return DLGC_WANTALLKEYS;
    if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) {
        if (!state || (lparam & (1LL << 30)) != 0) return 0;
        if (static_cast<std::uintptr_t>(GetMessageExtraInfo()) == injected_key_tag) return 0;
        if (wparam == VK_ESCAPE) { SendMessageW(dialog, WM_COMMAND, IDCANCEL, 0); return 0; }
        const auto key = captured_key(static_cast<unsigned>(wparam), static_cast<unsigned>((lparam >> 16) & 0xFF),
                                      (lparam & (1LL << 24)) != 0);
        if (key) {
            select_key(dialog, *state, *key);
            state->capture_vk = static_cast<unsigned>(wparam);
        } else {
            state->selected.reset();
            EnableWindow(GetDlgItem(dialog, IDOK), FALSE);
            set_text(dialog, IDC_SELECTED, L"Unsupported key. Try a supported key or choose from the list.");
        }
        return 0;
    }
    if ((message == WM_KEYUP || message == WM_SYSKEYUP) && state && state->capture_vk == wparam) {
        state->capture_vk = 0;
        SetFocus(GetDlgItem(dialog, IDOK));
        return 0;
    }
    if (message == WM_CHAR || message == WM_SYSCHAR) return 0;
    if (message == WM_NCDESTROY) RemoveWindowSubclass(control, capture_proc, 1);
    return DefSubclassProc(control, message, wparam, lparam);
}
void binding_layout(HWND window) {
    RECT r{}; GetClientRect(window, &r);
    const int w = MulDiv(r.right, 96, static_cast<int>(GetDpiForWindow(window)));
    const int h = MulDiv(r.bottom, 96, static_cast<int>(GetDpiForWindow(window)));
    move(window, IDC_CAPTURE_PROMPT, 16, 16, w - 32, 40);
    move(window, IDC_CAPTURE, 16, 64, w - 32, 36);
    move(window, IDC_SELECTED, 16, 110, w - 32, 44);
    move(window, IDC_CHOOSE, 16, 160, 130, 28);
    move(window, IDC_KEYS_LABEL, 16, 204, 115, 22);
    move(window, IDC_KEYS, 140, 200, w - 156, 220);
    move(window, IDCANCEL, w - 200, h - 44, 86, 28);
    move(window, IDOK, w - 102, h - 44, 86, 28);
}
INT_PTR CALLBACK binding_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* state = reinterpret_cast<BindingDialog*>(GetWindowLongPtrW(window, DWLP_USER));
    if (message == WM_INITDIALOG) {
        state = reinterpret_cast<BindingDialog*>(lparam);
        SetWindowLongPtrW(window, DWLP_USER, lparam);
        update_font(window, state->font);
        size_window(window, 500, 300);
        binding_layout(window);
        const HWND combo = GetDlgItem(window, IDC_KEYS);
        for (unsigned key = 0; key < 256; ++key) {
            const auto name = key_name(static_cast<std::uint16_t>(key));
            if (name.empty()) continue;
            const auto row = SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(widen(name).c_str()));
            SendMessageW(combo, CB_SETITEMDATA, static_cast<WPARAM>(row), key);
            if (key == state->original) SendMessageW(combo, CB_SETCURSEL, static_cast<WPARAM>(row), 0);
        }
        EnableWindow(GetDlgItem(window, IDOK), FALSE);
        SetWindowSubclass(GetDlgItem(window, IDC_CAPTURE), capture_proc, 1, 0);
        SetFocus(GetDlgItem(window, IDC_CAPTURE));
        return FALSE;
    }
    if (!state) return FALSE;
    switch (message) {
    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case IDOK: if (state->selected) EndDialog(window, IDOK); return TRUE;
        case IDCANCEL: state->selected.reset(); EndDialog(window, IDCANCEL); return TRUE;
        case IDC_CHOOSE:
        case IDC_KEYS:
            if (LOWORD(wparam) == IDC_KEYS && HIWORD(wparam) != CBN_SELCHANGE) break;
            {
                const HWND combo = GetDlgItem(window, IDC_KEYS);
                const auto row = SendMessageW(combo, CB_GETCURSEL, 0, 0);
                if (row != CB_ERR) select_key(window, *state, static_cast<std::uint16_t>(SendMessageW(combo, CB_GETITEMDATA, static_cast<WPARAM>(row), 0)));
                if (LOWORD(wparam) == IDC_CHOOSE) { SetFocus(combo); SendMessageW(combo, CB_SHOWDROPDOWN, TRUE, 0); }
            }
            return TRUE;
        }
        break;
    case WM_CLOSE: state->selected.reset(); EndDialog(window, IDCANCEL); return TRUE;
    case WM_DPICHANGED: {
        const auto* r = reinterpret_cast<RECT*>(lparam);
        SetWindowPos(window, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER);
        update_font(window, state->font); binding_layout(window); return TRUE;
    }
    case WM_DESTROY: if (state->font) { DeleteObject(state->font); state->font = nullptr; } return TRUE;
    }
    return FALSE;
}
LRESULT CALLBACK list_proc(HWND control, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR) {
    if (message == WM_GETDLGCODE && lparam) {
        const auto* key = reinterpret_cast<MSG*>(lparam);
        if (key->message == WM_KEYDOWN && key->wParam == VK_RETURN) return DLGC_WANTMESSAGE;
    }
    if (message == WM_KEYDOWN && wparam == VK_RETURN) {
        PostMessageW(GetParent(control), WM_COMMAND, IDC_CHANGE, 0); return 0;
    }
    if (message == WM_NCDESTROY) RemoveWindowSubclass(control, list_proc, 1);
    return DefSubclassProc(control, message, wparam, lparam);
}
struct Window {
    EngineService& engine;
    std::optional<std::filesystem::path> path;
    SettingsEditor editor;
    std::string diagnostic;
    HFONT font = nullptr;
    bool loading = false;
    enum class Operation { Idle, Capture, Save, ApplySave, Reload, ApplyReload } operation = Operation::Idle;
    Keyboard::Clock::time_point deadline{};
    Config candidate;
    DiskSnapshot reload_disk;
    std::uint64_t request = 0;
    std::uint64_t pause_request = 0;
    std::size_t row = 0;
    bool disk_readable = true;
    Window(EngineService& service, std::optional<std::filesystem::path> location, std::string warning)
        : engine(service), path(std::move(location)), editor(service.status().engine, {}), diagnostic(std::move(warning)) {
        DiskSnapshot disk; std::string error;
        disk_readable = path && inspect_config_file(*path, disk, error);
        if (!error.empty()) diagnostic += "\n" + error;
        editor = SettingsEditor(service.status().engine, std::move(disk));
    }
};
void settings_layout(HWND window) {
    RECT rect{}; GetClientRect(window, &rect);
    const int w = MulDiv(rect.right, 96, static_cast<int>(GetDpiForWindow(window)));
    const int h = MulDiv(rect.bottom, 96, static_cast<int>(GetDpiForWindow(window)));
    move(window, IDC_CONTROLLER_LABEL, 16, 20, 90, 22);
    move(window, IDC_CONTROLLER, 112, 16, 180, 180);
    move(window, IDC_STATUS, 316, 20, w - 332, 22);
    move(window, IDC_ENABLED, 16, 52, 300, 24);
    move(window, IDC_BINDINGS, 16, 84, w - 32, std::max(50, h - 378));
    move(window, IDC_CHANGE, 16, h - 282, 140, 28);
    move(window, IDC_CLEAR, 168, h - 282, 140, 28);
    move(window, IDC_ANALOG_GROUP, 16, h - 244, w - 32, 128);
    move(window, IDC_STICK_PRESS_LABEL, 28, h - 210, 110, 22);
    move(window, IDC_STICK_PRESS, 142, h - 214, 98, 26);
    move(window, IDC_STICK_RELEASE_LABEL, 296, h - 210, 126, 22);
    move(window, IDC_STICK_RELEASE, 432, h - 214, 98, 26);
    move(window, IDC_TRIGGER_PRESS_LABEL, 28, h - 166, 110, 22);
    move(window, IDC_TRIGGER_PRESS, 142, h - 170, 98, 26);
    move(window, IDC_TRIGGER_RELEASE_LABEL, 296, h - 166, 126, 22);
    move(window, IDC_TRIGGER_RELEASE, 432, h - 170, 98, 26);
    move(window, IDC_POLL_LABEL, 16, h - 100, 124, 22);
    move(window, IDC_POLL, 142, h - 104, 98, 26);
    move(window, IDC_MS, 252, h - 100, 120, 22);
    move(window, IDC_NOTICE, 16, h - 72, w - 32, 24);
    move(window, IDC_DEFAULTS, 16, h - 40, 80, 28);
    move(window, IDC_RELOAD, 104, h - 40, 88, 28);
    move(window, IDC_RESUME, 200, h - 40, 88, 28);
    move(window, IDCANCEL, w - 192, h - 40, 80, 28);
    move(window, IDOK, w - 104, h - 40, 88, 28);
    const HWND list = GetDlgItem(window, IDC_BINDINGS);
    RECT client{}; GetClientRect(list, &client);
    ListView_SetColumnWidth(list, 0, (client.right * 58) / 100);
    ListView_SetColumnWidth(list, 1, std::max(40L, client.right - (client.right * 58) / 100 - px(window, 5)));
}
void update_bindings(HWND window, const SettingsEditor& editor) {
    const HWND list = GetDlgItem(window, IDC_BINDINGS);
    for (std::size_t i = 0; i < input_count; ++i) {
        auto name = widen(key_name(editor.editable.bindings[i]));
        ListView_SetItemText(list, static_cast<int>(i), 1, name.data());
    }
}
void populate(HWND window, Window& state) {
    state.loading = true;
    const auto& c = state.editor.editable;
    SendDlgItemMessageW(window, IDC_CONTROLLER, CB_SETCURSEL, c.controller.index, 0);
    CheckDlgButton(window, IDC_ENABLED, c.general.enabled ? BST_CHECKED : BST_UNCHECKED);
    set_text(window, IDC_STICK_PRESS, numeric(c.analog.stick_press_threshold));
    set_text(window, IDC_STICK_RELEASE, numeric(c.analog.stick_release_threshold));
    set_text(window, IDC_TRIGGER_PRESS, std::to_wstring(c.analog.trigger_press_threshold));
    set_text(window, IDC_TRIGGER_RELEASE, std::to_wstring(c.analog.trigger_release_threshold));
    set_text(window, IDC_POLL, std::to_wstring(c.general.polling_interval_ms));
    update_bindings(window, state.editor);
    state.loading = false;
    SetWindowTextW(window, L"ZeroPad - Settings");
}
void busy(HWND window, Window& state, bool value) {
    for (const int id : {IDC_CONTROLLER, IDC_ENABLED, IDC_BINDINGS, IDC_CHANGE, IDC_CLEAR,
         IDC_STICK_PRESS, IDC_STICK_RELEASE, IDC_TRIGGER_PRESS, IDC_TRIGGER_RELEASE,
         IDC_POLL, IDC_DEFAULTS, IDC_RELOAD, IDC_RESUME, IDOK})
        EnableWindow(GetDlgItem(window, id), !value && (id != IDOK || state.disk_readable));
}
bool read_candidate(HWND window, Window& state) {
    Config c = state.editor.editable;
    auto bad = [&](int id, const char* description) {
        error_box(window, description); SetFocus(GetDlgItem(window, id));
        SendDlgItemMessageW(window, id, EM_SETSEL, 0, -1); return false;
    };
    if (!read_number(text(window, IDC_STICK_PRESS), c.analog.stick_press_threshold)) return bad(IDC_STICK_PRESS, "Stick press must be a finite number (use a decimal point).");
    if (!read_number(text(window, IDC_STICK_RELEASE), c.analog.stick_release_threshold)) return bad(IDC_STICK_RELEASE, "Stick release must be a finite number.");
    if (!read_number(text(window, IDC_TRIGGER_PRESS), c.analog.trigger_press_threshold)) return bad(IDC_TRIGGER_PRESS, "Trigger press must be an integer.");
    if (!read_number(text(window, IDC_TRIGGER_RELEASE), c.analog.trigger_release_threshold)) return bad(IDC_TRIGGER_RELEASE, "Trigger release must be an integer.");
    if (!read_number(text(window, IDC_POLL), c.general.polling_interval_ms)) return bad(IDC_POLL, "Polling interval must be an integer from 1 to 100.");
    c.controller.index = static_cast<int>(SendDlgItemMessageW(window, IDC_CONTROLLER, CB_GETCURSEL, 0, 0));
    c.general.enabled = IsDlgButtonChecked(window, IDC_ENABLED) == BST_CHECKED;
    const auto error = validate_config(c);
    if (!error.empty()) {
        int id = IDC_STICK_PRESS;
        if (error.find("trigger") != std::string::npos) id = IDC_TRIGGER_PRESS;
        else if (error.find("polling") != std::string::npos) id = IDC_POLL;
        else if (error.find("index") != std::string::npos) id = IDC_CONTROLLER;
        return bad(id, error.c_str());
    }
    state.editor.editable = c;
    state.candidate = c;
    return true;
}
void begin_reload(HWND window, Window& state) {
    if (!state.path) { error_box(window, "Configuration location is unavailable."); return; }
    if (MessageBoxW(window, L"Reload the configuration file and discard all unsaved edits?", L"ZeroPad", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    std::string error;
    if (!inspect_config_file(*state.path, state.reload_disk, error) || !state.reload_disk.exists ||
        !parse_config(state.reload_disk.bytes, state.candidate, error)) {
        error_box(window, error.empty() ? "Configuration file is missing." : error); return;
    }
    state.operation = Window::Operation::Reload;
    state.deadline = Keyboard::Clock::now() + std::chrono::seconds(1);
    state.pause_request = state.engine.suspend(); busy(window, state, true);
    set_text(window, IDC_NOTICE, L"Releasing old keys before reload...");
}
void begin_save(HWND window, Window& state) {
    if (!state.path || !state.disk_readable || !read_candidate(window, state)) return;
    DiskSnapshot disk; std::string error; bool unchanged = false;
    if (!inspect_config_file(*state.path, disk, error)) { error_box(window, error); return; }
    if (!state.editor.check_save(state.engine.status().engine, disk, unchanged, error)) {
        error_box(window, error);
        begin_reload(window, state); return;
    }
    if (unchanged) { EndDialog(window, IDOK); return; }
    Config persisted;
    if (disk.exists && !parse_config(disk.bytes, persisted, error) &&
        MessageBoxW(window, L"The existing configuration is invalid. Save will replace it with these settings. Continue?", L"ZeroPad", MB_YESNO | MB_ICONWARNING) != IDYES) return;
    state.operation = Window::Operation::Save;
    state.deadline = Keyboard::Clock::now() + std::chrono::seconds(1);
    state.pause_request = state.engine.suspend(); busy(window, state, true);
    set_text(window, IDC_NOTICE, L"Releasing old keys before Save...");
}
void begin_capture(HWND window, Window& state) {
    const int selected = ListView_GetNextItem(GetDlgItem(window, IDC_BINDINGS), -1, LVNI_SELECTED);
    if (selected < 0) { error_box(window, "Select a controller input first."); return; }
    state.row = static_cast<std::size_t>(selected);
    state.operation = Window::Operation::Capture;
    state.deadline = Keyboard::Clock::now() + std::chrono::seconds(1);
    state.pause_request = state.engine.suspend(); busy(window, state, true);
    set_text(window, IDC_NOTICE, L"Releasing mapped keys before capture...");
}
void tick(HWND window, Window& state) {
    const auto service = state.engine.status();
    const auto& status = service.engine;
    const int selected = static_cast<int>(SendDlgItemMessageW(window, IDC_CONTROLLER, CB_GETCURSEL, 0, 0));
    XINPUT_STATE controller{};
    const bool connected = selected >= 0 && XInputGetState(static_cast<DWORD>(selected), &controller) == ERROR_SUCCESS;
    set_text(window, IDC_STATUS, connected ? L"Status: Connected" : L"Status: Disconnected");
    if (service.stopped) {
        set_text(window, IDC_NOTICE, L"Input engine stopped. Close ZeroPad and restart.");
        busy(window, state, true); return;
    }
    if (state.operation == Window::Operation::Idle) {
        if (status.resume_blocked) set_text(window, IDC_NOTICE, L"Mapping paused: release keyboard keys, then choose Resume.");
        else if (status.injection_failed) set_text(window, IDC_NOTICE, L"Input submission failed; pending releases will retry.");
        else if (status.suspended) set_text(window, IDC_NOTICE, L"Mapping paused; release keyboard keys or choose Resume.");
        else set_text(window, IDC_NOTICE, L"Edits take effect only after Save. Closing this window exits ZeroPad.");
        return;
    }
    const bool applying = state.operation == Window::Operation::ApplySave || state.operation == Window::Operation::ApplyReload;
    if (applying) {
        if (service.completed_request == state.request && !status.applying) {
            if (!service.error.empty() || status.config != state.candidate) {
                state.operation = Window::Operation::Idle; busy(window, state, false); EnableWindow(GetDlgItem(window, IDCANCEL), TRUE);
                error_box(window, "Configuration was saved/read but could not be activated. Mapping remains paused. Retry Save or Reload, or Resume the old mapping. " + service.error);
                return;
            }
            const bool saved = state.operation == Window::Operation::ApplySave;
            state.operation = Window::Operation::Idle; EnableWindow(GetDlgItem(window, IDCANCEL), TRUE);
            if (saved) { EndDialog(window, IDOK); return; }
            state.editor = SettingsEditor(status, state.reload_disk);
            state.disk_readable = true;
            populate(window, state); busy(window, state, false); state.engine.resume();
        } else if (Keyboard::Clock::now() >= state.deadline) {
            // Persistence may already have succeeded. Never describe this as a
            // complete save, and keep mapping paused until explicit recovery.
            state.operation = Window::Operation::Idle; busy(window, state, false); EnableWindow(GetDlgItem(window, IDCANCEL), TRUE);
            error_box(window, "Activation timed out. The file may already be saved; mapping stays paused. Retry Save or Reload, or close ZeroPad.");
        }
        return;
    }
    if (!status.safe || service.pause_acknowledged != state.pause_request) {
        if (Keyboard::Clock::now() >= state.deadline) {
            state.operation = Window::Operation::Idle; busy(window, state, false); state.engine.resume();
            error_box(window, "Could not safely release all mapped keys. Capture/Save was not started. Pending releases will retry; try again after recovery.");
        }
        return;
    }
    if (state.operation == Window::Operation::Capture) {
        state.operation = Window::Operation::Idle;
        struct ResumeOnExit { EngineService& service; ~ResumeOnExit() { service.resume(); } } resume{state.engine};
        BindingDialog binding{state.editor.editable.bindings[state.row], {}, nullptr};
        const auto result = DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_BINDING), window, binding_proc, reinterpret_cast<LPARAM>(&binding));
        if (result == IDOK && binding.selected) {
            state.editor.assign(state.row, *binding.selected);
            update_bindings(window, state.editor);
            SetWindowTextW(window, L"ZeroPad - Settings *");
        } else if (result == -1) error_box(window, "Cannot create the key assignment dialog.");
        busy(window, state, false); SetFocus(GetDlgItem(window, IDC_BINDINGS));
        return;
    }
    std::string error;
    if (state.operation == Window::Operation::Save && !state.editor.persist(*state.path, status, error)) {
        state.operation = Window::Operation::Idle; busy(window, state, false); state.engine.resume();
        error_box(window, "Configuration was not saved or applied. " + error); return;
    }
    state.request = state.engine.apply(state.candidate, status.revision);
    state.operation = state.operation == Window::Operation::Save ? Window::Operation::ApplySave : Window::Operation::ApplyReload;
    EnableWindow(GetDlgItem(window, IDCANCEL), FALSE);
    state.deadline = Keyboard::Clock::now() + std::chrono::seconds(1);
}
INT_PTR CALLBACK settings_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* state = reinterpret_cast<Window*>(GetWindowLongPtrW(window, DWLP_USER));
    if (message == WM_INITDIALOG) {
        state = reinterpret_cast<Window*>(lparam);
        SetWindowLongPtrW(window, DWLP_USER, lparam);
        update_font(window, state->font);
        for (int i = 1; i <= 4; ++i) {
            const auto label = L"Controller " + std::to_wstring(i);
            SendDlgItemMessageW(window, IDC_CONTROLLER, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        }
        const HWND list = GetDlgItem(window, IDC_BINDINGS);
        ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        LVCOLUMNW column{}; column.mask = LVCF_TEXT | LVCF_WIDTH; column.cx = 300;
        wchar_t input[] = L"Controller Input", keyboard[] = L"Keyboard";
        column.pszText = input; ListView_InsertColumn(list, 0, &column);
        column.pszText = keyboard; ListView_InsertColumn(list, 1, &column);
        for (std::size_t i = 0; i < input_count; ++i) {
            LVITEMW item{}; item.mask = LVIF_TEXT; item.iItem = static_cast<int>(i);
            item.pszText = const_cast<wchar_t*>(input_display_names[i].data());
            ListView_InsertItem(list, &item);
        }
        ListView_SetItemState(list, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        SetWindowSubclass(list, list_proc, 1, 0);
        for (const int id : {IDC_STICK_PRESS, IDC_STICK_RELEASE, IDC_TRIGGER_PRESS, IDC_TRIGGER_RELEASE, IDC_POLL})
            SendDlgItemMessageW(window, id, EM_SETLIMITTEXT, 63, 0);
        populate(window, *state);
        size_window(window, 740, 680); settings_layout(window);
        EnableWindow(GetDlgItem(window, IDOK), state->disk_readable);
        SetTimer(window, 1, 250, nullptr);
        if (!state->diagnostic.empty()) error_box(window, state->diagnostic);
        return TRUE;
    }
    if (!state) return FALSE;
    switch (message) {
    case WM_TIMER: tick(window, *state); return TRUE;
    case WM_SIZE: settings_layout(window); return TRUE;
    case WM_GETMINMAXINFO: {
        auto* limits = reinterpret_cast<MINMAXINFO*>(lparam);
        RECT r{0, 0, px(window, 620), px(window, 480)};
        AdjustWindowRectExForDpi(&r, static_cast<DWORD>(GetWindowLongPtrW(window, GWL_STYLE)), FALSE,
                                static_cast<DWORD>(GetWindowLongPtrW(window, GWL_EXSTYLE)), GetDpiForWindow(window));
        MONITORINFO monitor{sizeof(monitor)}; GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor);
        limits->ptMinTrackSize = {std::min(r.right - r.left, monitor.rcWork.right - monitor.rcWork.left),
                                  std::min(r.bottom - r.top, monitor.rcWork.bottom - monitor.rcWork.top)};
        return TRUE;
    }
    case WM_DPICHANGED: {
        const auto* r = reinterpret_cast<RECT*>(lparam);
        SetWindowPos(window, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER);
        update_font(window, state->font); settings_layout(window); return TRUE;
    }
    case WM_NOTIFY: {
        const auto* notification = reinterpret_cast<NMHDR*>(lparam);
        if (notification->idFrom == IDC_BINDINGS && notification->code == NM_DBLCLK && state->operation == Window::Operation::Idle)
            begin_capture(window, *state);
        break;
    }
    case WM_COMMAND:
        if (LOWORD(wparam) == IDCANCEL) {
            if (state->operation == Window::Operation::ApplySave || state->operation == Window::Operation::ApplyReload) return TRUE;
            state->editor.cancel(); EndDialog(window, IDCANCEL); return TRUE;
        }
        if (state->loading || state->operation != Window::Operation::Idle) return FALSE;
        switch (LOWORD(wparam)) {
        case IDOK: begin_save(window, *state); return TRUE;
        case IDC_CHANGE: begin_capture(window, *state); return TRUE;
        case IDC_CLEAR: {
            const int selected = ListView_GetNextItem(GetDlgItem(window, IDC_BINDINGS), -1, LVNI_SELECTED);
            if (selected >= 0) { state->editor.clear(static_cast<std::size_t>(selected)); update_bindings(window, state->editor); }
            break;
        }
        case IDC_DEFAULTS: state->editor.defaults(); populate(window, *state); break;
        case IDC_RELOAD: begin_reload(window, *state); return TRUE;
        case IDC_RESUME: state->engine.resume(); return TRUE;
        case IDC_CONTROLLER:
            if (HIWORD(wparam) != CBN_SELCHANGE) return FALSE;
            set_text(window, IDC_STATUS, L"Status: Checking");
            break;
        default:
            if (HIWORD(wparam) != EN_CHANGE && LOWORD(wparam) != IDC_ENABLED) return FALSE;
        }
        SetWindowTextW(window, L"ZeroPad - Settings *"); return TRUE;
    case WM_CLOSE:
        if (state->operation == Window::Operation::ApplySave || state->operation == Window::Operation::ApplyReload) return TRUE;
        state->editor.cancel(); EndDialog(window, IDCANCEL); return TRUE;
    case WM_QUERYENDSESSION: SetWindowLongPtrW(window, DWLP_MSGRESULT, TRUE); return TRUE;
    case WM_ENDSESSION: if (wparam) EndDialog(window, IDCANCEL); return TRUE;
    case WM_DESTROY:
        KillTimer(window, 1);
        if (state->font) { DeleteObject(state->font); state->font = nullptr; }
        return TRUE;
    }
    return FALSE;
}
}
int run_settings_window(EngineService& engine, const std::optional<std::filesystem::path>& path, const std::string& diagnostic) {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES};
    if (!InitCommonControlsEx(&controls)) return -1;
    Window state(engine, path, diagnostic);
    return static_cast<int>(DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_SETTINGS), nullptr, settings_proc, reinterpret_cast<LPARAM>(&state)));
}
} // namespace zeropad
