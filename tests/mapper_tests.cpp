#include "zeropad/mapper.hpp"

#include <cstdlib>
#include <iostream>

namespace {
void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

int main() {
    zeropad::Mapper mapper;
    check(!mapper.update(false), "initial released state has no event");
    check(mapper.update(true) == true, "press requests key-down");
    mapper.complete(true);
    check(mapper.key_injected(), "successful key-down advances injected state");
    check(!mapper.update(true), "held input has no duplicate event");
    check(mapper.update(false) == false, "release requests key-up");
    mapper.complete(true);
    check(!mapper.update(false), "repeated released states have no event");

    check(mapper.update(true) == true, "press after reconnect requests key-down");
    mapper.complete(true);
    check(mapper.update(false) == false, "disconnect while pressed requests key-up");
    mapper.complete(true);
    check(!mapper.update(false), "disconnect while released has no event");
    check(mapper.update(true) == true, "reconnected controller resumes tracking");

    // A failed key-down must not claim the key is held. A later false state is a no-op,
    // and a later true state may retry the injection.
    mapper.complete(false);
    check(!mapper.key_injected(), "failed key-down leaves injected state released");
    check(mapper.update(false) == std::nullopt, "failed press followed by release needs no key-up");
    check(mapper.update(true) == true, "failed key-down can be retried");
    mapper.complete(true);
    check(mapper.update(false) == false, "shutdown while held requests key-up");
    mapper.complete(false);
    check(mapper.key_injected(), "failed key-up does not claim successful release");
    return EXIT_SUCCESS;
}
