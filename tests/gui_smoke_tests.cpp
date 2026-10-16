// Opt-in native dialog smoke test. Uses a mock engine, temporary config,
// and window messages only: never SendInput or the real Known Folder file.
#include "settings_window.h"
#define NOMINMAX
#include <Windows.h>
#include <CommCtrl.h>
#include "../resources/resource.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <thread>
using namespace zeropad;
namespace {
void check(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(EXIT_FAILURE); }
}
HWND find_window(const wchar_t* title) {
    struct Search { const wchar_t* title; HWND found; } search{title, nullptr};
    EnumWindows([](HWND window, LPARAM pointer) -> BOOL {
        auto& s = *reinterpret_cast<Search*>(pointer);
        DWORD process = 0; GetWindowThreadProcessId(window, &process);
        wchar_t title[256]{}; GetWindowTextW(window, title, 256);
        const std::wstring_view actual(title);
        const std::wstring expected(s.title);
        if (process == GetCurrentProcessId() && (actual == expected || actual == expected + L" *")) { s.found = window; return FALSE; }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&search));
    return search.found;
}
HWND wait_window(const wchar_t* title) {
    const auto deadline = Keyboard::Clock::now() + std::chrono::seconds(15);
    do {
        if (const auto window = find_window(title)) return window;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (Keyboard::Clock::now() < deadline);
    check(false, "native dialog appeared before watchdog"); return nullptr;
}
LRESULT send(HWND window, UINT message, WPARAM first = 0, LPARAM second = 0) {
    DWORD_PTR result = 0;
    check(SendMessageTimeoutW(window, message, first, second, SMTO_ABORTIFHUNG | SMTO_BLOCK, 5000, &result) != 0, "UI message completed");
    return static_cast<LRESULT>(result);
}
void command(HWND window, int id) { PostMessageW(window, WM_COMMAND, static_cast<WPARAM>(id), 0); }
void binding_is(HWND window, const wchar_t* expected) {
    wchar_t value[64]{}; LVITEMW item{}; item.iSubItem = 1; item.pszText = value; item.cchTextMax = 64;
    send(GetDlgItem(window, IDC_BINDINGS), LVM_GETITEMTEXTW, 0, reinterpret_cast<LPARAM>(&item));
    check(std::wstring_view(value) == expected, "displayed binding matches model");
}
void layout_is_contained(HWND window) {
    RECT client{}; GetClientRect(window, &client);
    for (int id : {IDC_CONTROLLER, IDC_ENABLED, IDC_BINDINGS, IDC_CHANGE, IDC_CLEAR, IDC_STICK_PRESS,
         IDC_STICK_RELEASE, IDC_TRIGGER_PRESS, IDC_TRIGGER_RELEASE, IDC_POLL, IDC_DEFAULTS, IDCANCEL, IDOK}) {
        RECT control{}; GetWindowRect(GetDlgItem(window, id), &control);
        MapWindowPoints(nullptr, window, reinterpret_cast<POINT*>(&control), 2);
        check(control.left >= 0 && control.top >= 0 && control.right <= client.right && control.bottom <= client.bottom,
              "controls fit current client area");
    }
    RECT list{}, buttons{}; GetWindowRect(GetDlgItem(window, IDC_BINDINGS), &list); GetWindowRect(GetDlgItem(window, IDC_CHANGE), &buttons);
    check(list.bottom < buttons.top, "binding list does not overlap edit buttons");
}
Snapshot poll(unsigned, void*) { return {true}; }
bool output(std::uint16_t, bool, void*) { return true; }
bool keys(void*) { return false; }
}
int main() {
    const auto directory = std::filesystem::temp_directory_path() / ("zeropad-gui-test-" + std::to_string(GetCurrentProcessId()));
    check(std::filesystem::create_directory(directory), "create isolated GUI fixture directory");
    const auto path = directory / "config.toml";
    std::string error; check(save_config(path, Config{}, error), "write GUI fixture defaults");
    EngineService engine({}, poll, output, keys);
    std::thread cancel_test([&] {
        const HWND window = wait_window(L"ZeroPad - Settings");
        const HWND list = GetDlgItem(window, IDC_BINDINGS);
        check(send(list, LVM_GETITEMCOUNT) == static_cast<LRESULT>(input_count), "all 24 binding rows are present");
        binding_is(window, L"Space"); layout_is_contained(window);
        send(GetDlgItem(window, IDC_POLL), WM_SETTEXT, 0, reinterpret_cast<LPARAM>(L"0"));
        command(window, IDOK);
        const HWND validation = wait_window(L"ZeroPad");
        command(validation, IDOK);
        send(GetDlgItem(window, IDC_POLL), WM_SETTEXT, 0, reinterpret_cast<LPARAM>(L"4"));
        check(IsWindow(window), "invalid Save retains editor");
        command(window, IDC_CHANGE);
        const HWND capture = wait_window(L"Assign Keyboard Key");
        const HWND button = GetDlgItem(capture, IDC_CAPTURE);
        send(button, WM_KEYDOWN, VK_SHIFT, 0x36LL << 16);
        send(button, WM_KEYUP, VK_SHIFT, (0x36LL << 16) | (1LL << 30) | (1LL << 31));
        wchar_t proposed[128]{}; send(GetDlgItem(capture, IDC_SELECTED), WM_GETTEXT, 128, reinterpret_cast<LPARAM>(proposed));
        check(std::wstring_view(proposed) == L"Selected: RShift", "focused capture distinguishes right Shift");
        command(capture, IDCANCEL);
        // Wait until the modal dialog closes and the parent is enabled again.
        const auto deadline = Keyboard::Clock::now() + std::chrono::seconds(15);
        while (!IsWindowEnabled(window) && Keyboard::Clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        binding_is(window, L"Space");
        command(window, IDC_CHANGE);
        const HWND alternative = wait_window(L"Assign Keyboard Key");
        const HWND combo = GetDlgItem(alternative, IDC_KEYS);
        bool found_escape = false;
        const auto count = send(combo, CB_GETCOUNT);
        for (LRESULT i = 0; i < count; ++i) {
            if (send(combo, CB_GETITEMDATA, static_cast<WPARAM>(i)) != VK_ESCAPE) continue;
            send(combo, CB_SETCURSEL, static_cast<WPARAM>(i));
            send(alternative, WM_COMMAND, MAKEWPARAM(IDC_KEYS, CBN_SELCHANGE), reinterpret_cast<LPARAM>(combo));
            found_escape = true; break;
        }
        check(found_escape, "alternative selector includes Escape");
        command(alternative, IDOK);
        const auto alternative_deadline = Keyboard::Clock::now() + std::chrono::seconds(15);
        while (!IsWindowEnabled(window) && Keyboard::Clock::now() < alternative_deadline) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        binding_is(window, L"Escape");
        send(window, WM_COMMAND, IDC_CLEAR); binding_is(window, L"None");
        send(window, WM_COMMAND, IDC_DEFAULTS); binding_is(window, L"Space");
        command(window, IDCANCEL);
    });
    const int cancel_result = run_settings_window(engine, path, {});
    check(cancel_result == IDCANCEL, "Cancel closes primary window");
    cancel_test.join();
    Config persisted; check(load_config(path, persisted, error) == LoadStatus::Loaded && persisted == Config{}, "Cancel writes nothing");
    std::thread save_test([&] {
        const HWND window = wait_window(L"ZeroPad - Settings");
        send(window, WM_COMMAND, IDC_CLEAR); binding_is(window, L"None");
        command(window, IDOK);
    });
    check(run_settings_window(engine, path, {}) == IDOK, "Save persists, activates, and closes window");
    save_test.join();
    check(load_config(path, persisted, error) == LoadStatus::Loaded && persisted.bindings[0] == 0 && engine.status().engine.config == persisted,
          "saved and active GUI configuration match");
    engine.stop();
    check(!engine.status().engine.cleanup_failed, "mock engine cleanup");
    const auto resolved = std::filesystem::canonical(directory);
    check(resolved.parent_path() == std::filesystem::canonical(std::filesystem::temp_directory_path()) &&
          resolved.filename().wstring().starts_with(L"zeropad-gui-test-"), "safe fixture cleanup target");
    std::filesystem::remove_all(resolved);
    std::cout << "Native GUI smoke tests passed. Visual DPI and physical hardware checks remain manual.\n";
}
