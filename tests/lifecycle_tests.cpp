#include "lifecycle.h"
#include "engine_service.h"
#include <cstdlib>
#include <iostream>
#include <vector>
#include <atomic>
#include <future>
using namespace zeropad;
namespace {
void check(bool ok, const char* text) { if (!ok) { std::cerr << "FAIL: " << text << '\n'; std::exit(1); } }
struct Rig {
    EngineCore engine;
    Keyboard::Clock::time_point now{};
    Snapshot controller{true, button_masks[0]};
    bool fail_release = false;
    std::vector<bool> events;
    static bool output(std::uint16_t, bool down, void* ptr) {
        auto& self = *static_cast<Rig*>(ptr);
        if (!down && self.fail_release) return false;
        self.events.push_back(down); return true;
    }
    void tick() { engine.step(controller, false, now, output, this); }
    void advance() { now += Keyboard::retry_interval; }
};
void lifecycle_and_tray() {
    check(tray_action(tray_settings) == TrayAction::Settings && tray_action(tray_toggle) == TrayAction::Toggle && tray_action(tray_exit) == TrayAction::Exit,
          "tray commands map to distinct actions");
    check(tray_action(0) == TrayAction::None, "unknown commands ignored");
    AppLifecycle app;
    Rig rig; rig.tick();
    for (int i = 0; i < 20; ++i) {
        check(app.open_settings() && !app.open_settings(), "only one settings editor");
        SettingsEditor editor(rig.engine.status(), {}); editor.clear(0); editor.cancel();
        app.settings_closed(); rig.tick();
        check(!app.stopping && rig.events.size() == 1, "closing editor leaves held key and engine unchanged");
    }
    rig.engine.runtime_enabled(false); rig.fail_release = true; rig.tick();
    check(rig.engine.status().mode == MappingMode::Recovering, "failed disable does not falsely acknowledge disabled");
    rig.fail_release = false; rig.advance(); rig.tick();
    check(rig.engine.status().mode == MappingMode::Disabled && !rig.events.back(), "disable releases held input");
    check(rig.engine.status().config.general.enabled, "tray disable leaves startup preference unchanged");
    check(tray_tooltip(rig.engine.status().mode) == L"ZeroPad - Disabled", "tooltip reflects effective state");
    rig.engine.runtime_enabled(true); rig.tick(); rig.tick();
    check(rig.engine.status().mode == MappingMode::Running && rig.events.back(), "enable samples fresh state");
    std::string error;
    const auto same = rig.events.size();
    check(rig.engine.apply(Config{}, 0, error), "no-change Save clears matching override"); rig.tick();
    check(rig.events.size() == same && !rig.engine.status().session_override, "no-change Save preserves held key and clears override");
    rig.engine.suspend(); rig.tick(); rig.engine.runtime_enabled(true); rig.tick();
    check(rig.engine.status().mode == MappingMode::Capture && !rig.events.back(), "tray cannot bypass capture suspension");
    check(rig.engine.apply(Config{}, 0, error), "saved configuration accepted"); rig.tick();
    check(!rig.engine.status().session_override, "successful Save clears session override");
    check(app.exit() && !app.exit() && !app.open_settings(), "exit is idempotent and blocks reopening");
}
void power_and_recovery() {
    Rig rig; rig.controller.ly = 10000; rig.tick();
    rig.engine.power(true); rig.fail_release = true; rig.tick();
    const auto before = rig.events.size();
    rig.engine.power(false); rig.engine.power(false); rig.controller = {true}; rig.controller.ly = 7000;
    rig.tick(); check(rig.events.size() == before && rig.engine.status().mode == MappingMode::Recovering, "resume waits for outstanding releases");
    rig.fail_release = false; rig.advance(); rig.tick(); rig.tick();
    check(!rig.events.back() && rig.engine.status().mode == MappingMode::Running, "resume resets stick hysteresis");
    const auto count = rig.events.size();
    rig.engine.power(false); rig.tick(); check(rig.events.size() == count, "repeated resume is idempotent");
    rig.engine.runtime_enabled(false); rig.tick(); rig.engine.power(true); rig.tick(); rig.engine.power(false); rig.tick(); rig.tick();
    check(rig.engine.status().mode == MappingMode::Disabled, "resume preserves runtime disable");
    rig.engine.session_locked(true); rig.engine.runtime_enabled(true); rig.tick();
    check(rig.engine.status().mode == MappingMode::Recovering, "tray enable cannot bypass session lock");
    rig.engine.power(true); rig.engine.session_locked(false); rig.tick();
    check(rig.engine.status().mode == MappingMode::Recovering, "unlock cannot bypass power suspension");
    rig.engine.power(false); rig.tick(); rig.tick(); check(rig.engine.status().mode == MappingMode::Running, "all independent reasons must clear");
    rig.controller = {true, button_masks[0]}; rig.tick();
    rig.controller = {}; rig.fail_release = true; rig.tick();
    check(rig.engine.status().mode == MappingMode::Recovering, "disconnect retains failed release");
    rig.controller = {true, button_masks[1]}; rig.tick();
    rig.fail_release = false; rig.advance(); rig.tick(); const auto recovered = rig.events.size(); rig.tick();
    check(rig.events.size() == recovered + 1 && rig.events.back(), "reconnect presses only after release reconciliation");
    rig.controller = {}; rig.tick(); const auto disconnected = rig.events.size(); rig.tick();
    check(rig.events.size() == disconnected, "repeated disconnect does not duplicate releases");
}
struct Mock {
    std::atomic_bool down = false;
    std::atomic_int presses = 0, releases = 0;
    std::atomic_bool reject_up = false;
    static Snapshot poll(unsigned, void*) { return {true, button_masks[0]}; }
    static bool output(std::uint16_t, bool pressed, void* ptr) {
        auto& m = *static_cast<Mock*>(ptr);
        if (!pressed) { ++m.releases; if (m.reject_up) return false; }
        if (pressed) ++m.presses;
        m.down = pressed; return true;
    }
    static bool keys(void*) { return false; }
};
void shutdown() {
    Mock mock;
    EngineService service({}, Mock::poll, Mock::output, Mock::keys, &mock);
    check(service.wait_status([&](const auto&) { return mock.down.load(); }, std::chrono::seconds(10)), "worker has injected a mock key");
    mock.reject_up = true;
    service.request_stop();
    const auto at_stop = mock.presses.load();
    check(service.suspend() == 0 && service.apply(Config{}, 0) == 0, "shutdown rejects application commands");
    service.resume(); service.runtime_enabled(true); service.power(false);
    service.stop();
    check(mock.presses == at_stop && service.status().stopped, "no key-down after shutdown request and worker joins");
    check(mock.releases == 5 && service.status().engine.cleanup_failed, "failed releases use five bounded attempts");
    Rig rig; rig.tick(); rig.engine.suspend(); rig.tick(); rig.engine.stopping();
    std::string error; check(!rig.engine.apply(Config{}, 0, error), "shutdown rejects config apply during capture");
    rig.engine.runtime_enabled(true); rig.tick(); check(rig.engine.status().mode == MappingMode::Stopping && !rig.events.back(), "shutdown cannot be bypassed");
}
struct GatedPoll {
    std::promise<void> entered, continue_poll;
    std::shared_future<void> release = continue_poll.get_future().share();
    std::atomic_int polls = 0, presses = 0;
    static Snapshot poll(unsigned, void* pointer) {
        auto& self = *static_cast<GatedPoll*>(pointer);
        if (++self.polls == 1) { self.entered.set_value(); self.release.wait(); return {true, button_masks[0]}; }
        return {true};
    }
    static bool output(std::uint16_t, bool down, void* pointer) {
        if (down) ++static_cast<GatedPoll*>(pointer)->presses;
        return true;
    }
};
void in_flight_poll() {
    for (bool stop : {false, true}) {
        GatedPoll mock; auto entered = mock.entered.get_future();
        EngineService engine({}, GatedPoll::poll, GatedPoll::output, Mock::keys, &mock);
        check(entered.wait_for(std::chrono::seconds(10)) == std::future_status::ready, "poll enters controlled gate");
        if (stop) engine.request_stop();
        else { engine.power(true); engine.power(false); engine.power(false); }
        mock.continue_poll.set_value();
        if (!stop) check(engine.wait_status([&](const auto& s) { return mock.polls >= 3 && s.engine.mode == MappingMode::Running; }, std::chrono::seconds(10)), "rapid power notifications drain into one recovery");
        engine.stop();
        check(mock.presses == 0, "cached in-flight poll cannot inject after stop or rapid suspend/resume");
    }
}
}
int main() { lifecycle_and_tray(); power_and_recovery(); shutdown(); in_flight_poll(); std::cout << "Lifecycle tests passed.\n"; }
