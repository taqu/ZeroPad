#include "settings.h"
#include "engine_service.h"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
using namespace zeropad;
namespace {
void check(bool condition, const char* description) {
    if (!condition) { std::cerr << "FAIL: " << description << '\n'; std::exit(EXIT_FAILURE); }
}
struct Fixture {
    EngineCore engine;
    Keyboard::Clock::time_point now{};
    std::vector<std::pair<std::uint16_t, bool>> events;
    bool fail_up = false;
    Snapshot controller{true, button_masks[0]};
    static bool output(std::uint16_t key, bool down, void* ptr) {
        auto& self = *static_cast<Fixture*>(ptr);
        if (!down && self.fail_up) return false;
        self.events.emplace_back(key, down); return true;
    }
    void tick(bool physical_down = false) { engine.step(controller, physical_down, now, output, this); }
    void advance() { now += Keyboard::retry_interval; }
    void suspend() { engine.suspend(); tick(); }
    void resume(bool physical = false) { engine.resume(now); tick(physical); }
};
void editing_and_capture() {
    Fixture f;
    DiskSnapshot disk{true, serialize_config(f.engine.status().config)};
    SettingsEditor editor(f.engine.status(), disk);
    editor.clear(0);
    check(editor.editable.bindings[0] == 0 && f.engine.status().config.bindings[0] == 0x20, "editable copy cannot mutate active config");
    editor.cancel(); check(!editor.modified() && editor.editable == Config{}, "Cancel discards edits");
    editor.editable.general.enabled = false; editor.defaults();
    check(editor.editable == Config{}, "centralized defaults");
    check(input_display_names.size() == input_count, "every input has a display name");
    for (std::size_t i = 0; i < input_count; ++i) {
        check(!input_display_names[i].empty() && !input_names[i].empty(), "stable mapping model");
        check(editor.assign(i, 'A'), "all rows editable");
    }
    for (unsigned key = 0; key < 256; ++key) {
        if (key_name(static_cast<std::uint16_t>(key)).empty()) continue;
        check(editor.assign(0, static_cast<std::uint16_t>(key)), "every canonical key selectable");
        check(parse_key_name(key_name(editor.editable.bindings[0])) == key, "selected key serializable");
    }
    const auto previous = editor.editable;
    check(!editor.assign(0, 0x5B) && !editor.assign(input_count, 'X') && editor.editable == previous, "unsupported assignment rejected");
    check(captured_key('X', 0, false) == 'X', "letter capture");
    check(captured_key(0x10, 0x2A, false) == 0xA0 && captured_key(0x10, 0x36, false) == 0xA1, "left and right Shift");
    check(captured_key(0x11, 0, false) == 0xA2 && captured_key(0x11, 0, true) == 0xA3, "left and right Control");
    check(captured_key(0x12, 0, false) == 0xA4 && captured_key(0x12, 0, true) == 0xA5, "left and right Alt");
    for (unsigned key : {0x09u, 0x0Du, 0x1Bu, 0x20u, 0x70u, 0x7Bu, 0x21u, 0x2Eu})
        check(captured_key(key, 0, false) == key, "special capture key");
    check(!captured_key('A', 0, false, true), "identified injected key rejected");
    check(!captured_key(0x5B, 0, false) && !captured_key(0, 0, false) && !captured_key(0x10, 0, false), "unsupported physical input rejected");
    int integer = 7; double floating = .25;
    check(read_number(L"100", integer) && integer == 100, "integer edit conversion");
    check(read_number(L"0.1234567890123456", floating) && floating == .1234567890123456, "threshold edit conversion");
    check(!read_number(L"4.0", integer) && !read_number(L"99999999999999999999999", integer), "wrong integer and overflow rejected");
    check(!read_number(L"nan", floating) && !read_number(L"inf", floating) && !read_number(L"0,25", floating), "non-finite and locale text rejected");
    check(!read_number(L"", floating) && !read_number(L"0.2tail", floating), "invalid numeric text rejected");
}
void suspension() {
    Fixture f; f.tick(); check(f.events.size() == 1, "initial mapped key down");
    f.fail_up = true; f.suspend();
    check(f.engine.status().suspended && !f.engine.status().safe, "failed release prevents capture readiness");
    for (int i = 0; i < 20; ++i) f.tick();
    check(f.events.size() == 1, "suspended engine creates no new presses");
    f.fail_up = false; f.advance(); f.tick();
    check(f.engine.status().safe && f.events.size() == 2 && !f.events.back().second, "held keys released before capture");
    const auto before = f.engine.status().config;
    f.resume(true);
    check(f.engine.status().suspended && f.events.size() == 2, "capture key held blocks resume");
    f.now += std::chrono::seconds(3); f.tick(true);
    check(f.engine.status().resume_blocked, "bounded resume policy reports held-key timeout");
    f.tick(false); check(f.engine.status().suspended, "timeout requires explicit Resume");
    f.resume(); check(!f.engine.status().suspended && f.events.size() == 3, "explicit Resume uses fresh controller snapshot");
    check(f.engine.status().config == before, "capture cancel does not change config");
    f.suspend(); f.controller = {}; f.resume();
    check(!f.engine.status().suspended && f.events.size() == 4, "dialog destruction cleanup resumes without stale input");
    f.controller = {true}; f.controller.ly = 7000; f.tick();
    check(f.events.size() == 4, "resume resets analog hysteresis");
}
void conflicts_and_no_change() {
    Fixture f; f.tick();
    DiskSnapshot disk{true, serialize_config(Config{})};
    SettingsEditor editor(f.engine.status(), disk);
    bool unchanged = false; std::string error;
    check(editor.check_save(f.engine.status(), disk, unchanged, error) && unchanged, "no-change Save detected");
    f.tick(); check(f.events.size() == 1, "no-change Save needs no release/repress");
    auto changed = f.engine.status(); ++changed.revision;
    check(!editor.check_save(changed, disk, unchanged, error), "external active revision conflict");
    auto external = disk; external.bytes += "# external edit\n";
    check(!editor.check_save(f.engine.status(), external, unchanged, error), "external disk edit conflict");
    editor.editable.general.polling_interval_ms = 0;
    check(!editor.check_save(f.engine.status(), disk, unchanged, error) && f.engine.status().config == Config{}, "invalid Save never mutates engine");
}
struct Directory {
    std::filesystem::path path;
    Directory() {
        path = std::filesystem::temp_directory_path() / ("zeropad-settings-" + std::to_string(Keyboard::Clock::now().time_since_epoch().count()));
        check(std::filesystem::create_directory(path), "temporary test directory");
    }
    ~Directory() {
        // Only this explicitly created test directory is removed.
        std::error_code error;
        if (path.parent_path() == std::filesystem::temp_directory_path() && path.filename().string().starts_with("zeropad-settings-"))
            std::filesystem::remove_all(path, error);
    }
};
void save_apply() {
    Directory dir; const auto path = dir.path / "config.toml";
    Fixture f; std::string error;
    check(save_config(path, Config{}, error), "initial persistence");
    DiskSnapshot disk; check(inspect_config_file(path, disk, error), "disk baseline");
    SettingsEditor editor(f.engine.status(), disk);
    editor.editable.bindings[0] = editor.editable.bindings[1] = 'P';
    check(!editor.persist(path, f.engine.status(), error), "cannot persist changed settings before suspension");
    f.tick(); f.fail_up = true; f.suspend();
    check(!editor.persist(path, f.engine.status(), error), "failed release prevents Save");
    check(!f.engine.apply(editor.editable, editor.revision(), error), "unsafe engine transition rejected");
    f.fail_up = false; f.advance(); f.tick();
    bool wrote = false;
    check(!editor.persist(path, f.engine.status(), error, [&](const auto&, const auto&, auto& message) {
        wrote = true; message = "Simulated write failure"; return false;
    }) && wrote, "persistence failure reported");
    Config persisted; check(load_config(path, persisted, error) == LoadStatus::Loaded && persisted == Config{}, "failed persistence leaves disk and engine unchanged");
    check(editor.persist(path, f.engine.status(), error), "successful safe persistence");
    check(!f.engine.apply(editor.editable, editor.revision() + 1, error), "activation failure is explicit");
    check(f.engine.status().safe && f.engine.status().config == Config{}, "activation failure leaves safe old config");
    check(load_config(path, persisted, error) == LoadStatus::Loaded && persisted == editor.editable, "saved configuration retained after activation failure");
    check(f.engine.apply(editor.editable, editor.revision(), error), "retry activates persisted candidate");
    f.tick(); check(f.engine.status().config == editor.editable && f.engine.status().revision == 1, "new config acknowledged before success");
    f.resume(); const auto event_count = f.events.size();
    f.controller.buttons = button_masks[0] | button_masks[1]; f.tick(); f.controller.buttons = button_masks[1]; f.tick();
    check(f.events.size() == event_count, "duplicate keys safe after GUI save");
    f.suspend(); auto candidate = f.engine.status().config;
    candidate.controller.index = 3; candidate.analog.stick_press_threshold = .8; candidate.general.enabled = false;
    check(f.engine.apply(candidate, 1, error), "controller and analog changes queued"); f.tick(); f.resume();
    check(f.engine.status().config == candidate && f.events.size() == event_count + 1, "disabled settings release keys and reset controller state");
    f.suspend(); candidate.general.enabled = true;
    check(f.engine.apply(candidate, 2, error), "enable new candidate"); f.tick(); f.resume();
    check(f.events.back().second, "reenable mapping");
    SettingsEditor latest(f.engine.status(), {true, serialize_config(persisted)});
    { std::ofstream file(path, std::ios::app); file << "# another editor\n"; }
    check(!latest.persist(path, f.engine.status(), error), "final disk conflict recheck");
}
struct Devices {
    std::atomic_uint poll_count = 0;
    std::atomic_uint last_index = 0;
    std::atomic_bool held = false;
    static Snapshot poll(unsigned index, void* pointer) {
        auto& self = *static_cast<Devices*>(pointer); ++self.poll_count; self.last_index.store(index);
        return {true, button_masks[0]};
    }
    static bool output(std::uint16_t, bool down, void* pointer) { static_cast<Devices*>(pointer)->held.store(down); return true; }
    static bool keys(void*) { return false; }
};
void threading() {
    Devices devices;
    {
        EngineService service({}, Devices::poll, Devices::output, Devices::keys, &devices);
        const auto pause = service.suspend();
        check(service.wait_status([&](const auto& s) { return s.pause_acknowledged == pause && s.engine.safe; }, std::chrono::seconds(10)), "worker acknowledges suspension through synchronized snapshot");
        auto candidate = service.status().engine.config; candidate.controller.index = 2; candidate.bindings[0] = 'M';
        const auto request = service.apply(candidate, 0);
        check(service.wait_status([&](const auto& s) { return s.completed_request == request && s.engine.config == candidate; }, std::chrono::seconds(10)), "configuration transferred and activated on worker");
        service.resume();
        check(service.wait_status([](const auto& s) { return !s.engine.suspended; }, std::chrono::seconds(10)), "worker resumes after command");
        service.stop();
        check(service.status().stopped && !service.status().engine.cleanup_failed && !devices.held.load(), "shutdown joins worker and releases keys");
        check(devices.poll_count > 0 && devices.last_index == 2, "single worker polls configured controller");
    }
}
}
int main() {
    editing_and_capture(); suspension(); conflicts_and_no_change(); save_apply(); threading();
    std::cout << "All settings tests passed.\n";
}
