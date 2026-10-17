#include "runtime.h"
#include <array>
#include <cstdlib>
#include <iostream>
using namespace zeropad;
namespace {
struct Output {
    std::array<bool, 256> held{};
    unsigned attempts = 0;
    bool failures = true;
    static bool send(std::uint16_t key, bool down, void* context) {
        auto& self = *static_cast<Output*>(context);
        if (self.failures && ++self.attempts % 13 == 0) return false;
        if (key >= self.held.size() || self.held[key] == down) {
            std::cerr << "duplicate or invalid event\n"; std::exit(1);
        }
        self.held[key] = down; return true;
    }
};
}
int main() {
    unsigned random = 42;
    for (unsigned cycle = 0; cycle < 100; ++cycle) {
        Config config; config.bindings.fill(0);
        config.bindings[0] = config.bindings[1] = 0x20;
        config.bindings[2] = 0x45;
        Runtime runtime(config); Output output;
        Keyboard::Clock::time_point now{};
        for (unsigned i = 0; i < 1000; ++i) {
            random = random * 1664525u + 1013904223u;
            Snapshot snapshot{random % 11 != 0};
            for (unsigned bit = 0; bit < 3; ++bit)
                if (random & (1u << (bit + 8))) snapshot.buttons |= button_masks[bit];
            runtime.tick(snapshot, now, Output::send, &output);
            now += Keyboard::retry_interval;
            // A second stable sample with successful output must reconcile to
            // the independent union of all active digital bindings.
            output.failures = false;
            runtime.tick(snapshot, now, Output::send, &output);
            const bool space = snapshot.connected && (snapshot.buttons & (button_masks[0] | button_masks[1]));
            const bool e = snapshot.connected && (snapshot.buttons & button_masks[2]);
            if (output.held[0x20] != space || output.held[0x45] != e) return 2;
            output.failures = true;
            now += Keyboard::retry_interval;
        }
        output.failures = false;
        if (!runtime.release(now, Output::send, &output) || runtime.held()) return 3;
        for (bool held : output.held) if (held) return 4;
    }
    std::cout << "100 start/stop cycles, 100000 deterministic snapshots with shared mappings and failed output passed\n";
}
