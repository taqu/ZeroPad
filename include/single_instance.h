#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <string>
namespace zeropad {
inline constexpr wchar_t host_class[] = L"ZeroPad.TrayHost.4.8D6A7225";
inline constexpr wchar_t instance_mutex[] = L"Local\\ZeroPad.Instance.8D6A7225";
inline constexpr wchar_t open_settings_message[] = L"ZeroPad.OpenSettings.8D6A7225";
enum class InstanceResult { First, Existing, Error };
class SingleInstance {
public:
    explicit SingleInstance(const wchar_t* name = instance_mutex) noexcept;
    ~SingleInstance();
    SingleInstance(const SingleInstance&) = delete;
    SingleInstance& operator=(const SingleInstance&) = delete;
    InstanceResult result() const noexcept { return result_; }
    DWORD error() const noexcept { return error_; }
private:
    HANDLE handle_ = nullptr;
    InstanceResult result_ = InstanceResult::Error;
    DWORD error_ = 0;
};
bool request_existing_settings(std::wstring& error, const wchar_t* endpoint = host_class,
                               DWORD startup_wait_ms = 2000, DWORD send_timeout_ms = 1000);
}
