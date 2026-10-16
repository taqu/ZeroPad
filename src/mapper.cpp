#include "zeropad/mapper.hpp"

namespace zeropad {

std::optional<bool> Mapper::update(bool a_pressed) const noexcept {
    if (a_pressed == key_injected_) {
        return std::nullopt;
    }
    return a_pressed;
}

void Mapper::complete(bool success) noexcept {
    // Caller invokes this only after an event was requested.
    if (success) {
        key_injected_ = !key_injected_;
    }
}

} // namespace zeropad
