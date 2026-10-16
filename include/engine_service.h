#pragma once
#include "settings.h"
#include <condition_variable>
#include <mutex>
#include <thread>
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
private:
    void run(Config initial);
    PollController poll_;
    KeyOutput output_;
    PhysicalKeysDown keys_down_;
    void* context_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    ServiceStatus status_;
    bool stopping_ = false;
    std::optional<std::pair<bool, std::uint64_t>> pause_command_;
    std::uint64_t next_pause_ = 0;
    struct Apply { Config config; std::uint64_t revision, request; };
    std::optional<Apply> apply_command_;
    std::uint64_t next_request_ = 0;
    std::thread thread_;
};
}
