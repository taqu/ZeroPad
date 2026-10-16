#pragma once
#include "config.h"
#include "keyboard.h"
namespace zeropad {
// Configuration changes and polling share the engine's owning thread. A pending change
// blocks controller input until every old key has been successfully released.
class Runtime {
public:
    explicit Runtime(const Config& config = {}) noexcept;
    bool request_config(const Config& candidate, std::string& error);
    bool reload(const std::filesystem::path& path, std::string& error);
    bool tick(const Snapshot& snapshot, Keyboard::Clock::time_point now,
              KeyOutput output, void* context = nullptr) noexcept;
    bool release(Keyboard::Clock::time_point now, KeyOutput output, void* context = nullptr) noexcept;
    const Config& config() const noexcept { return active_; }
    bool transitioning() const noexcept { return pending_.has_value(); }
    bool held() const noexcept { return keyboard_.held(); }
    void suspend(bool suspended) noexcept;
private:
    Config active_;
    std::optional<Config> pending_;
    LogicalInputs logical_;
    Mapper mapper_;
    Keyboard keyboard_;
    bool suspended_ = false;
};
}
