#include "tray.h"
#include "single_instance.h"
#include <atomic>
#include <future>
#include <iostream>
#include <vector>
#include <cstdlib>
using namespace zeropad;
namespace {
void check(bool ok, const char* text) { if (!ok) { std::cerr << "FAIL: " << text << '\n'; std::exit(1); } }
std::vector<DWORD> calls;
DWORD fail_operation = ~0u;
std::wstring tooltip;
BOOL WINAPI notify(DWORD operation, PNOTIFYICONDATAW data) {
    calls.push_back(operation); tooltip = data->szTip;
    if (operation == NIM_SETVERSION) check(data->uVersion == NOTIFYICON_VERSION_4, "version 4 notification protocol");
    check(data->uID == 1, "stable notification identity");
    return operation == fail_operation ? FALSE : TRUE;
}
void tray() {
    TrayIcon icon(notify);
    check(icon.add(reinterpret_cast<HWND>(1), reinterpret_cast<HICON>(1), MappingMode::Running), "icon registration");
    check(calls == std::vector<DWORD>{NIM_ADD, NIM_SETVERSION}, "ADD followed by SETVERSION");
    check(icon.add(reinterpret_cast<HWND>(1), reinterpret_cast<HICON>(1), MappingMode::Disabled), "repeated add updates existing icon");
    check(calls.back() == NIM_MODIFY && tooltip == L"ZeroPad - Disabled", "no duplicate ADD and tooltip updated");
    check(icon.restore(MappingMode::Recovering), "Explorer recovery re-registers icon");
    check(calls[calls.size()-3] == NIM_DELETE && calls[calls.size()-2] == NIM_ADD && calls.back() == NIM_SETVERSION, "Explorer recovery resets version");
    check(icon.remove() && icon.remove(), "icon cleanup idempotent");
    fail_operation = NIM_ADD;
    check(!icon.add(reinterpret_cast<HWND>(1), reinterpret_cast<HICON>(1), MappingMode::Running) && !icon.registered(), "failed registration not claimed successful");
    fail_operation = NIM_SETVERSION;
    check(!icon.add(reinterpret_cast<HWND>(1), reinterpret_cast<HICON>(1), MappingMode::Running) && calls.back() == NIM_DELETE, "version failure removes unusable icon");
    fail_operation = ~0u;
}
void mutex_tests() {
    const auto name = L"Local\\ZeroPad.Test." + std::to_wstring(GetCurrentProcessId());
    {
        SingleInstance first(name.c_str()), second(name.c_str());
        check(first.result() == InstanceResult::First && second.result() == InstanceResult::Existing, "OS ownership distinguishes second instance");
    }
    { SingleInstance released(name.c_str()); check(released.result() == InstanceResult::First, "mutex handle resources released"); }
    HANDLE collision = CreateEventW(nullptr, FALSE, FALSE, name.c_str()); check(collision != nullptr, "create type collision fixture");
    { SingleInstance error(name.c_str()); check(error.result() == InstanceResult::Error && error.error() == ERROR_INVALID_HANDLE, "creation errors are not mistaken for existing instance"); }
    CloseHandle(collision);
}
std::atomic_bool block_request = false;
std::atomic_int opens = 0;
HANDLE gate = nullptr;
UINT open_message;
LRESULT CALLBACK endpoint_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == open_message) {
        ++opens;
        if (block_request) WaitForSingleObject(gate, 10000);
        return 1;
    }
    if (message == WM_CLOSE) { DestroyWindow(window); return 0; }
    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(window, message, wparam, lparam);
}
void ipc_tests() {
    const auto name = L"ZeroPad.Test.Endpoint." + std::to_wstring(GetCurrentProcessId());
    open_message = RegisterWindowMessageW(open_settings_message);
    gate = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    std::promise<HWND> created; auto future = created.get_future();
    std::thread endpoint([&] {
        WNDCLASSW type{}; type.lpfnWndProc = endpoint_proc; type.hInstance = GetModuleHandleW(nullptr); type.lpszClassName = name.c_str();
        RegisterClassW(&type);
        const HWND window = CreateWindowExW(0, name.c_str(), L"", 0, 0, 0, 0, 0, nullptr, nullptr, type.hInstance, nullptr);
        created.set_value(window);
        MSG message{}; while (GetMessageW(&message, nullptr, 0, 0) > 0) DispatchMessageW(&message);
        UnregisterClassW(name.c_str(), type.hInstance);
    });
    const HWND window = future.get(); check(window != nullptr, "isolated hidden IPC endpoint");
    std::wstring error;
    check(request_existing_settings(error, name.c_str(), 0, 1000) && opens == 1, "second launch sends only open settings request");
    const auto mutex = L"Local\\ZeroPad.ChildTest." + std::to_wstring(GetCurrentProcessId());
    SingleInstance owner(mutex.c_str());
    wchar_t executable[32768]{}; GetModuleFileNameW(nullptr, executable, 32768);
    std::wstring command = L"\"" + std::wstring(executable) + L"\" --child " + mutex + L" " + name;
    STARTUPINFOW startup{}; startup.cb = sizeof(startup); PROCESS_INFORMATION process{};
    check(CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process) != FALSE, "launch isolated second-instance test process");
    check(WaitForSingleObject(process.hProcess, 5000) == WAIT_OBJECT_0, "second process exits after bounded settings IPC");
    DWORD child_result = 1; GetExitCodeProcess(process.hProcess, &child_result);
    CloseHandle(process.hThread); CloseHandle(process.hProcess);
    check(child_result == 0 && opens == 2, "real second process sees ownership and requests Settings without an engine");
    block_request = true;
    check(!request_existing_settings(error, name.c_str(), 0, 25) && !error.empty(), "unresponsive endpoint uses bounded IPC");
    SetEvent(gate); PostMessageW(window, WM_CLOSE, 0, 0); endpoint.join(); CloseHandle(gate);
    check(!request_existing_settings(error, name.c_str(), 0, 25), "missing stale endpoint fails safely");
}
}
int wmain(int argc, wchar_t** argv) {
    if (argc == 4 && std::wstring_view(argv[1]) == L"--child") {
        SingleInstance instance(argv[2]); std::wstring error;
        return instance.result() == InstanceResult::Existing && request_existing_settings(error, argv[3], 0, 1000) ? 0 : 1;
    }
    tray(); mutex_tests(); ipc_tests(); std::cout << "Native lifecycle tests passed.\n";
}
