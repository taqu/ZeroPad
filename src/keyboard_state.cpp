#include "keyboard.h"
namespace zeropad {
bool Keyboard::reconcile(const Mapper& mapper, Clock::time_point now, KeyOutput output, void* context) noexcept {
    bool success = true;
    // Release before pressing to handle direct changes between directions.
    for (bool pressed : {false, true}) {
        for (std::size_t key = 1; key < injected_.size(); ++key) {
            if (mapper.desired(key) != pressed || injected_[key] == pressed) continue;
            if (now < retry_after_[key]) { success = false; continue; }
            if (output(static_cast<std::uint16_t>(key), pressed, context)) {
                injected_[key] = pressed;
                retry_after_[key] = {};
            } else {
                retry_after_[key] = now + retry_interval;
                success = false;
            }
        }
    }
    return success;
}
bool Keyboard::held() const noexcept {
    for (bool pressed : injected_) if (pressed) return true;
    return false;
}
}
