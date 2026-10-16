#include "config.h"
#include "cpptoml.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>

namespace zeropad {
namespace {
struct KeyName { std::string_view name; std::uint16_t key; };
constexpr KeyName named_keys[] {
    {"None", 0}, {"Space", 0x20}, {"Enter", 0x0D}, {"Escape", 0x1B},
    {"Tab", 0x09}, {"Backspace", 0x08}, {"Up", 0x26}, {"Down", 0x28},
    {"Left", 0x25}, {"Right", 0x27}, {"Insert", 0x2D}, {"Delete", 0x2E},
    {"Home", 0x24}, {"End", 0x23}, {"PageUp", 0x21}, {"PageDown", 0x22},
    {"LShift", 0xA0}, {"RShift", 0xA1}, {"LCtrl", 0xA2}, {"RCtrl", 0xA3},
    {"LAlt", 0xA4}, {"RAlt", 0xA5}
};
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
std::string_view trim(std::string_view s) {
    const auto first = s.find_first_not_of(" \t\r");
    if (first == s.npos) return {};
    return s.substr(first, s.find_last_not_of(" \t\r") - first + 1);
}
bool identifier(std::string_view s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
               (c >= '0' && c <= '9') || c == '_' || c == '-';
    });
}
bool valid_utf8(std::string_view s) {
    for (std::size_t i = 0; i < s.size();) {
        const auto c = static_cast<unsigned char>(s[i++]);
        if (c < 0x80) {
            if (c == 0 || (c < 0x20 && c != '\t' && c != '\n' && c != '\r') || c == 0x7F) return false;
            continue;
        }
        unsigned code = 0, count = 0, minimum = 0;
        if (c >= 0xC2 && c <= 0xDF) { code = c & 0x1F; count = 1; minimum = 0x80; }
        else if (c >= 0xE0 && c <= 0xEF) { code = c & 0x0F; count = 2; minimum = 0x800; }
        else if (c >= 0xF0 && c <= 0xF4) { code = c & 7; count = 3; minimum = 0x10000; }
        else return false;
        if (s.size() - i < count) return false;
        for (unsigned n = 0; n < count; ++n) {
            const auto next = static_cast<unsigned char>(s[i++]);
            if ((next & 0xC0) != 0x80) return false;
            code = (code << 6) | (next & 0x3F);
        }
        if (code < minimum || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) return false;
    }
    return true;
}
// A small schema envelope, not a second TOML parser: only flat tables and
// single-line scalar values reach upstream. No recursion or Unicode escapes.
std::string preflight(std::string_view text) {
    require(text.size() <= max_config_bytes, "Configuration exceeds 64 KiB");
    require(valid_utf8(text), "Configuration contains NUL, invalid UTF-8, or control bytes");
    if (text.starts_with("\xEF\xBB\xBF")) text.remove_prefix(3);
    static const std::regex number(R"([+-]?(0|[1-9](_?[0-9])*)(\.[0-9](_?[0-9])*)?([eE][+-]?[0-9](_?[0-9])*)?|0x[0-9A-Fa-f](_?[0-9A-Fa-f])*|0o[0-7](_?[0-7])*|0b[01](_?[01])*|[+-]?(inf|nan))");
    std::set<std::string> tables, fields;
    std::string section;
    std::string parser_text;
    std::size_t line_number = 0;
    while (!text.empty()) {
        ++line_number;
        auto end = text.find('\n');
        auto line = trim(text.substr(0, end));
        text = end == text.npos ? std::string_view{} : text.substr(end + 1);
        char quote = 0;
        for (std::size_t i = 0; i < line.size(); ++i) {
            if (quote) { if (line[i] == quote) quote = 0; }
            else if (line[i] == '"' || line[i] == '\'') quote = line[i];
            else if (line[i] == '#') { line = trim(line.substr(0, i)); break; }
        }
        const auto where = " at line " + std::to_string(line_number);
        require(quote == 0, "Unterminated string" + where);
        if (line.empty()) continue;
        if (line.front() == '[') {
            require(line.back() == ']' && identifier(trim(line.substr(1, line.size() - 2))),
                    "Only single-level named tables are supported (nesting limit 1)" + where);
            section = std::string(trim(line.substr(1, line.size() - 2)));
            require(tables.insert(section).second, "Duplicate section " + section + where);
            require(section == "general" || section == "controller" || section == "analog" || section == "bindings",
                    "Unknown section " + section + where);
            parser_text.append(line).append("\n");
            continue;
        }
        auto equal = line.find('=');
        require(equal != line.npos, "Expected field = value" + where);
        auto key = trim(line.substr(0, equal));
        auto value = trim(line.substr(equal + 1));
        require(identifier(key), "Use bare field names; dotted/nested keys are unsupported" + where);
        const auto path = section + "." + std::string(key);
        const bool known = (section.empty() && key == "version") ||
            (section == "general" && (key == "enabled" || key == "polling_interval_ms")) ||
            (section == "controller" && key == "index") ||
            (section == "analog" && (key == "stick_press_threshold" || key == "stick_release_threshold" ||
                                      key == "trigger_press_threshold" || key == "trigger_release_threshold")) ||
            (section == "bindings" && std::find(input_names.begin(), input_names.end(), key) != input_names.end());
        require(known, "Unknown field " + path + where);
        require(fields.insert(path).second, "Duplicate field " + path + where);
        require(!value.empty() && value.size() <= 64, "Missing or oversized scalar value for " + path + where);
        if (value.front() == '"' || value.front() == '\'') {
            require(value.size() >= 2 && value.back() == value.front(), "Invalid string for " + path);
            const auto body = value.substr(1, value.size() - 2);
            require(std::all_of(body.begin(), body.end(), [](char c) {
                return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
            }), "Only canonical unescaped key-name strings are supported for " + path);
        } else {
            require(value == "true" || value == "false" || std::regex_match(value.begin(), value.end(), number),
                    "Expected scalar boolean, number, or key-name string for " + path + where);
        }
        parser_text.append(line).append("\n");
    }
    return parser_text;
}
std::string raw(cpptoml::TomlProxy proxy) {
    std::string result(static_cast<std::size_t>(proxy.size()) + 1, '\0');
    proxy.getString(result.data());
    result.resize(static_cast<std::size_t>(proxy.size()));
    return result;
}
std::string numeric_text(cpptoml::TomlProxy p) {
    auto s = raw(p);
    s.erase(std::remove(s.begin(), s.end(), '_'), s.end());
    if (!s.empty() && s.front() == '+') s.erase(s.begin());
    return s;
}
int integer(cpptoml::TomlProxy p, const std::string& field) {
    using cpptoml::TomlType;
    const auto type = p.type();
    require(type == TomlType::Integer || type == TomlType::Hex || type == TomlType::Oct || type == TomlType::Bin,
            field + " must be an integer");
    auto s = numeric_text(p);
    int base = 10;
    if (type == TomlType::Hex || type == TomlType::Oct || type == TomlType::Bin) {
        base = type == TomlType::Hex ? 16 : type == TomlType::Oct ? 8 : 2;
        s.erase(0, 2);
    }
    int value = 0;
    const auto result = std::from_chars(s.data(), s.data() + s.size(), value, base);
    require(result.ec == std::errc{} && result.ptr == s.data() + s.size(), field + " integer is out of range");
    return value;
}
double numeric(cpptoml::TomlProxy p, const std::string& field) {
    require(p.type() != cpptoml::TomlType::NaN && p.type() != cpptoml::TomlType::Inf,
            field + " must be a finite number");
    if (p.type() != cpptoml::TomlType::Float) return integer(p, field);
    auto s = numeric_text(p);
    double value = 0;
    const auto result = std::from_chars(s.data(), s.data() + s.size(), value);
    require(result.ec == std::errc{} && result.ptr == s.data() + s.size() && std::isfinite(value),
            field + " must be a finite number");
    return value;
}
void read_table(cpptoml::TomlProxy table, std::string_view section, Config& c) {
    for (auto entry = table.begin(); entry; entry = entry.next()) {
        require(entry.type() == cpptoml::TomlType::KeyValue, "Invalid table entry");
        const auto name = raw(entry.key());
        const auto field = section.empty() ? name : std::string(section) + "." + name;
        const auto p = entry.value();
        if (section.empty()) {
            if (name == "version") require(integer(p, field) == Config::schema_version, "Unsupported schema version");
            else {
                require((name == "general" || name == "controller" || name == "analog" || name == "bindings") &&
                        p.type() == cpptoml::TomlType::Table, "Unknown section or field " + field);
                read_table(p, name, c);
            }
        } else if (section == "general" && name == "enabled") {
            require(p.type() == cpptoml::TomlType::True || p.type() == cpptoml::TomlType::False, field + " must be boolean");
            c.general.enabled = p.getBool();
        } else if (section == "general" && name == "polling_interval_ms") c.general.polling_interval_ms = integer(p, field);
        else if (section == "controller" && name == "index") c.controller.index = integer(p, field);
        else if (section == "analog" && name == "stick_press_threshold") c.analog.stick_press_threshold = numeric(p, field);
        else if (section == "analog" && name == "stick_release_threshold") c.analog.stick_release_threshold = numeric(p, field);
        else if (section == "analog" && name == "trigger_press_threshold") c.analog.trigger_press_threshold = integer(p, field);
        else if (section == "analog" && name == "trigger_release_threshold") c.analog.trigger_release_threshold = integer(p, field);
        else if (section == "bindings") {
            const auto it = std::find(input_names.begin(), input_names.end(), name);
            require(it != input_names.end(), "Unknown controller input " + field);
            require(p.type() == cpptoml::TomlType::String, field + " must be a key-name string");
            auto value = raw(p);
            require(value.size() >= 2, "Invalid key string for " + field);
            auto key = parse_key_name(std::string_view(value).substr(1, value.size() - 2));
            require(key.has_value(), "Unknown keyboard key for " + field + ": " + value);
            c.bindings[static_cast<std::size_t>(it - input_names.begin())] = *key;
        } else require(false, "Unknown field " + field);
    }
}
}
std::optional<std::uint16_t> parse_key_name(std::string_view name) noexcept {
    for (const auto& entry : named_keys) if (entry.name == name) return entry.key;
    if (name.size() == 1 && ((name[0] >= 'A' && name[0] <= 'Z') || (name[0] >= '0' && name[0] <= '9')))
        return static_cast<std::uint16_t>(name[0]);
    if (name.size() >= 2 && name.front() == 'F') {
        for (int i = 1; i <= 12; ++i) {
            // No locale conversion or allocation in this lookup.
            if ((i < 10 && name.size() == 2 && name[1] == '0' + i) ||
                (i >= 10 && name.size() == 3 && name[1] == '1' && name[2] == '0' + i - 10))
                return static_cast<std::uint16_t>(0x70 + i - 1);
        }
    }
    return std::nullopt;
}
std::string key_name(std::uint16_t key) {
    for (const auto& entry : named_keys) if (entry.key == key) return std::string(entry.name);
    if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9')) return std::string(1, static_cast<char>(key));
    if (key >= 0x70 && key <= 0x7B) return "F" + std::to_string(key - 0x70 + 1);
    return {};
}
std::string validate_config(const Config& c) {
    if (c.general.polling_interval_ms < 1 || c.general.polling_interval_ms > 100) return "general.polling_interval_ms must be 1..100";
    if (c.controller.index < 0 || c.controller.index > 3) return "controller.index must be 0..3";
    const auto& a = c.analog;
    if (!std::isfinite(a.stick_press_threshold) || !std::isfinite(a.stick_release_threshold) ||
        a.stick_release_threshold < 0 || a.stick_release_threshold >= a.stick_press_threshold || a.stick_press_threshold > 1)
        return "analog stick thresholds require finite 0 <= release < press <= 1";
    if (a.trigger_release_threshold < 0 || a.trigger_release_threshold >= a.trigger_press_threshold || a.trigger_press_threshold > 255)
        return "analog trigger thresholds require 0 <= release < press <= 255";
    for (std::size_t i = 0; i < input_count; ++i)
        if (key_name(c.bindings[i]).empty()) return "Invalid binding for " + std::string(input_names[i]);
    return {};
}
bool parse_config(std::string_view text, Config& output, std::string& error) {
    try {
        auto buffer = preflight(text);
        // Padding keeps upstream lookahead within allocated storage. Only the
        // actual text (with a final newline) is passed as the logical document.
        buffer += '\n';
        const auto size = buffer.size();
        buffer.append(16, ' ');
        cpptoml::TomlParser parser;
        require(parser.parse(buffer.data(), buffer.data() + size), "Invalid TOML syntax");
        bool version = false;
        for (auto e = parser.root().begin(); e; e = e.next()) if (raw(e.key()) == "version") version = true;
        require(version, "Missing required version field");
        Config candidate;
        read_table(parser.root(), {}, candidate);
        const auto validation = validate_config(candidate);
        require(validation.empty(), validation);
        output = candidate;
        error.clear();
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
std::string serialize_config(const Config& c) {
    const auto error = validate_config(c);
    if (!error.empty()) throw std::invalid_argument(error);
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::boolalpha << std::setprecision(std::numeric_limits<double>::max_digits10);
    out << "version = 1\n\n[general]\nenabled = " << c.general.enabled
        << "\npolling_interval_ms = " << c.general.polling_interval_ms
        << "\n\n[controller]\nindex = " << c.controller.index
        << "\n\n[analog]\nstick_press_threshold = " << c.analog.stick_press_threshold
        << "\nstick_release_threshold = " << c.analog.stick_release_threshold
        << "\ntrigger_press_threshold = " << c.analog.trigger_press_threshold
        << "\ntrigger_release_threshold = " << c.analog.trigger_release_threshold
        << "\n\n[bindings]\n";
    for (std::size_t i = 0; i < input_count; ++i) out << input_names[i] << " = \"" << key_name(c.bindings[i]) << "\"\n";
    return out.str();
}
} // namespace zeropad
