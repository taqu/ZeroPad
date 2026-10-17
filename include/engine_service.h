#pragma once
#include "settings.h"
#include <condition_variable>
#include <mutex>
#include <thread>
#include <atomic>
namespace zeropad {
using PollController = Snapshot (*)(unsigned index, void* context);
using PhysicalKeysDown = bool (*)(void* context);
struct ServiceStatus {
    EngineStatus engine;
    std::uint64_t completed_request = 0;
    std::uint64_t pause_acknowledged = 0;
    std::string error;
    bool stopped = false;
    bool worker_failed = false;
};
// Exactly one worker owns Runtime. The mutex only protects commands/status;
// callbacks, file I/O and window messages never execute under this mutex.
class EngineService {
public:
    EngineService(const Config& initial, PollController poll, KeyOutput output,
                  PhysicalKeysDown keys_down, void* context = nullptr);
    ~EngineService();
    EngineService(const EngineService&) = delete;
    EngineService& operator=(const EngineService&) = delete;
    ServiceStatus status() const;
    // A bounded condition wait for non-UI clients and lifecycle tests. GUI
    // handlers use asynchronous status snapshots instead.
    bool wait_status(const std::function<bool(const ServiceStatus&)>& predicate, std::chrono::milliseconds timeout);
    std::uint64_t suspend();
    void resume();
    std::uint64_t apply(const Config& config, std::uint64_t revision);
    void stop();
    void request_stop();
    void runtime_enabled(bool enabled);
    void power(bool suspended);
    void session_locked(bool locked);
private:
    void run(Config initial);
    static bool guarded_output(std::uint16_t key, bool pressed, void* self);
    void environment_command(bool value, unsigned bit, std::optional<bool>& command);
    PollController poll_;
    KeyOutput output_;
    PhysicalKeysDown keys_down_;
    void* context_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    ServiceStatus status_;
    bool stopping_ = false;
    std::mutex output_mutex_;
    std::atomic_bool stop_requested_ = false;
    std::atomic_uint inhibit_ = 0;
    std::atomic_uint64_t command_epoch_ = 0, applied_epoch_ = 0;
    std::optional<bool> enabled_command_, power_command_, session_command_;
    bool power_pulse_ = false, session_pulse_ = false;
    std::optional<std::pair<bool, std::uint64_t>> pause_command_;
    std::uint64_t next_pause_ = 0;
    struct Apply { Config config; std::uint64_t revision, request; };
    std::optional<Apply> apply_command_;
    std::uint64_t next_request_ = 0;
    std::thread thread_;
};
}
