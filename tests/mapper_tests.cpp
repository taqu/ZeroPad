#include "mapper.h"
#include "keyboard.h"
#include <cstdlib>
#include <iostream>
#include <vector>
#include <utility>
using namespace zeropad;
namespace {
void check(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(EXIT_FAILURE); }
}
struct Rig {
    LogicalInputs logical;
    Mapper mapper;
    Keyboard keyboard;
    Keyboard::Clock::time_point now{};
    std::vector<std::pair<std::uint16_t, bool>> events;
    std::array<bool, 256> fail{};
    int calls = 0;
    explicit Rig(Mapping mapping = default_mapping) : mapper(mapping) {}
    static bool output(std::uint16_t key, bool down, void* context) {
        auto& r = *static_cast<Rig*>(context);
        ++r.calls;
        if (r.fail[key]) return false;
        r.events.emplace_back(key, down); return true;
    }
    void tick(Snapshot s) {
        mapper.update(logical.update(s));
        keyboard.reconcile(mapper, now, output, this);
    }
    void advance() { now += Keyboard::retry_interval; }
    void expect(std::uint16_t key, bool down) {
        check(!events.empty() && events.back() == std::pair{key, down}, "expected key transition");
    }
};
void digital() {
    for (std::size_t i = 0; i < button_masks.size(); ++i) {
        Rig r;
        r.tick({true}); check(r.events.empty(), "neutral");
        r.tick({true, button_masks[i]}); r.expect(default_mapping[i], true);
        r.tick({true, button_masks[i]}); check(r.events.size() == 1, "held button no repeat");
        r.tick({true}); r.expect(default_mapping[i], false);
        r.tick({true}); check(r.events.size() == 2, "released no repeat");
    }
    Rig r;
    r.tick({true, 0xFFFF, 255, 255, 32767, 32767, -32768, -32768});
    check(r.events.size() == 20, "all digital, triggers, diagonal sticks simultaneous; reserved bits ignored");
    r.tick({}); check(r.events.size() == 40 && !r.keyboard.held(), "disconnect releases all");
    r.tick({}); check(r.events.size() == 40, "disconnect no repeats");
    r.tick({true, button_masks[0]}); r.expect(0x20, true);
    r.tick({true, button_masks[1]});
    check(!r.keyboard.injected(0x20) && r.keyboard.injected(0x1B), "same snapshot button replacement");
    r.tick({true, 0x0C00}); check(!r.keyboard.held(), "reserved buttons ignored");
}
void triggers() {
    for (bool right : {false, true}) {
        Rig r; Snapshot s{true};
        auto& value = right ? s.rt : s.lt;
        auto key = default_mapping[right ? 15 : 14];
        for (int v : {0, 25, 26, 39}) { value = static_cast<std::uint8_t>(v); r.tick(s); }
        check(r.events.empty(), "trigger below press threshold");
        value = 40; r.tick(s); r.expect(key, true);
        for (int v : {39, 26, 40, 255, 26}) { value = static_cast<std::uint8_t>(v); r.tick(s); }
        check(r.events.size() == 1, "trigger hysteresis");
        value = 25; r.tick(s); r.expect(key, false);
        value = 39; r.tick(s); check(r.events.size() == 2, "trigger released band");
    }
    Rig r; Snapshot s{true}; s.lt = s.rt = 255; r.tick(s);
    s.lt = 0; r.tick(s); check(r.keyboard.injected('X') && !r.keyboard.injected('Z'), "independent triggers");
    r.tick({}); s.lt = 30; s.rt = 0; r.tick(s);
    check(!r.keyboard.held(), "disconnect resets trigger hysteresis");
}
void sticks() {
    for (int axis = 0; axis < 4; ++axis) {
        Rig r; Snapshot s{true};
        std::int16_t* axes[] = {&s.lx, &s.ly, &s.rx, &s.ry};
        auto& value = *axes[axis];
        const std::size_t positive[] = {19, 16, 23, 20};
        const std::size_t negative[] = {18, 17, 22, 21};
        auto pos = default_mapping[positive[axis]], neg = default_mapping[negative[axis]];
        for (int v : {0, 100, 6553, 8191}) { value = static_cast<std::int16_t>(v); r.tick(s); }
        check(r.events.empty(), "stick deadzone and below press");
        value = 8192; r.tick(s); r.expect(pos, true);
        for (int v : {8191, 6554, 8192, 32767, 6554}) { value = static_cast<std::int16_t>(v); r.tick(s); }
        check(r.events.size() == 1, "positive stick hysteresis");
        value = 6553; r.tick(s); r.expect(pos, false);
        value = -8191; r.tick(s); check(r.events.size() == 2, "negative below threshold");
        value = -8192; r.tick(s); r.expect(neg, true);
        value = -6554; r.tick(s); check(r.events.size() == 3, "negative release band");
        value = -6553; r.tick(s); r.expect(neg, false);
        value = -32768; r.tick(s); r.expect(neg, true);
        value = 32767; r.tick(s);
        check(r.events[r.events.size()-2] == std::pair{neg, false}, "opposite releases first");
        r.expect(pos, true);
        value = 0; r.tick(s); check(!r.keyboard.held(), "center releases");
    }
    Rig r; Snapshot s{true}; s.lx = s.ly = 32767; r.tick(s);
    check(r.keyboard.injected('W') && r.keyboard.injected('D'), "diagonal");
    s.lx = -32768; r.tick(s);
    check(r.events.size() == 4 && r.keyboard.injected('W') && r.keyboard.injected('A'), "diagonal direction change");
    s.buttons = button_masks[12]; r.tick(s);
    check(r.keyboard.injected(0xA0) && r.keyboard.injected('W'), "click independent of movement");
    r.tick({}); s = {true}; s.ly = 7000; r.tick(s);
    check(!r.keyboard.held(), "disconnect resets stick hysteresis");
}
void duplicates() {
    Mapping mapping{}; mapping[0] = mapping[1] = mapping[14] = 0x20;
    for (bool reverse : {false, true}) {
        Rig r(mapping);
        auto first = button_masks[reverse ? 1 : 0], second = button_masks[reverse ? 0 : 1];
        r.tick({true, first}); r.tick({true, static_cast<std::uint16_t>(first | second)});
        r.tick({true, second}); check(r.events.size() == 1, "shared key remains down");
        r.tick({true, first}); check(r.events.size() == 1, "same snapshot shared transfer");
        Snapshot s{true, first}; s.lt = 255; r.tick(s); s.buttons = 0; r.tick(s);
        check(r.events.size() == 1, "three shared bindings");
        r.tick({}); check(r.events.size() == 2, "shared disconnect exactly once");
        r.tick({}); check(r.events.size() == 2, "shared disconnected no repeat");
    }
}
void failures() {
    Rig r; r.fail[0x20] = true; r.tick({true, button_masks[0]});
    check(!r.keyboard.injected(0x20) && r.calls == 1, "failed down unrecorded");
    for (int i = 0; i < 100; ++i) r.tick({true, button_masks[0]});
    check(r.calls == 1, "retry rate limited");
    r.advance(); r.tick({true, button_masks[0]}); check(r.calls == 2, "retry at deadline");
    r.fail[0x20] = false; r.advance(); r.tick({});
    check(r.events.empty(), "disconnect cancels failed down");
    r.tick({true, button_masks[0]}); r.expect(0x20, true);
    r.fail[0x20] = true; r.tick({}); check(r.keyboard.injected(0x20), "failed up retained");
    r.tick({true, button_masks[1]}); check(r.keyboard.injected(0x1B), "another key progresses independently");
    r.fail[0x20] = false; r.advance(); r.tick({});
    check(!r.keyboard.held(), "release retries reconcile");
    r.tick({true, static_cast<std::uint16_t>(button_masks[0] | button_masks[1])});
    r.mapper.release_all(); r.fail[0x20] = r.fail[0x1B] = true;
    for (int i = 0; i < 5; ++i) { r.advance(); r.keyboard.reconcile(r.mapper, r.now, Rig::output, &r); }
    check(r.keyboard.injected(0x20) && r.keyboard.injected(0x1B), "bounded shutdown retains failed keys");
    r.fail[0x20] = r.fail[0x1B] = false; r.advance();
    r.keyboard.reconcile(r.mapper, r.now, Rig::output, &r);
    check(!r.keyboard.held(), "shutdown releases all held keys");
}
}
int main() {
    digital(); triggers(); sticks(); duplicates(); failures();
    std::cout << "All mapping tests passed.\n";
}
