#ifndef INC_ZEROPAD_CONTROLLER_H_
#define INC_ZEROPAD_CONTROLLER_H_
/**
 * @file controller.h
 */

namespace zeropad
{
class Controller
{
public:
    /**
     * @brief Polling controller inputs
     */
    void poll() noexcept;

    /**
     * @brief Check connection
     * @return 
     */
    [[nodiscard]] bool connected() const noexcept
    {
        return connected_;
    }

    [[nodiscard]] bool a_pressed() const noexcept;

private:
    bool connected_ = false;
    bool a_pressed_ = false;
};

} // namespace zeropad
#endif // INC_ZEROPAD_CONTROLLER_H_
