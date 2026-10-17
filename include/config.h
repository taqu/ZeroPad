#pragma once
#include "mapper.h"
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace zeropad {
struct GeneralConfig {
    bool enabled = true;
    int polling_interval_ms = 4;
    bool operator==(const GeneralConfig&) const = default;
};
struct ControllerConfig {
    int index = 0;
    bool operator==(const ControllerConfig&) const = default;
};
struct Config {
    static constexpr int schema_version = 1;
    GeneralConfig general;
    ControllerConfig controller;
    AnalogConfig analog;
    Mapping bindings = default_mapping;
    bool operator==(const Config&) const = default;
};
inline constexpr std::size_t max_config_bytes = 64 * 1024;
inline constexpr std::array<std::string_view, input_count> input_names {
    "A", "B", "X", "Y", "DPAD_UP", "DPAD_DOWN", "DPAD_LEFT", "DPAD_RIGHT",
    "LB", "RB", "START", "BACK", "LS_BUTTON", "RS_BUTTON", "LT", "RT",
    "LS_UP", "LS_DOWN", "LS_LEFT", "LS_RIGHT", "RS_UP", "RS_DOWN", "RS_LEFT", "RS_RIGHT"
};
std::optional<std::uint16_t> parse_key_name(std::string_view name) noexcept;
std::string key_name(std::uint16_t key);
// Empty error means valid. Parsing never changes output on failure.
std::string validate_config(const Config& config);
bool parse_config(std::string_view text, Config& output, std::string& error);
std::string serialize_config(const Config& config);
enum class LoadStatus { Loaded, Missing, Error };
LoadStatus load_config(const std::filesystem::path& path, Config& output, std::string& error);
bool save_config(const std::filesystem::path& path, const Config& config, std::string& error);
std::optional<std::filesystem::path> default_config_path(std::string& error);
// Missing files are persisted if possible; invalid existing files are preserved.
Config startup_config(const std::filesystem::path& path, std::string& diagnostic);
} // namespace zeropad
