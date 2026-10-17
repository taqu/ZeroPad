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
    { std::lock_guard lock(mutex_); if (stopping_) return 0; request = ++next_pause_; pause_command_ = std::pair{true, request}; }
    wake_.notify_one();
    return request;
}
void EngineService::resume() {
    { std::lock_guard lock(mutex_); if (stopping_) return; pause_command_ = std::pair{false, ++next_pause_}; }
    wake_.notify_one();
}
std::uint64_t EngineService::apply(const Config& config, std::uint64_t revision) {
    std::uint64_t request;
    {
        std::lock_guard lock(mutex_);
        if (stopping_) return 0;
        request = ++next_request_;
        apply_command_ = Apply{config, revision, request};
    }
    wake_.notify_one();
    return request;
}
void EngineService::stop() {
    request_stop();
    if (thread_.joinable()) thread_.join();
}
void EngineService::request_stop() {
    { std::lock_guard gate(output_mutex_); stop_requested_.store(true); }
    { std::lock_guard lock(mutex_); stopping_ = true; }
    wake_.notify_all();
}
bool EngineService::guarded_output(std::uint16_t key, bool pressed, void* pointer) {
    auto& self = *static_cast<EngineService*>(pointer);
    std::lock_guard gate(self.output_mutex_);
    if (pressed && (self.stop_requested_.load() || self.inhibit_.load() != 0 ||
                    self.command_epoch_.load() != self.applied_epoch_.load())) return false;
    return self.output_(key, pressed, self.context_);
}
void EngineService::environment_command(bool value, unsigned bit, std::optional<bool>& command) {
    { std::lock_guard gate(output_mutex_);
      std::lock_guard lock(mutex_); if (stopping_) return;
      if (value) inhibit_.fetch_or(bit);
      ++command_epoch_; command = value;
      if (value && bit == 2u) power_pulse_ = true;
      if (value && bit == 4u) session_pulse_ = true; }
    wake_.notify_all();
}
void EngineService::runtime_enabled(bool enabled) {
    // The mailbox stores enabled; the output gate stores inhibited.
    { std::lock_guard gate(output_mutex_);
      std::lock_guard lock(mutex_); if (stopping_) return;
      if (!enabled) inhibit_.fetch_or(8u);
      ++command_epoch_; enabled_command_ = enabled; }
    wake_.notify_all();
}
void EngineService::power(bool suspended) { environment_command(suspended, 2u, power_command_); }
void EngineService::session_locked(bool locked) { environment_command(locked, 4u, session_command_); }
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
            std::optional<bool> enabled, power, session;
            bool power_pulse = false, session_pulse = false;
            std::uint64_t epoch;
            {
                std::lock_guard lock(mutex_);
                if (stopping_) break;
                pause.swap(pause_command_);
                apply.swap(apply_command_);
                enabled.swap(enabled_command_); power.swap(power_command_); session.swap(session_command_);
                power_pulse = power_pulse_; power_pulse_ = false;
                session_pulse = session_pulse_; session_pulse_ = false;
                epoch = command_epoch_.load();
            }
            const auto now = Keyboard::Clock::now();
            if (enabled) { engine.runtime_enabled(*enabled); if (*enabled) inhibit_.fetch_and(~8u); }
            if (power) { if (power_pulse) engine.power(true); engine.power(*power); if (!*power) inhibit_.fetch_and(~2u); }
            if (session) { if (session_pulse) engine.session_locked(true); engine.session_locked(*session); if (!*session) inhibit_.fetch_and(~4u); }
            if (pause) {
                if (pause->first) engine.suspend(); else engine.resume(now);
                pause_acknowledged = pause->second;
            }
            if (apply) {
                if (engine.apply(apply->config, apply->revision, error)) inhibit_.fetch_and(~8u);
                completed = apply->request;
            }
            // Newer environmental commands invalidate an in-flight poll even
            // if an older resume just cleared its output-inhibition bit.
            applied_epoch_.store(epoch);
            auto snapshot = poll_(static_cast<unsigned>(engine.status().config.controller.index), context_);
            engine.step(snapshot, engine.waiting_for_physical_release() && keys_down_(context_), now, guarded_output, this);
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
            wake_.wait_for(lock, delay, [&] { return stopping_ || pause_command_ || apply_command_ || enabled_command_ || power_command_ || session_command_; });
        }
    } catch (const std::exception& exception) { error = exception.what(); worker_failed = true; }
    for (int attempt = 0; attempt < 5; ++attempt) {
        if (attempt != 0) std::this_thread::sleep_for(Keyboard::retry_interval);
        if (engine.release(Keyboard::Clock::now(), guarded_output, this)) break;
    }
    std::lock_guard lock(mutex_);
    status_.engine = engine.status();
    status_.error = error;
    status_.stopped = true;
    status_.worker_failed = worker_failed;
    wake_.notify_all();
}
}
