#ifndef INC_ZEROPAD_KEYBOARD_H_
#define INC_ZEROPAD_KEYBOARD_H_
/**
 * @file keyboard.h
 */

namespace zeropad
{

/**
 * @brief Translate to space key being pressed
 * @param pressed 
 * @return 
 */
[[nodiscard]] bool set_space_key(bool pressed) noexcept;

} // namespace zeropad
#endif // INC_ZEROPAD_KEYBOARD_H_
