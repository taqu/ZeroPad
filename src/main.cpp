#include "settings_window.h"
#include "controller.h"
#define NOMINMAX
#include <Windows.h>
namespace {
struct EngineDevices { zeropad::Controller controller; };
zeropad::Snapshot poll(unsigned index, void* context) {
    auto& devices = *static_cast<EngineDevices*>(context);
    devices.controller.poll(index);
    return devices.controller.snapshot();
}
bool output(std::uint16_t key, bool pressed, void*) { return zeropad::send_key(key, pressed, nullptr); }
bool keys_down(void*) {
    // High bits represent current state, not the unreliable "pressed since"
    // bit. Include all keyboard keys so Tab/Enter and dialog activation keys
    // must also be released before mapping resumes.
    for (int key = VK_BACK; key < 255; ++key) if (GetAsyncKeyState(key) & 0x8000) return true;
    return false;
}
}
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    try {
        std::string diagnostic;
        const auto path = zeropad::default_config_path(diagnostic);
        zeropad::Config initial;
        if (path) initial = zeropad::startup_config(*path, diagnostic);
        EngineDevices devices;
        zeropad::EngineService engine(initial, poll, output, keys_down, &devices);
        const int result = zeropad::run_settings_window(engine, path, diagnostic);
        engine.stop();
        const auto status = engine.status();
        if (status.engine.cleanup_failed) {
            MessageBoxW(nullptr, L"ZeroPad could not release all submitted keys during shutdown.", L"ZeroPad", MB_OK | MB_ICONERROR);
            return 1;
        }
        if (result == -1 || status.worker_failed) {
            MessageBoxW(nullptr, L"ZeroPad could not open the settings window or the input worker stopped unexpectedly.", L"ZeroPad", MB_OK | MB_ICONERROR);
            return 1;
        }
        return 0;
    } catch (const std::exception&) {
        MessageBoxW(nullptr, L"ZeroPad could not initialize. No settings changes were requested.", L"ZeroPad", MB_OK | MB_ICONERROR);
        return 1;
    }
}
