#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace zeropad {
enum class Input : std::size_t {
    A, B, X, Y, DpadUp, DpadDown, DpadLeft, DpadRight,
    LB, RB, Start, Back, LSButton, RSButton, LT, RT,
    LSUp, LSDown, LSLeft, LSRight, RSUp, RSDown, RSLeft, RSRight, Count
};
constexpr auto input_count = static_cast<std::size_t>(Input::Count);
using LogicalState = std::array<bool, input_count>;
struct Snapshot {
    bool connected = false;
    std::uint16_t buttons = 0;
    std::uint8_t lt = 0, rt = 0;
    std::int16_t lx = 0, ly = 0, rx = 0, ry = 0;
};
inline constexpr int trigger_press = 40, trigger_release = 25;
inline constexpr double stick_press = 0.25, stick_release = 0.20;
static_assert(0 <= trigger_release && trigger_release < trigger_press && trigger_press <= 255);
static_assert(0 <= stick_release && stick_release < stick_press && stick_press <= 1);
struct AnalogConfig {
    double stick_press_threshold = stick_press;
    double stick_release_threshold = stick_release;
    int trigger_press_threshold = trigger_press;
    int trigger_release_threshold = trigger_release;
    bool operator==(const AnalogConfig&) const = default;
};
inline constexpr std::array<std::uint16_t, 14> button_masks {
    0x1000, 0x2000, 0x4000, 0x8000, 0x0001, 0x0002, 0x0004,
    0x0008, 0x0100, 0x0200, 0x0010, 0x0020, 0x0040, 0x0080
};
class LogicalInputs {
public:
    explicit LogicalInputs(AnalogConfig analog = {}) noexcept : analog_(analog) {}
    const LogicalState& update(const Snapshot& snapshot) noexcept;
private:
    AnalogConfig analog_;
    LogicalState state_{};
};
using Mapping = std::array<std::uint16_t, input_count>;
// Windows virtual-key values; zero disables a binding.
inline constexpr Mapping default_mapping {
    0x20, 0x1B, 'E', 'R', 0x26, 0x28, 0x25, 0x27,
    'Q', 'F', 0x0D, 0x09, 0xA0, 0xA2, 'Z', 'X',
    'W', 'S', 'A', 'D', 'I', 'K', 'J', 'L'
};
class Mapper {
public:
    explicit Mapper(Mapping mapping = default_mapping) noexcept : mapping_(mapping) {}
    void update(const LogicalState& inputs) noexcept;
    void release_all() noexcept { desired_.fill(false); }
    bool desired(std::size_t key) const noexcept { return desired_[key]; }
private:
    Mapping mapping_;
    std::array<bool, 256> desired_{};
};
} // namespace zeropad
