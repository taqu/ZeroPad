#include "zeropad/controller.hpp"
#include "zeropad/keyboard.hpp"
#include "zeropad/mapper.hpp"

#define NOMINMAX
#include <Windows.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

namespace {
constexpr auto connected_poll_interval = std::chrono::milliseconds(4);
constexpr auto disconnected_poll_interval = std::chrono::milliseconds(250);
constexpr auto injection_retry_interval = std::chrono::milliseconds(100);
std::atomic_bool stop_requested = false;

BOOL WINAPI handle_console_control(DWORD) {
    stop_requested.store(true, std::memory_order_relaxed);
    return TRUE;
}
}

int main() {
    SetConsoleCtrlHandler(handle_console_control, TRUE);

    std::cout << "ZeroPad Phase 0\nWaiting for controller...\n";
    zeropad::Controller controller;
    zeropad::Mapper mapper;
    bool was_connected = false;
    bool failure_reported = false;
    auto retry_after = std::chrono::steady_clock::time_point::min();

    while (!stop_requested.load(std::memory_order_relaxed)) {
        controller.poll();
        const bool connected = controller.connected();
        if (connected != was_connected) {
            if (connected) {
                std::cout << "\nController connected.\nA -> Space mapping active.\n";
            } else {
                std::cout << "\nController disconnected.\nWaiting for controller...\n";
            }
            was_connected = connected;
        }

        const auto action = mapper.update(connected && controller.a_pressed());
        if (action.has_value() && std::chrono::steady_clock::now() >= retry_after) {
            const bool success = zeropad::set_space_key(*action);
            mapper.complete(success);
            if (!success && !failure_reported) {
                std::cerr << "SendInput failed; will retry while the desired state differs.\n";
                failure_reported = true;
            } else if (success) {
                failure_reported = false;
            }
            if (!success) {
                retry_after = std::chrono::steady_clock::now() + injection_retry_interval;
            }
        }

        std::this_thread::sleep_for(connected ? connected_poll_interval : disconnected_poll_interval);
    }

    if (const auto release = mapper.update(false); release.has_value()) {
        if (!zeropad::set_space_key(*release)) {
            std::cerr << "SendInput failed while releasing Space during shutdown.\n";
            return 1;
        }
        mapper.complete(true);
    }

    SetConsoleCtrlHandler(handle_console_control, FALSE);
    std::cout << "Exiting.\n";
    return 0;
}
