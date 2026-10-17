#include "keyboard.h"
#include <Windows.h>
namespace zeropad {
bool send_key(std::uint16_t key, bool pressed, void*) noexcept {
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = key;
    input.ki.dwExtraInfo = injected_key_tag;
    const bool extended = (key >= VK_PRIOR && key <= VK_DOWN) ||
                          key == VK_INSERT || key == VK_DELETE ||
                          key == VK_RCONTROL || key == VK_RMENU;
    input.ki.dwFlags = (pressed ? 0u : KEYEVENTF_KEYUP) | (extended ? KEYEVENTF_EXTENDEDKEY : 0u);
    return SendInput(1, &input, sizeof(input)) == 1;
}
}
