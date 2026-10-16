#include "runtime.h"
namespace zeropad {
Runtime::Runtime(const Config& config) noexcept
    : active_(config), logical_(config.analog), mapper_(config.bindings) {}
bool Runtime::request_config(const Config& candidate, std::string& error) {
    error = validate_config(candidate);
    if (!error.empty()) return false;
    if (candidate == active_ && !pending_) return true;
    pending_ = candidate;
    mapper_.release_all();
    return true;
}
bool Runtime::reload(const std::filesystem::path& path, std::string& error) {
    Config candidate;
    if (load_config(path, candidate, error) != LoadStatus::Loaded) {
        if (error.empty()) error = "Configuration file is missing";
        return false;
    }
    return request_config(candidate, error);
}
bool Runtime::tick(const Snapshot& snapshot, Keyboard::Clock::time_point now,
                   KeyOutput output, void* context) noexcept {
    if (pending_) {
        mapper_.release_all();
        const bool success = keyboard_.reconcile(mapper_, now, output, context);
        if (keyboard_.held()) return success;
        active_ = *pending_;
        pending_.reset();
        logical_ = LogicalInputs(active_.analog);
        mapper_ = Mapper(active_.bindings);
        // The caller must poll again, now using the new controller index.
        return success;
    }
    mapper_.update(logical_.update(active_.general.enabled && !suspended_ ? snapshot : Snapshot{}));
    return keyboard_.reconcile(mapper_, now, output, context);
}
bool Runtime::release(Keyboard::Clock::time_point now, KeyOutput output, void* context) noexcept {
    pending_.reset();
    mapper_.release_all();
    return keyboard_.reconcile(mapper_, now, output, context);
}
void Runtime::suspend(bool suspended) noexcept {
    if (suspended_ == suspended) return;
    suspended_ = suspended;
    logical_ = LogicalInputs(active_.analog);
    if (suspended) mapper_.release_all();
}
}
