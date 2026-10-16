#include "zeropad/controller.hpp"

#include <Windows.h>
#include <Xinput.h>

namespace zeropad {

void Controller::poll() noexcept {
    XINPUT_STATE state{};
    connected_ = XInputGetState(0, &state) == ERROR_SUCCESS;
    a_pressed_ = connected_ && (state.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0;
}

bool Controller::a_pressed() const noexcept {
    return a_pressed_;
}

} // namespace zeropad
