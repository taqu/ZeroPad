#pragma once

namespace zeropad {

class Controller {
public:
    void poll() noexcept;
    [[nodiscard]] bool connected() const noexcept { return connected_; }
    [[nodiscard]] bool a_pressed() const noexcept;

private:
    bool connected_ = false;
    bool a_pressed_ = false;
};

} // namespace zeropad
