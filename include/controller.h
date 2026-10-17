#pragma once
#include "mapper.h"
namespace zeropad {
class Controller {
public:
    void poll(unsigned int index = 0) noexcept;
    bool connected() const noexcept { return snapshot_.connected; }
    const Snapshot& snapshot() const noexcept { return snapshot_; }
private:
    Snapshot snapshot_{};
};
}
