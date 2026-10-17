#include "mapper.h"

namespace zeropad {
namespace {
bool threshold(double value, bool held, double press, double release) noexcept {
    return held ? value > release : value >= press;
}
double normalize(std::int16_t value) noexcept {
    return value < 0 ? static_cast<double>(value) / 32768.0 : static_cast<double>(value) / 32767.0;
}
}
const LogicalState& LogicalInputs::update(const Snapshot& s) noexcept {
    if (!s.connected) { state_.fill(false); return state_; }
    for (std::size_t i = 0; i < button_masks.size(); ++i)
        state_[i] = (s.buttons & button_masks[i]) != 0;
    state_[14] = threshold(s.lt, state_[14], analog_.trigger_press_threshold, analog_.trigger_release_threshold);
    state_[15] = threshold(s.rt, state_[15], analog_.trigger_press_threshold, analog_.trigger_release_threshold);
    const std::array<double, 8> directions {
        normalize(s.ly), -normalize(s.ly), -normalize(s.lx), normalize(s.lx),
        normalize(s.ry), -normalize(s.ry), -normalize(s.rx), normalize(s.rx)
    };
    for (std::size_t i = 0; i < directions.size(); ++i)
        state_[16 + i] = threshold(directions[i], state_[16 + i], analog_.stick_press_threshold, analog_.stick_release_threshold);
    return state_;
}
void Mapper::update(const LogicalState& inputs) noexcept {
    desired_.fill(false);
    for (std::size_t i = 0; i < input_count; ++i)
        if (inputs[i] && mapping_[i] > 0 && mapping_[i] < desired_.size())
            desired_[mapping_[i]] = true;
}
} // namespace zeropad
