#include "single_instance.h"
namespace zeropad {
SingleInstance::SingleInstance(const wchar_t* name) noexcept {
    SetLastError(ERROR_SUCCESS);
    handle_ = CreateMutexW(nullptr, FALSE, name);
    if (!handle_) { error_ = GetLastError(); return; }
    result_ = GetLastError() == ERROR_ALREADY_EXISTS ? InstanceResult::Existing : InstanceResult::First;
}
SingleInstance::~SingleInstance() { if (handle_) CloseHandle(handle_); }
bool request_existing_settings(std::wstring& error, const wchar_t* endpoint, DWORD startup_wait_ms, DWORD send_timeout_ms) {
    const UINT message = RegisterWindowMessageW(open_settings_message);
    if (!message) { error = L"Cannot register the settings request message."; return false; }
    HWND existing = nullptr;
    const auto deadline = GetTickCount64() + startup_wait_ms;
    do {
        existing = FindWindowW(endpoint, nullptr);
        if (existing) break;
        Sleep(25);
    } while (GetTickCount64() < deadline);
    if (!existing) { error = L"ZeroPad is already starting or running, but its settings endpoint is unavailable. No second engine was started."; return false; }
    DWORD process = 0; GetWindowThreadProcessId(existing, &process);
    AllowSetForegroundWindow(process);
    DWORD_PTR result = 0;
    if (!SendMessageTimeoutW(existing, message, 0, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, send_timeout_ms, &result) || result != 1) {
        error = L"The running ZeroPad instance did not acknowledge Settings within one second. No second engine was started.";
        return false;
    }
    return true;
}
}
