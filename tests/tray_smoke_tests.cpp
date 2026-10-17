#include "tray.h"
#include "single_instance.h"
#include "../resources/resource.h"
#include <CommCtrl.h>
#include <Wtsapi32.h>
#include <Psapi.h>
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <thread>
using namespace zeropad;
namespace {
void check(bool ok, const char* message) { if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); } }
struct Mock {
    std::atomic_bool held = false, connected = true;
    static Snapshot poll(unsigned, void* pointer) { return {static_cast<Mock*>(pointer)->connected.load(), button_masks[0]}; }
    static bool output(std::uint16_t, bool down, void* pointer) { static_cast<Mock*>(pointer)->held = down; return true; }
    static bool keys(void*) { return false; }
};
HWND locate(const wchar_t* wanted) {
    struct Search { const wchar_t* text; HWND found = nullptr; } search{wanted};
    EnumWindows([](HWND window, LPARAM pointer) -> BOOL {
        auto& s = *reinterpret_cast<Search*>(pointer); DWORD process = 0; GetWindowThreadProcessId(window, &process);
        wchar_t title[128]{}, type[128]{}; GetWindowTextW(window, title, 128); GetClassNameW(window, type, 128);
        if (process == GetCurrentProcessId() && (std::wstring_view(type) == s.text || std::wstring_view(title).starts_with(s.text))) {
            s.found = window; return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&search));
    return search.found;
}
HWND wait_window(const wchar_t* title, bool exists = true) {
    const auto deadline = Keyboard::Clock::now() + std::chrono::seconds(15);
    do {
        const auto window = locate(title);
        if ((window != nullptr) == exists) return window;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (Keyboard::Clock::now() < deadline);
    check(false, "window lifecycle watchdog"); return nullptr;
}
void send(HWND window, UINT message, WPARAM first = 0, LPARAM second = 0) {
    DWORD_PTR result = 0;
    check(SendMessageTimeoutW(window, message, first, second, SMTO_BLOCK | SMTO_ABORTIFHUNG, 2000, &result) != 0, "host accepts bounded notification");
}
double cpu_ms() {
    FILETIME c{}, e{}, k{}, u{}; GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u);
    ULARGE_INTEGER kernel{}, user{}; kernel.LowPart = k.dwLowDateTime; kernel.HighPart = k.dwHighDateTime;
    user.LowPart = u.dwLowDateTime; user.HighPart = u.dwHighDateTime;
    return static_cast<double>(kernel.QuadPart + user.QuadPart) / 10000.0;
}
void metrics(const char* scenario) {
    const double before = cpu_ms(); const auto start = Keyboard::Clock::now();
    std::this_thread::sleep_for(std::chrono::seconds(3));
    PROCESS_MEMORY_COUNTERS_EX memory{}; memory.cb = sizeof(memory);
    GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory));
    const auto seconds = std::chrono::duration<double>(Keyboard::Clock::now() - start).count();
    std::cout << scenario << ",seconds=" << seconds << ",cpu_one_core_percent=" << (cpu_ms() - before)/(seconds*10)
              << ",working_set_bytes=" << memory.WorkingSetSize << ",private_bytes=" << memory.PrivateUsage << '\n';
}
}
int main() {
    const auto folder = std::filesystem::temp_directory_path() / ("zeropad-tray-test-" + std::to_string(GetCurrentProcessId()));
    check(std::filesystem::create_directory(folder), "isolated tray fixture");
    const auto path = folder / "config.toml"; std::string error;
    check(save_config(path, Config{}, error), "default fixture persisted");
    Mock mock; EngineService engine({}, Mock::poll, Mock::output, Mock::keys, &mock);
    std::thread automation([&] {
        const HWND host = wait_window(host_class);
        HWND settings = wait_window(L"ZeroPad - Settings");
        PostMessageW(settings, WM_COMMAND, IDCANCEL, 0); wait_window(L"ZeroPad - Settings", false);
        check(engine.wait_status([&](const auto&) { return mock.held.load(); }, std::chrono::seconds(10)), "closing Settings leaves mapping held and engine alive");
        metrics("tray_harness_connected_settings_closed");
        const UINT open = RegisterWindowMessageW(open_settings_message);
        for (int i = 0; i < 3; ++i) {
            send(host, open); settings = wait_window(L"ZeroPad - Settings");
            send(host, open); send(host, WM_NULL);
            check(locate(L"ZeroPad - Settings") == settings, "repeated Settings request focuses existing editor");
            PostMessageW(settings, WM_COMMAND, IDCANCEL, 0); wait_window(L"ZeroPad - Settings", false);
            check(!engine.status().stopped && mock.held, "repeated close leaves engine running");
        }
        send(host, WM_COMMAND, tray_toggle);
        check(engine.wait_status([](const auto& s) { return s.engine.mode == MappingMode::Disabled; }, std::chrono::seconds(10)), "tray Disable acknowledged after release");
        check(!mock.held && engine.status().engine.config.general.enabled, "runtime disable does not edit startup configuration");
        send(host, WM_POWERBROADCAST, PBT_APMSUSPEND);
        check(engine.wait_status([](const auto& s) { return s.engine.mode == MappingMode::Recovering && s.engine.safe; }, std::chrono::seconds(10)), "power suspend releases mapping");
        send(host, WM_POWERBROADCAST, PBT_APMRESUMEAUTOMATIC); send(host, WM_POWERBROADCAST, PBT_APMRESUMESUSPEND);
        check(engine.wait_status([](const auto& s) { return s.engine.mode == MappingMode::Disabled; }, std::chrono::seconds(10)), "repeated resume preserves disabled override");
        send(host, WM_COMMAND, tray_toggle);
        check(engine.wait_status([&](const auto& s) { return s.engine.mode == MappingMode::Running && mock.held; }, std::chrono::seconds(10)), "tray Enable resumes fresh input");
        send(host, WM_WTSSESSION_CHANGE, WTS_SESSION_LOCK);
        check(engine.wait_status([&](const auto& s) { return s.engine.safe && !mock.held; }, std::chrono::seconds(10)), "session lock releases mock held key");
        send(host, WM_WTSSESSION_CHANGE, WTS_SESSION_UNLOCK);
        check(engine.wait_status([&](const auto& s) { return s.engine.mode == MappingMode::Running && mock.held; }, std::chrono::seconds(10)), "unlock resumes safely");
        send(host, RegisterWindowMessageW(L"TaskbarCreated")); send(host, RegisterWindowMessageW(L"TaskbarCreated"));
        check(!engine.status().stopped && mock.held, "Explorer notification recovery keeps one engine and held state");
        mock.connected = false;
        check(engine.wait_status([&](const auto& s) { return !s.engine.connected && !mock.held; }, std::chrono::seconds(10)), "disconnect releases held mapping");
        metrics("tray_harness_disconnected_settings_closed");
        mock.connected = true;
        check(engine.wait_status([&](const auto& s) { return s.engine.connected && mock.held; }, std::chrono::seconds(10)), "reconnect uses fresh state");
        send(host, open); settings = wait_window(L"ZeroPad - Settings");
        PostMessageW(settings, WM_COMMAND, IDC_CHANGE, 0); wait_window(L"Assign Keyboard Key");
        PostMessageW(host, WM_COMMAND, tray_exit, 0);
    });
    check(run_tray_app(engine, path, {}) == 0, "central tray shutdown completes during capture");
    automation.join(); engine.stop();
    check(engine.status().stopped && !engine.status().engine.cleanup_failed && !mock.held, "worker stops and releases before tray resources are destroyed");
    check(!locate(host_class) && !locate(L"Assign Keyboard Key"), "host and capture windows cleaned up");
    Config persisted; check(load_config(path, persisted, error) == LoadStatus::Loaded && persisted == Config{}, "tray session actions did not write config");
    const auto resolved = std::filesystem::canonical(folder);
    check(resolved.parent_path() == std::filesystem::canonical(std::filesystem::temp_directory_path()) && resolved.filename().wstring().starts_with(L"zeropad-tray-test-"), "safe fixture cleanup");
    std::filesystem::remove_all(resolved);
    std::cout << "Tray GUI lifecycle smoke tests passed (mocked controller/output). Real sleep and Explorer restarts remain manual.\n";
}
