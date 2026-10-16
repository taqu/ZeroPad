#pragma once
#include "runtime.h"
#include <functional>

namespace zeropad {
inline constexpr std::array<std::wstring_view, input_count> input_display_names {
    L"Button A", L"Button B", L"Button X", L"Button Y", L"D-Pad Up", L"D-Pad Down", L"D-Pad Left", L"D-Pad Right",
    L"Left Bumper (LB)", L"Right Bumper (RB)", L"Start", L"Back", L"Left Stick Click", L"Right Stick Click",
    L"Left Trigger (LT)", L"Right Trigger (RT)", L"Left Stick Up", L"Left Stick Down", L"Left Stick Left", L"Left Stick Right",
    L"Right Stick Up", L"Right Stick Down", L"Right Stick Left", L"Right Stick Right"
};
// A focused key-message conversion; scan code distinguishes Shift and the
// extended bit distinguishes Control/Alt. No hooks or character translation.
std::optional<std::uint16_t> captured_key(unsigned vk, unsigned scan, bool extended, bool injected = false);
bool read_number(std::wstring_view text, int& output) noexcept;
bool read_number(std::wstring_view text, double& output) noexcept;

struct EngineStatus {
    Config config;
    std::uint64_t revision = 0;
    bool suspended = false;
    bool safe = false;
    bool applying = false;
    bool resume_blocked = false;
    bool injection_failed = false;
    bool cleanup_failed = false;
};
// Deterministic engine coordination, also exercised by hardware-free tests.
class EngineCore {
public:
    explicit EngineCore(const Config& initial = {}) : runtime_(initial) { status_.config = initial; }
    void suspend() noexcept;
    void resume(Keyboard::Clock::time_point now) noexcept;
    bool apply(const Config& config, std::uint64_t expected_revision, std::string& error);
    void step(const Snapshot& snapshot, bool physical_keys_down, Keyboard::Clock::time_point now,
              KeyOutput output, void* context = nullptr) noexcept;
    bool release(Keyboard::Clock::time_point now, KeyOutput output, void* context = nullptr) noexcept;
    const EngineStatus& status() const noexcept { return status_; }
    bool waiting_for_physical_release() const noexcept { return resuming_; }
private:
    Runtime runtime_;
    EngineStatus status_;
    bool resuming_ = false;
    Keyboard::Clock::time_point resume_deadline_{};
};

struct DiskSnapshot {
    bool exists = false;
    std::string bytes;
    bool operator==(const DiskSnapshot&) const = default;
};
bool inspect_config_file(const std::filesystem::path& path, DiskSnapshot& output, std::string& error);
class SettingsEditor {
public:
    SettingsEditor(const EngineStatus& status, DiskSnapshot disk)
        : editable(status.config), original_(status.config), revision_(status.revision), disk_(std::move(disk)) {}
    Config editable;
    void defaults() noexcept { editable = Config{}; }
    void cancel() noexcept { editable = original_; }
    void clear(std::size_t input) noexcept { if (input < input_count) editable.bindings[input] = 0; }
    bool assign(std::size_t input, std::uint16_t key);
    bool modified() const noexcept { return editable != original_; }
    std::uint64_t revision() const noexcept { return revision_; }
    bool check_save(const EngineStatus& active, const DiskSnapshot& disk, bool& unchanged, std::string& error) const;
    using SaveFunction = std::function<bool(const std::filesystem::path&, const Config&, std::string&)>;
    // Call only after the engine acknowledges a safe suspension. Persistence
    // precedes activation. On activation failure the new file remains saved,
    // the engine remains suspended, and Retry Save can activate it safely.
    bool persist(const std::filesystem::path& path, const EngineStatus& active,
                 std::string& error, const SaveFunction& save = save_config);
private:
    Config original_;
    std::uint64_t revision_;
    DiskSnapshot disk_;
};
} // namespace zeropad
