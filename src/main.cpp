#include "tray.h"
#include "single_instance.h"
#include "controller.h"
namespace {
struct EngineDevices { zeropad::Controller controller; };
bool interactive_desktop() {
    const HDESK input = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    if (!input) return false;
    wchar_t input_name[128]{}, worker_name[128]{}; DWORD needed = 0;
    const bool ok = GetUserObjectInformationW(input, UOI_NAME, input_name, sizeof(input_name), &needed) &&
        GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()), UOI_NAME, worker_name, sizeof(worker_name), &needed) &&
        std::wstring_view(input_name) == worker_name;
    CloseDesktop(input);
    return ok;
}
zeropad::Snapshot poll(unsigned index, void* context) {
    // Never retain controller hysteresis while the input desktop is elsewhere.
    if (!interactive_desktop()) return {};
    auto& devices = *static_cast<EngineDevices*>(context);
    devices.controller.poll(index);
    return devices.controller.snapshot();
}
bool output(std::uint16_t key, bool pressed, void*) {
    if (pressed && !interactive_desktop()) return false;
    return zeropad::send_key(key, pressed, nullptr);
}
bool keys_down(void*) {
    for (int key = VK_BACK; key < 255; ++key) if (GetAsyncKeyState(key) & 0x8000) return true;
    return false;
}
}
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    try {
        zeropad::SingleInstance instance;
        if (instance.result() == zeropad::InstanceResult::Error) {
            const auto message = L"Cannot create the session-local instance mutex. Windows error " + std::to_wstring(instance.error()) + L". No input engine was started.";
            MessageBoxW(nullptr, message.c_str(), L"ZeroPad", MB_OK | MB_ICONERROR); return 1;
        }
        if (instance.result() == zeropad::InstanceResult::Existing) {
            std::wstring error;
            if (!zeropad::request_existing_settings(error)) { MessageBoxW(nullptr, error.c_str(), L"ZeroPad", MB_OK | MB_ICONWARNING); return 1; }
            return 0;
        }
        std::string diagnostic;
        const auto path = zeropad::default_config_path(diagnostic);
        zeropad::Config initial;
        if (path) initial = zeropad::startup_config(*path, diagnostic);
        EngineDevices devices;
        zeropad::EngineService engine(initial, poll, output, keys_down, &devices);
        const int result = zeropad::run_tray_app(engine, path, diagnostic);
        engine.stop();
        const auto status = engine.status();
        if (status.engine.cleanup_failed) {
            MessageBoxW(nullptr, L"ZeroPad could not release all submitted keys during shutdown.", L"ZeroPad", MB_OK | MB_ICONERROR); return 1;
        }
        if (result || status.worker_failed) {
            MessageBoxW(nullptr, L"ZeroPad could not initialize or continue its tray/engine infrastructure. It stopped safely; diagnostics are available through debugger output.", L"ZeroPad", MB_OK | MB_ICONERROR);
            return 1;
        }
        return 0;
    } catch (const std::exception&) {
        MessageBoxW(nullptr, L"ZeroPad could not initialize. Initialized resources were cleaned up.", L"ZeroPad", MB_OK | MB_ICONERROR); return 1;
    }
}
