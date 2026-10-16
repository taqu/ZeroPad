#include "config.h"
#include "runtime.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif
using namespace zeropad;
namespace {
void check(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(EXIT_FAILURE); }
}
Config parse(std::string_view text) {
    Config c; std::string error;
    if (!parse_config(text, c, error)) { std::cerr << error << '\n'; check(false, "valid configuration accepted"); }
    return c;
}
void reject(std::string_view text) {
    Config c; c.bindings[0] = 'T'; const auto original = c;
    std::string error;
    check(!parse_config(text, c, error), "invalid configuration rejected");
    check(!error.empty() && c == original, "failure diagnostic and no partial application");
}
void parsing() {
    Config defaults;
    check(validate_config(defaults).empty(), "default validation");
    check(defaults.bindings == default_mapping && defaults.bindings[0] == 0x20, "phase 1 defaults");
    check(defaults.analog == AnalogConfig{} && input_names.size() == input_count, "all logical inputs and analog defaults");
    check(parse("version = 1") == defaults, "missing optional sections default");
    check(parse("version = +1\n[general]\npolling_interval_ms = 0x4") == defaults, "checked integer conversion");
    check(parse("\xEF\xBB\xBFversion = 1 # UTF-8 comment: \xF0\x9F\x98\x80\n") == defaults, "UTF-8 BOM and comment");
    check(parse("version = 1\n[bindings]\nA = 'None'").bindings[0] == 0, "explicit None and literal string");
    for (const auto* bad : {
        "", "# empty", "[general]\nenabled = true", "version = 2", "version = 1.0",
        "version = true", "version = 1\n[unknown]", "version = 1\nunknown = 2",
        "version = 1\n[general]\nenabld = true", "version = 1\nversion = 1",
        "version = 1\n[general]\nenabled = true\nenabled = false",
        "version = 1\n[general]\n[general]", "version = 1\n[bindings]\nA = 32",
        "version = 1\n[bindings]\nA = \"space\"", "version = 1\n[bindings]\nAA = \"Space\"",
        "version = 1\n[general]\nenabled = 1", "version = 1\n[general]\npolling_interval_ms = 4.0",
        "version = 1\n[controller]\nindex = \"0\"", "version = 1\n[analog]\ntrigger_press_threshold = 40.0",
        "version = 1\n[general", "version = 1\n[bindings]\nA = \"Space",
        "version = 1\n[bindings]\nA = \"Space\" trailing", "version = 1\n[bindings]\nA = \"Ctrl+C\"",
        "version = 1\n[bindings]\nA = \"\\uD800\"", "version = 1\n[bindings]\nA = \"\\U00110000\"",
        "version = 1\n[general]\nenabled = [[[[[[true]]]]]]", "version = 1\n[general.deep.nested]",
        "version = 1\n[general]\nenabled = {x={y=true}}", "version = 1\n[[bindings]]",
        "version = 1\n[bindings]\nA = \"\"\"Space\"\"\"", "version = 1\n[general]\nenabled = tru",
        "version = 1\n[general]\npolling_interval_ms = 999999999999999999999999999999",
        "version = 1\n[analog]\nstick_press_threshold = 1e999", "version = 1\n[analog]\nstick_press_threshold = 1e-999"
    }) reject(bad);
    reject(std::string(max_config_bytes + 1, '#'));
    std::string nul = "version = 1\n# comment"; nul += '\0'; reject(nul);
    for (const auto* bytes : {"\xC0\xAF", "\xED\xA0\x80", "\xF4\x90\x80\x80", "\xFF", "\xE2\x82", "\x80"})
        reject(std::string("version = 1\n# ") + bytes);
    std::string maximum = "version = 1\n#"; maximum.resize(max_config_bytes, 'x');
    check(parse(maximum) == defaults, "64 KiB boundary accepted");
}
void numeric_validation() {
    for (int value : {1, 100}) {
        auto c = parse("version = 1\n[general]\npolling_interval_ms = " + std::to_string(value));
        check(c.general.polling_interval_ms == value, "poll boundaries");
    }
    for (int value : {-1, 0, 101}) reject("version = 1\n[general]\npolling_interval_ms = " + std::to_string(value));
    for (int value : {0, 3}) check(parse("version = 1\n[controller]\nindex = " + std::to_string(value)).controller.index == value, "index boundaries");
    for (int value : {-1, 4}) reject("version = 1\n[controller]\nindex = " + std::to_string(value));
    for (const auto* fields : {
        "trigger_press_threshold = 256", "trigger_release_threshold = -1",
        "trigger_release_threshold = 40", "trigger_release_threshold = 41", "trigger_press_threshold = -1",
        "stick_press_threshold = 1.01", "stick_release_threshold = -0.01", "stick_release_threshold = 0.25",
        "stick_release_threshold = 0.3", "stick_press_threshold = -1.0", "stick_press_threshold = nan",
        "stick_press_threshold = +inf", "stick_press_threshold = -inf", "stick_release_threshold = true"
    }) reject(std::string("version = 1\n[analog]\n") + fields);
    const auto c = parse("version = 1\n[analog]\nstick_press_threshold = 1\nstick_release_threshold = 0\ntrigger_press_threshold = 255\ntrigger_release_threshold = 0");
    check(c.analog.stick_press_threshold == 1 && c.analog.trigger_press_threshold == 255, "analog boundaries");
    Config invalid;
    invalid.analog.stick_release_threshold = std::numeric_limits<double>::quiet_NaN();
    check(!validate_config(invalid).empty(), "typed NaN rejected");
    invalid = {}; invalid.analog.stick_press_threshold = std::numeric_limits<double>::infinity();
    check(!validate_config(invalid).empty(), "typed infinity rejected");
    invalid = {}; invalid.bindings[0] = 0xFFFF; check(!validate_config(invalid).empty(), "unknown typed virtual key rejected");
}
void names_and_roundtrip() {
    for (unsigned key = 0; key < 256; ++key) {
        const auto name = key_name(static_cast<std::uint16_t>(key));
        if (!name.empty()) check(parse_key_name(name) == key, "canonical key-name roundtrip");
    }
    for (const auto* name : {"None", "Space", "Enter", "Escape", "Tab", "Backspace", "Up", "Down", "Left", "Right",
                            "Insert", "Delete", "Home", "End", "PageUp", "PageDown", "LShift", "RShift", "LCtrl", "RCtrl", "LAlt", "RAlt"})
        check(parse_key_name(name).has_value(), "required named key");
    for (char c = 'A'; c <= 'Z'; ++c) check(parse_key_name(std::string(1, c)) == c, "letters");
    for (char c = '0'; c <= '9'; ++c) check(parse_key_name(std::string(1, c)) == c, "digits");
    for (int i = 1; i <= 12; ++i) check(parse_key_name("F" + std::to_string(i)) == 0x6F + i, "function keys");
    for (const auto* bad : {"", "a", "SPACE", "32", "0x20", "F0", "F13", "F01", "Ctrl+C", "Alt+Tab"})
        check(!parse_key_name(bad), "unsupported key syntax rejected");
    for (const auto name : input_names) {
        const auto c = parse("version = 1\n[bindings]\n" + std::string(name) + " = \"F12\"");
        bool found = false;
        for (std::size_t i = 0; i < input_count; ++i) if (input_names[i] == name) found = c.bindings[i] == 0x7B;
        check(found, "every input configurable");
    }
    Config c;
    check(parse(serialize_config(c)) == c, "default roundtrip");
    c.general = {false, 100}; c.controller.index = 3;
    c.analog = {0.8765432109876543, 0.1234567890123456, 250, 1};
    c.bindings.fill('7'); c.bindings[0] = 0;
    const auto serialized = serialize_config(c);
    check(serialized == serialize_config(c), "deterministic serialization");
    check(parse(serialized) == c, "custom precision and None roundtrip");
    check(serialized.find('\r') == std::string::npos, "canonical LF output");
    c.analog.stick_release_threshold = 0;
    c.analog.stick_press_threshold = std::numeric_limits<double>::denorm_min();
    check(parse(serialize_config(c)) == c, "smallest positive threshold roundtrip");
}
struct TempDirectory {
    std::filesystem::path path;
    TempDirectory() {
        const auto base = std::filesystem::temp_directory_path();
        for (int i = 0; i < 100; ++i) {
            path = base / ("zeropad-tests-" + std::to_string(Keyboard::Clock::now().time_since_epoch().count()) + "-" + std::to_string(i));
            if (std::filesystem::create_directory(path)) return;
        }
        check(false, "create test directory");
    }
    ~TempDirectory() { std::error_code ec; std::filesystem::remove_all(path, ec); }
};
void write(const std::filesystem::path& path, std::string_view text) {
    std::ofstream file(path, std::ios::binary); file.write(text.data(), static_cast<std::streamsize>(text.size()));
    check(static_cast<bool>(file), "write fixture");
}
std::string read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
void no_temporary_files(const std::filesystem::path& dir) {
    for (const auto& entry : std::filesystem::directory_iterator(dir))
        check(entry.path().extension() != ".tmp", "no orphan temporary files");
}
void file_io() {
    TempDirectory dir; const auto path = dir.path / "nested" / "config.toml";
    Config c; std::string error;
    check(load_config(path, c, error) == LoadStatus::Missing, "missing file");
    check(startup_config(path, error) == Config{} && error.empty(), "startup creates defaults");
    check(load_config(path, c, error) == LoadStatus::Loaded && c == Config{}, "initial config load");
    const auto original = read(path);
    c.bindings[0] = 0x0D; c.general.enabled = false;
    check(save_config(path, c, error), "safe existing-file replacement");
    Config loaded; check(load_config(path, loaded, error) == LoadStatus::Loaded && loaded == c, "saved config loaded");
    const auto valid = read(path);
    auto invalid = c; invalid.general.polling_interval_ms = 0;
    check(!save_config(path, invalid, error) && read(path) == valid, "invalid save preserves old bytes");
#ifdef _WIN32
    HANDLE lock = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    check(lock != INVALID_HANDLE_VALUE, "lock replacement fixture");
    check(!save_config(path, Config{}, error), "locked destination replacement fails");
    CloseHandle(lock);
    check(read(path) == valid, "failed replacement preserves existing file");
#endif
    no_temporary_files(path.parent_path());
    write(path, "version = 88\n");
    check(startup_config(path, error) == Config{} && !error.empty(), "invalid startup uses defaults");
    check(read(path) == "version = 88\n", "invalid startup file preserved");
    loaded = c;
    check(load_config(path, loaded, error) == LoadStatus::Error && loaded == c, "invalid load preserves output");
    write(path, std::string(max_config_bytes + 1, 'x'));
    check(load_config(path, loaded, error) == LoadStatus::Error, "oversized disk file");
    const auto blocker = dir.path / "blocker"; write(blocker, "do not change");
    check(!save_config(blocker / "config.toml", c, error), "directory creation failure");
    check(startup_config(blocker / "config.toml", error) == Config{} && !error.empty(), "persistence failure startup continues");
    check(read(blocker) == "do not change", "blocked parent preserved");
    check(!save_config(dir.path, c, error), "cannot replace directory");
    no_temporary_files(dir.path);
    check(save_config(dir.path / "new.toml", c, error), "new save");
    write(path, original);
    check(startup_config(path, error) == Config{} && read(path) == original, "ordinary startup does not rewrite");
}
struct Output {
    std::vector<std::pair<std::uint16_t, bool>> events;
    bool fail_release = false;
    int attempts = 0;
    static bool send(std::uint16_t key, bool down, void* ptr) {
        auto& self = *static_cast<Output*>(ptr); ++self.attempts;
        if (!down && self.fail_release) return false;
        self.events.emplace_back(key, down); return true;
    }
};
void reloads() {
    TempDirectory dir; const auto path = dir.path / "config.toml";
    Runtime r; Output o; Keyboard::Clock::time_point now{};
    Snapshot s{true, button_masks[0]};
    auto tick = [&] { return r.tick(s, now, Output::send, &o); };
    std::string error;
    check(tick() && o.events.back() == std::pair<std::uint16_t, bool>{0x20, true}, "initial A space");
    check(save_config(path, Config{}, error) && r.reload(path, error), "unchanged reload");
    tick(); check(o.events.size() == 1 && !r.transitioning(), "unchanged no keyboard events");
    write(path, "version = 3");
    check(!r.reload(path, error) && r.config() == Config{}, "invalid reload preserves active");
    check(!r.reload(dir.path / "missing", error), "missing reload rejected");
    Config c; c.bindings[0] = 0x0D;
    check(save_config(path, c, error) && r.reload(path, error), "valid reload candidate");
    o.fail_release = true;
    check(!tick() && r.transitioning() && r.config() == Config{}, "release failure holds old config");
    const int attempts = o.attempts;
    for (int i = 0; i < 50; ++i) tick();
    check(o.attempts == attempts && o.events.size() == 1, "transition retries rate limited and no new downs");
    o.fail_release = false; now += Keyboard::retry_interval;
    tick(); check(!r.transitioning() && r.config() == c && !r.held(), "retry completes transition");
    check(o.events.size() == 2 && !o.events.back().second, "old key released before new snapshot");
    tick(); check(o.events.back() == std::pair<std::uint16_t, bool>{0x0D, true}, "new mapping active");
    c.general.enabled = false; check(r.request_config(c, error), "disable request"); tick(); tick();
    check(!r.held() && o.events.size() == 4, "disable releases and blocks controller");
    c.general.enabled = true; r.request_config(c, error); tick(); tick();
    check(r.held() && o.events.size() == 5, "reenable fresh input");
    c.controller.index = 3; r.request_config(c, error); tick();
    check(!r.held() && r.config().controller.index == 3, "index transition releases old input");
    s = {}; tick(); check(!r.held(), "new disconnected controller does not reuse old state");
    s = {true}; s.ly = 10000; tick(); check(r.held(), "stick activation");
    s.ly = 7000; tick(); const auto before = o.events.size();
    r.request_config(c, error); tick();
    check(o.events.size() == before && r.held(), "unchanged preserves analog hysteresis");
    c.analog.stick_press_threshold = 0.5; r.request_config(c, error); tick(); tick();
    check(!r.held(), "threshold change resets hysteresis");
    c.bindings[0] = c.bindings[1] = 'P'; r.request_config(c, error); tick();
    s = {true, button_masks[0]}; tick(); const auto start = o.events.size();
    s.buttons = button_masks[0] | button_masks[1]; tick(); s.buttons = button_masks[1]; tick();
    check(o.events.size() == start, "duplicate mapping remains safe after reload");
    s = {}; tick(); check(!r.held(), "disconnect after reload");
    c.general.polling_interval_ms = 0;
    const auto active = r.config();
    check(!r.request_config(c, error) && r.config() == active && !r.transitioning(), "invalid typed candidate rejected");
}
}
int main() {
    parsing(); numeric_validation(); names_and_roundtrip(); file_io(); reloads();
    std::cout << "All configuration tests passed.\n";
}
