// Standalone sampling tool. Never calls SendInput or writes user settings.
#include "engine_service.h"
#include "controller.h"
#define NOMINMAX
#include <Windows.h>
#include <Psapi.h>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <vector>
using namespace zeropad;
namespace {
struct Samples {
    std::vector<double> intervals, processing;
    LARGE_INTEGER frequency{}, last_poll{}, sample{};
    unsigned polls = 0, connected = 0;
    enum Kind { Synthetic, Disconnected, Hardware } kind = Synthetic;
    Controller controller;
    Samples() { intervals.reserve(16384); processing.reserve(16384); QueryPerformanceFrequency(&frequency); }
    static Snapshot poll(unsigned index, void* pointer) {
        auto& s = *static_cast<Samples*>(pointer);
        LARGE_INTEGER now{}; QueryPerformanceCounter(&now);
        if (s.last_poll.QuadPart && s.intervals.size() < s.intervals.capacity())
            s.intervals.push_back(1000.0 * static_cast<double>(now.QuadPart - s.last_poll.QuadPart) / static_cast<double>(s.frequency.QuadPart));
        s.last_poll = now; ++s.polls;
        Snapshot snapshot;
        if (s.kind == Hardware) { s.controller.poll(index); snapshot = s.controller.snapshot(); }
        else if (s.kind == Synthetic) snapshot = {true, static_cast<std::uint16_t>((s.polls % 2) ? button_masks[0] : 0)};
        if (snapshot.connected) ++s.connected;
        QueryPerformanceCounter(&s.sample);
        return snapshot;
    }
    static bool output(std::uint16_t, bool, void* pointer) {
        auto& s = *static_cast<Samples*>(pointer); LARGE_INTEGER now{}; QueryPerformanceCounter(&now);
        if (s.processing.size() < s.processing.capacity())
            s.processing.push_back(1000.0 * static_cast<double>(now.QuadPart - s.sample.QuadPart) / static_cast<double>(s.frequency.QuadPart));
        return true;
    }
    static bool keys(void*) { return false; }
};
double cpu_ms() {
    FILETIME created{}, exit{}, kernel{}, user{}; GetProcessTimes(GetCurrentProcess(), &created, &exit, &kernel, &user);
    ULARGE_INTEGER k{}, u{}; k.LowPart = kernel.dwLowDateTime; k.HighPart = kernel.dwHighDateTime;
    u.LowPart = user.dwLowDateTime; u.HighPart = user.dwHighDateTime;
    return static_cast<double>(k.QuadPart + u.QuadPart) / 10000.0;
}
double percentile(std::vector<double> values, double fraction) {
    if (values.empty()) return 0;
    std::sort(values.begin(), values.end());
    return values[static_cast<std::size_t>((values.size() - 1) * fraction)];
}
void measure(const char* name, Samples::Kind kind, bool enabled, unsigned seconds) {
    Samples samples; samples.kind = kind;
    Config config; config.general.enabled = enabled;
    const auto start = Keyboard::Clock::now(); const double before = cpu_ms();
    EngineService service(config, Samples::poll, Samples::output, Samples::keys, &samples);
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
    PROCESS_MEMORY_COUNTERS_EX memory{}; memory.cb = sizeof(memory);
    GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory));
    const double after = cpu_ms(); const auto finish = Keyboard::Clock::now();
    service.stop();
    const auto duration = std::chrono::duration<double>(finish - start).count();
    std::cout << name << ",seconds=" << duration << ",cpu_one_core_percent=" << (after-before)/(duration*10)
              << ",working_set_bytes=" << memory.WorkingSetSize << ",private_bytes=" << memory.PrivateUsage
              << ",polls=" << samples.polls << ",connected_samples=" << samples.connected
              << ",poll_p50_ms=" << percentile(samples.intervals, .5) << ",poll_p95_ms=" << percentile(samples.intervals, .95)
              << ",poll_max_ms=" << percentile(samples.intervals, 1)
              << ",sample_to_mock_output_p50_ms=" << percentile(samples.processing, .5)
              << ",sample_to_mock_output_p95_ms=" << percentile(samples.processing, .95) << '\n';
}
}
int main(int argc, char** argv) {
    unsigned seconds = 5;
    if (argc > 1) { const int parsed = std::atoi(argv[1]); if (parsed < 1 || parsed > 60) return 1; seconds = static_cast<unsigned>(parsed); }
    const auto exe = std::filesystem::path(argv[0]).parent_path() / "ZeroPad.exe";
    std::error_code error; const auto size = std::filesystem::file_size(exe, error);
    std::cout << "Release_executable_bytes=" << (error ? 0 : size) << ",logical_processors=" << GetActiveProcessorCount(ALL_PROCESSOR_GROUPS) << '\n';
    measure("synthetic_connected", Samples::Synthetic, true, seconds);
    measure("synthetic_disconnected", Samples::Disconnected, true, seconds);
    measure("synthetic_disabled", Samples::Synthetic, false, seconds);
    measure("hardware_xinput_no_injection", Samples::Hardware, true, seconds);
    std::cout << "Memory/CPU describe this engine-only probe, not the full tray GUI. Processing times end at a mock callback, not SendInput or target consumption.\n";
}
