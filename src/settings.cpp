#include "settings.h"
#include <array>
#include <charconv>
#include <cmath>
#include <fstream>

namespace zeropad {
std::optional<std::uint16_t> captured_key(unsigned vk, unsigned scan, bool extended, bool injected) {
    if (injected) return std::nullopt;
    if (vk == 0x10) vk = scan == 0x36 ? 0xA1 : scan == 0x2A ? 0xA0 : 0;
    if (vk == 0x11) vk = extended ? 0xA3 : 0xA2;
    if (vk == 0x12) vk = extended ? 0xA5 : 0xA4;
    if (vk == 0 || vk > 255 || key_name(static_cast<std::uint16_t>(vk)).empty()) return std::nullopt;
    return static_cast<std::uint16_t>(vk);
}
namespace {
template <typename Number> bool number(std::wstring_view text, Number& output) noexcept {
    if (text.empty() || text.size() >= 64) return false;
    std::array<char, 64> buffer{};
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] > 127) return false;
        buffer[i] = static_cast<char>(text[i]);
    }
    Number candidate{};
    const auto parsed = std::from_chars(buffer.data(), buffer.data() + text.size(), candidate);
    if (parsed.ec != std::errc{} || parsed.ptr != buffer.data() + text.size() || !std::isfinite(static_cast<double>(candidate))) return false;
    output = candidate;
    return true;
}
}
bool read_number(std::wstring_view text, int& output) noexcept { return number(text, output); }
bool read_number(std::wstring_view text, double& output) noexcept { return number(text, output); }
void EngineCore::update_mode() noexcept {
    status_.suspended = pauses_ != 0;
    if (pauses_ & static_cast<unsigned>(PauseReason::Stopping)) status_.mode = MappingMode::Stopping;
    else if (pauses_ & (static_cast<unsigned>(PauseReason::Recovery) | static_cast<unsigned>(PauseReason::Power) | static_cast<unsigned>(PauseReason::Session)))
        status_.mode = MappingMode::Recovering;
    else if (pauses_ & static_cast<unsigned>(PauseReason::Editor)) status_.mode = MappingMode::Capture;
    else status_.mode = status_.session_override.value_or(status_.config.general.enabled) ? MappingMode::Running : MappingMode::Disabled;
}
void EngineCore::block(PauseReason reason, bool enabled) noexcept {
    const auto bit = static_cast<unsigned>(reason);
    if (enabled) {
        if (pauses_ & bit) return;
        pauses_ |= bit;
    } else {
        if (!(pauses_ & bit)) return;
        pauses_ &= ~bit;
        pauses_ |= static_cast<unsigned>(PauseReason::Recovery);
    }
    runtime_.suspend(true);
    status_.safe = false;
    update_mode();
}
void EngineCore::suspend() noexcept {
    block(PauseReason::Editor, true);
    status_.safe = false;
    status_.resume_blocked = false;
    resuming_ = false;
}
void EngineCore::resume(Keyboard::Clock::time_point now) noexcept {
    if (!(pauses_ & static_cast<unsigned>(PauseReason::Editor))) return;
    resuming_ = true;
    status_.resume_blocked = false;
    resume_deadline_ = now + std::chrono::seconds(2);
}
void EngineCore::runtime_enabled(bool enabled) noexcept {
    if (status_.mode == MappingMode::Stopping) return;
    status_.session_override = enabled;
    runtime_.enabled_override(enabled);
    block(PauseReason::Recovery, true);
}
void EngineCore::power(bool suspended) noexcept { block(PauseReason::Power, suspended); }
void EngineCore::session_locked(bool locked) noexcept { block(PauseReason::Session, locked); }
void EngineCore::stopping() noexcept {
    resuming_ = false;
    block(PauseReason::Stopping, true);
}
bool EngineCore::apply(const Config& config, std::uint64_t expected_revision, std::string& error) {
    error = validate_config(config);
    if (!error.empty()) return false;
    if (status_.mode == MappingMode::Stopping) { error = "Application is stopping."; return false; }
    if (expected_revision != status_.revision) { error = "Active configuration changed; reload the editor before saving."; return false; }
    if (config == status_.config && !runtime_.transitioning() &&
        status_.session_override.value_or(config.general.enabled) == config.general.enabled) {
        status_.session_override.reset();
        runtime_.enabled_override(std::nullopt);
        return true; // No effective change, so held keys/hysteresis stay intact.
    }
    if (!status_.safe || !status_.suspended) { error = "Engine is not safely suspended; release pending keys and retry Save."; return false; }
    if (!runtime_.request_config(config, error)) return false;
    status_.session_override.reset();
    runtime_.enabled_override(std::nullopt);
    status_.applying = runtime_.transitioning();
    return true;
}
void EngineCore::step(const Snapshot& snapshot, bool physical_keys_down, Keyboard::Clock::time_point now,
                      KeyOutput output, void* context) noexcept {
    if (status_.connected && !snapshot.connected) block(PauseReason::Recovery, true);
    status_.connected = snapshot.connected;
    if (resuming_ && !runtime_.held() && !runtime_.transitioning()) {
        if (!physical_keys_down) {
            pauses_ &= ~static_cast<unsigned>(PauseReason::Editor);
            runtime_.suspend(pauses_ != 0);
            update_mode();
            resuming_ = false;
        } else if (now >= resume_deadline_) {
            resuming_ = false;
            status_.resume_blocked = true;
        }
    }
    const auto before = runtime_.config();
    status_.injection_failed = !runtime_.tick(snapshot, now, output, context);
    if (runtime_.config() != before) ++status_.revision;
    status_.config = runtime_.config();
    status_.applying = runtime_.transitioning();
    if ((pauses_ & static_cast<unsigned>(PauseReason::Recovery)) && !runtime_.held() && !runtime_.transitioning()) {
        pauses_ &= ~static_cast<unsigned>(PauseReason::Recovery);
        runtime_.suspend(pauses_ != 0);
        // Deliberately wait for the next fresh poll before any new key-down.
    }
    update_mode();
    status_.safe = status_.suspended && !runtime_.held() && !runtime_.transitioning();
}
bool EngineCore::release(Keyboard::Clock::time_point now, KeyOutput output, void* context) noexcept {
    stopping();
    runtime_.release(now, output, context);
    status_.cleanup_failed = runtime_.held();
    return !status_.cleanup_failed;
}
bool inspect_config_file(const std::filesystem::path& path, DiskSnapshot& output, std::string& error) {
    error.clear();
    std::error_code ec;
    DiskSnapshot candidate;
    candidate.exists = std::filesystem::exists(path, ec);
    if (ec) { error = "Cannot inspect configuration: " + ec.message(); return false; }
    if (candidate.exists) {
        std::ifstream file(path, std::ios::binary);
        if (!file) { error = "Cannot read configuration for change detection"; return false; }
        std::array<char, max_config_bytes + 1> bytes{};
        file.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (file.bad() || (!file.eof() && file.fail())) { error = "Cannot read complete configuration"; return false; }
        if (static_cast<std::size_t>(file.gcount()) > max_config_bytes) { error = "Configuration exceeds 64 KiB; preserve it and repair externally."; return false; }
        candidate.bytes.assign(bytes.data(), static_cast<std::size_t>(file.gcount()));
    }
    output = std::move(candidate);
    return true;
}
bool SettingsEditor::assign(std::size_t input, std::uint16_t key) {
    if (input >= input_count || key_name(key).empty()) return false;
    editable.bindings[input] = key;
    return true;
}
bool SettingsEditor::check_save(const EngineStatus& active, const DiskSnapshot& disk, bool& unchanged, std::string& error) const {
    unchanged = false;
    error = validate_config(editable);
    if (!error.empty()) return false;
    if (active.revision != revision_ || disk != disk_) {
        error = "Configuration changed outside this editor. Reload to discard edits, or cancel Save.";
        return false;
    }
    Config persisted;
    std::string ignored;
    unchanged = editable == active.config && disk.exists && parse_config(disk.bytes, persisted, ignored) && persisted == editable;
    return true;
}
bool SettingsEditor::persist(const std::filesystem::path& path, const EngineStatus& active,
                             std::string& error, const SaveFunction& save) {
    DiskSnapshot current;
    bool unchanged = false;
    if (!inspect_config_file(path, current, error) || !check_save(active, current, unchanged, error)) return false;
    if (unchanged) return true;
    if (!active.safe || !active.suspended) { error = "Engine has not released all keys; configuration was not saved."; return false; }
    if (!save(path, editable, error)) return false;
    // The canonical bytes are known; an external edit immediately after save
    // will be detected against this baseline on the next attempt.
    disk_ = {true, serialize_config(editable)};
    return true;
}
} // namespace zeropad
