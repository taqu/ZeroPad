#pragma once
#include "mapper.h"
#include <chrono>
namespace zeropad {
inline constexpr std::uintptr_t injected_key_tag = 0x5A504144;
using KeyOutput = bool (*)(std::uint16_t key, bool pressed, void* context);
class Keyboard {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr auto retry_interval = std::chrono::milliseconds(100);
    bool reconcile(const Mapper& mapper, Clock::time_point now, KeyOutput output, void* context = nullptr) noexcept;
    bool held() const noexcept;
    bool injected(std::size_t key) const noexcept { return injected_[key]; }
private:
    std::array<bool, 256> injected_{};
    std::array<Clock::time_point, 256> retry_after_{};
};
bool send_key(std::uint16_t key, bool pressed, void*) noexcept;
}
