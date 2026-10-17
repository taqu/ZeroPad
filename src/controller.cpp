#include "controller.h"
#include <Windows.h>
#include <Xinput.h>
namespace zeropad {
void Controller::poll(unsigned int index) noexcept {
    XINPUT_STATE state{};
    snapshot_ = {};
    if (XInputGetState(index, &state) != ERROR_SUCCESS) return;
    const auto& g = state.Gamepad;
    snapshot_ = {true, g.wButtons, g.bLeftTrigger, g.bRightTrigger,
                 g.sThumbLX, g.sThumbLY, g.sThumbRX, g.sThumbRY};
}
}
