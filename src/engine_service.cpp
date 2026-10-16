#include "engine_service.h"
namespace zeropad {
EngineService::EngineService(const Config& initial, PollController poll, KeyOutput output,
                             PhysicalKeysDown keys_down, void* context)
    : poll_(poll), output_(output), keys_down_(keys_down), context_(context) {
    status_.engine.config = initial;
    thread_ = std::thread(&EngineService::run, this, initial);
}
EngineService::~EngineService() { stop(); }
ServiceStatus EngineService::status() const { std::lock_guard lock(mutex_); return status_; }
bool EngineService::wait_status(const std::function<bool(const ServiceStatus&)>& predicate, std::chrono::milliseconds timeout) {
    std::unique_lock lock(mutex_);
    return wake_.wait_for(lock, timeout, [&] { return predicate(status_); });
}
std::uint64_t EngineService::suspend() {
    std::uint64_t request;
    { std::lock_guard lock(mutex_); request = ++next_pause_; pause_command_ = std::pair{true, request}; }
    wake_.notify_one();
    return request;
}
void EngineService::resume() {
    { std::lock_guard lock(mutex_); pause_command_ = std::pair{false, ++next_pause_}; }
    wake_.notify_one();
}
std::uint64_t EngineService::apply(const Config& config, std::uint64_t revision) {
    std::uint64_t request;
    {
        std::lock_guard lock(mutex_);
        request = ++next_request_;
        apply_command_ = Apply{config, revision, request};
    }
    wake_.notify_one();
    return request;
}
void EngineService::stop() {
    { std::lock_guard lock(mutex_); stopping_ = true; }
    wake_.notify_one();
    if (thread_.joinable()) thread_.join();
}
void EngineService::run(Config initial) {
    EngineCore engine(initial);
    std::uint64_t completed = 0;
    std::uint64_t pause_acknowledged = 0;
    std::string error;
    bool worker_failed = false;
    try {
        for (;;) {
            std::optional<std::pair<bool, std::uint64_t>> pause;
            std::optional<Apply> apply;
            {
                std::lock_guard lock(mutex_);
                if (stopping_) break;
                pause.swap(pause_command_);
                apply.swap(apply_command_);
            }
            const auto now = Keyboard::Clock::now();
            if (pause) {
                if (pause->first) engine.suspend(); else engine.resume(now);
                pause_acknowledged = pause->second;
            }
            if (apply) {
                engine.apply(apply->config, apply->revision, error);
                completed = apply->request;
            }
            auto snapshot = poll_(static_cast<unsigned>(engine.status().config.controller.index), context_);
            engine.step(snapshot, engine.waiting_for_physical_release() && keys_down_(context_), now, output_, context_);
            {
                std::lock_guard lock(mutex_);
                status_.engine = engine.status();
                status_.completed_request = completed;
                status_.pause_acknowledged = pause_acknowledged;
                status_.error = error;
            }
            wake_.notify_all();
            const auto delay = std::chrono::milliseconds(engine.status().suspended ? 25 :
                snapshot.connected ? engine.status().config.general.polling_interval_ms : 250);
            std::unique_lock lock(mutex_);
            wake_.wait_for(lock, delay, [&] { return stopping_ || pause_command_.has_value() || apply_command_.has_value(); });
        }
    } catch (const std::exception& exception) { error = exception.what(); worker_failed = true; }
    for (int attempt = 0; attempt < 5; ++attempt) {
        if (attempt != 0) std::this_thread::sleep_for(Keyboard::retry_interval);
        if (engine.release(Keyboard::Clock::now(), output_, context_)) break;
    }
    std::lock_guard lock(mutex_);
    status_.engine = engine.status();
    status_.error = error;
    status_.stopped = true;
    status_.worker_failed = worker_failed;
    wake_.notify_all();
}
}
