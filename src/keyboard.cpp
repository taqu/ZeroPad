#include "zeropad/keyboard.hpp"

#include <Windows.h>

namespace zeropad {

bool set_space_key(bool pressed) noexcept {
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = VK_SPACE;
    input.ki.dwFlags = pressed ? 0 : KEYEVENTF_KEYUP;
    return SendInput(1, &input, sizeof(input)) == 1;
}

} // namespace zeropad
