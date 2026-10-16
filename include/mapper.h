#ifndef INC_ZEROPAD_MAPPER_H_
#define INC_ZEROPAD_MAPPER_H_
/**
 * @file mapper.h
 */

#include <optional>

namespace zeropad
{

/**
 * @brief A value of true requests key-down; false requests key-up.
 */
class Mapper
{
public:
    [[nodiscard]] std::optional<bool> update(bool a_pressed) const noexcept;
    void complete(bool success) noexcept;
    [[nodiscard]] bool key_injected() const noexcept
    {
        return key_injected_;
    }

private:
    bool key_injected_ = false;
};

} // namespace zeropad
#endif // INC_ZEROPAD_MAPPER_H_
