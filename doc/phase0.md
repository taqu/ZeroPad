# ZeroPad — Phase 0 Implementation Instructions

## 1. Project Overview

Implement Phase 0 of **ZeroPad**, an open-source, ultra-lightweight Windows application written in modern C++.

ZeroPad converts game controller inputs into keyboard inputs.

The long-term goal is to provide a lightweight, reliable, standalone Windows application with a simple native Win32 configuration interface.

However, **Phase 0 must focus exclusively on establishing a minimal, working input conversion engine**.

Do not implement features planned for later phases.

## 2. Phase 0 Objective

Create a minimal Windows executable that:

1. Detects an XInput-compatible game controller.
2. Monitors the controller's A button.
3. Simulates a Space key press when A is pressed.
4. Simulates a Space key release when A is released.
5. Releases the simulated key when the controller disconnects or the application shuts down normally.
6. Can be built using CMake and MSVC.
7. Includes basic automated tests for input state transitions.

**The primary milestone is a reliable A-button-to-Space-key conversion.**

The implementation must be real and functional, not a prototype using mock controller input.

## 3. Supported Platform

Target platform:

- Windows 11
- x64
- C++20
- MSVC
- CMake

Windows versions that have reached the end of their general support lifecycle do not need to be supported.

Do not introduce compatibility layers for legacy Windows versions.

Use Windows APIs directly whenever appropriate.

Do not depend on:

- Qt
- SDL
- SFML
- GLFW
- Boost
- .NET
- Electron
- WebView2
- Third-party input libraries

The objective is to keep the executable and runtime memory footprint as small as reasonably possible.

## 4. Required Technologies

### 4.1 Controller Input

Use the Windows XInput API.

Requirements:

- Use `XInputGetState`.
- Support controller index 0 for Phase 0.
- Detect the `XINPUT_GAMEPAD_A` button.
- Handle the absence of a connected controller gracefully.
- Detect controller disconnections during execution.
- Avoid excessive CPU usage when no controller is connected.

Do not implement DirectInput, Raw Input, GameInput, or generic HID support.

### 4.2 Keyboard Output

Use the Windows `SendInput` API.

Requirements:

- Use `INPUT_KEYBOARD`.
- Generate proper key-down and key-up events.
- Use `VK_SPACE` for the Space key.
- Use `KEYEVENTF_KEYUP` for key release.
- Check the result of `SendInput` and report failures appropriately.
- Do not repeatedly generate key-down events while the A button remains pressed.

Keyboard events must only be generated when the logical button state changes.

Do not use deprecated APIs such as `keybd_event`.

No custom keyboard driver or virtual controller driver is allowed.

### 4.3 Polling

Implement a simple polling loop.

Use a configurable constant for the polling interval, initially:

`4 ms`

This corresponds to a nominal polling frequency of 250 Hz.

The implementation should:

- Avoid busy waiting.
- Use an appropriate Windows sleep mechanism.
- Prioritize low CPU usage.
- Avoid unnecessarily increasing system-wide timer resolution.
- Accept that ordinary Windows scheduling does not guarantee exact 4 ms polling intervals.

A disconnected controller may be polled at a slower rate, such as 250 ms, to reduce idle CPU usage.

Do not add sophisticated timing or scheduling infrastructure in Phase 0.

## 5. Architecture

Keep the architecture minimal but maintain separation between controller input, input mapping, and keyboard output.

Suggested project structure:

```text
ZeroPad/
├── CMakeLists.txt
├── README.md
├── LICENSE
├── include/
│   └── zeropad/
│       ├── controller.hpp
│       ├── mapper.hpp
│       └── keyboard.hpp
├── src/
│   ├── main.cpp
│   ├── controller.cpp
│   ├── mapper.cpp
│   └── keyboard.cpp
└── tests/
    └── mapper_tests.cpp
```

This structure is a guideline, not a requirement.

Use fewer files if that produces a cleaner implementation.

### Responsibilities

**Controller**

- Read XInput controller state.
- Detect connection and disconnection.
- Expose whether the A button is currently pressed.

**Mapper**

- Track logical button state.
- Detect press and release transitions.
- Avoid duplicate keyboard events.
- Ensure appropriate release transitions are generated when input becomes unavailable.

**Keyboard**

- Send keyboard events through `SendInput`.
- Report injection failures.
- Keep Windows-specific keyboard injection separate from mapping logic.

**Main**

- Initialize the application.
- Run the polling loop.
- Handle shutdown.
- Coordinate key release and resource cleanup.

Avoid introducing generic frameworks, dependency injection containers, complex inheritance hierarchies, or unnecessary abstractions.

Use straightforward modern C++.

## 6. Input State Management

Input state management is particularly important.

The application must track the previous logical state of the controller's A button.

Expected behavior:

| Previous state | Current state | Action |
|---|---|---|
| Released | Released | None |
| Released | Pressed | Space key down |
| Pressed | Pressed | None |
| Pressed | Released | Space key up |

### Controller Disconnection

If the controller disconnects while A is pressed:

1. Detect the disconnection.
2. Generate the required Space key-up event.
3. Reset the controller's logical state.
4. Continue waiting for a controller connection.

A disconnected controller must be treated as having no buttons pressed.

### Application Shutdown

On normal shutdown:

- Release any keyboard keys currently held by ZeroPad.
- Avoid leaving a simulated Space key pressed.
- Exit cleanly.

Provide a way to terminate the console application, such as Ctrl+C.

A Windows console control handler may request shutdown, but it should not perform complex cleanup directly inside the handler. Prefer signaling the main loop to stop and allowing the main loop to perform cleanup.

Forced termination and process crashes cannot be guaranteed to execute cleanup code.

### SendInput Failures

Distinguish between the desired controller state and the keyboard state successfully injected by the application.

If a key-down event fails, do not blindly assume that the key is held.

If a key-up event fails, report the failure and provide a reasonable best-effort recovery strategy.

Avoid infinite retries or high-frequency logging.

## 7. Console Application

Phase 0 may use a simple console application.

The console should provide minimal diagnostic output.

Example:

```text
ZeroPad Phase 0
Waiting for controller...

Controller connected.
A -> Space mapping active.

Controller disconnected.
Waiting for controller...
```

Avoid printing messages every polling iteration.

Do not introduce a logging framework.

Standard C++ output streams or minimal Win32 diagnostics are sufficient.

The application should run without administrator privileges under normal circumstances.

Document that Windows input integrity restrictions and some applications, including certain games, may prevent `SendInput` events from being accepted.

## 8. CMake Requirements

Provide a clean `CMakeLists.txt`.

Requirements:

- Set the C++ standard to C++20.
- Support MSVC.
- Link the required Windows libraries.
- Use the Windows SDK's XInput API and the available system import library.
- Enable reasonable compiler warnings.
- Support Debug and Release builds.
- Enable automated tests through CTest.
- Do not download dependencies during configuration.

Prefer a straightforward configuration that works with Visual Studio 2022 or newer.

Example commands:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Do not add package managers unless strictly necessary.

## 9. Automated Testing

Introduce minimal automated tests for the mapping logic.

Tests must not require a physical controller or generate actual keyboard input.

Separate state transition logic from Windows API calls so it can be tested independently.

Required test cases:

1. Initial released state produces no event.
2. Pressing A produces one key-down event.
3. Holding A produces no additional events.
4. Releasing A produces one key-up event.
5. Repeated released states produce no events.
6. Controller disconnection while A is pressed produces a release.
7. Controller disconnection while A is released produces no event.
8. Reconnecting the controller correctly resumes state tracking.
9. Shutdown while a key is held requests a release.
10. Keyboard injection failures do not incorrectly advance the recorded injected state.

Use a lightweight test approach.

A standalone test executable using standard C++ assertions or explicit checks is acceptable.

Do not introduce a large third-party testing framework solely for Phase 0.

## 10. Coding Standards

Follow these conventions:

- Use C++20.
- Prefer RAII where applicable.
- Use `constexpr` for compile-time constants.
- Use explicit types where they improve readability.
- Use `std::atomic` for shared shutdown flags when appropriate.
- Keep Windows-specific types out of the core mapping logic where practical.
- Avoid global mutable state except where required by minimal Windows callback integration.
- Avoid dynamic allocation in the polling hot path when practical.
- Avoid unnecessary threads.
- Avoid exceptions as part of normal input processing.
- Keep functions small and focused.
- Write comments only where behavior is not obvious.
- Do not overengineer.

Performance, correctness, and maintainability matter more than architectural sophistication.

## 11. Performance Goals

ZeroPad is intended to become an ultra-lightweight background utility.

Phase 0 should establish a suitable foundation.

Preliminary targets:

- Low idle CPU usage.
- No busy-wait loops.
- No avoidable allocations during steady-state polling.
- No unnecessary background threads.
- Small Release executable.
- Small runtime memory footprint.

Long-term aspirational targets:

- Executable size: approximately 1 MB or less.
- Resident memory: approximately 10 MB or less.

These are engineering targets, not strict Phase 0 acceptance criteria.

Do not sacrifice correctness or code clarity merely to achieve an arbitrary binary size.

Measure and report actual binary size if a Release build is available.

## 12. Explicitly Out of Scope

Do not implement any of the following in Phase 0:

- GUI or settings window
- System tray
- TOML configuration
- cpptoml integration
- Configurable key bindings
- Additional controller buttons
- Analog stick mapping
- Trigger mapping
- Mouse emulation
- Multiple controller support
- Controller selection UI
- Profiles
- Macros
- Turbo / rapid-fire
- Windows startup integration
- Automatic updates
- Installers
- Plugins
- Virtual controller drivers
- DirectInput support
- GameInput support

These features belong to later phases.

Do not add placeholder implementations or speculative abstractions for future features.

## 13. README

Create a concise `README.md` containing:

- What ZeroPad is.
- Current Phase 0 functionality.
- Supported platform.
- Build requirements.
- Build instructions.
- How to run the application.
- How to test the A-to-Space mapping.
- How to stop the application.
- Known limitations.
- Planned future development phases, briefly.

Clearly indicate that the project is in an early development stage.

Do not claim that unimplemented features are supported.

## 14. Implementation Workflow

Follow this order:

### Step 1 — Repository Inspection

Inspect the existing repository first.

Identify any existing source files, build configuration, conventions, and license information.

Preserve existing useful code.

Do not overwrite unrelated work.

### Step 2 — Minimal Build

Set up the CMake project.

Verify that a minimal Windows console executable can be built.

### Step 3 — Controller Input

Implement XInput controller detection.

Confirm that the A button's state can be obtained correctly.

### Step 4 — Keyboard Injection

Implement a small keyboard output component using `SendInput`.

Support Space key-down and key-up events.

### Step 5 — State Mapping

Implement edge-triggered input mapping.

Ensure that holding A does not generate repeated key-down events.

### Step 6 — Connection and Shutdown Handling

Handle controller disconnection, reconnection, and graceful shutdown.

Ensure held keys are released on normal shutdown.

### Step 7 — Tests

Implement automated state transition tests.

Keep test execution independent of real controller hardware.

### Step 8 — Documentation and Verification

Update the README.

Build and execute tests where the environment supports Windows and MSVC.

Perform manual controller testing if hardware is available.

If the environment cannot run Windows executables, do not claim that runtime behavior has been verified.

## 15. Definition of Done

Phase 0 is complete only when all of the following are satisfied:

- [ ] CMake project builds successfully on Windows 11 x64 with MSVC.
- [ ] ZeroPad detects an XInput controller on index 0.
- [ ] Pressing A generates Space key-down.
- [ ] Holding A does not generate duplicate key-down events.
- [ ] Releasing A generates Space key-up.
- [ ] Disconnecting the controller releases any held simulated key.
- [ ] Reconnecting the controller works correctly.
- [ ] Graceful shutdown releases any held simulated key.
- [ ] Input injection failures are handled reasonably.
- [ ] Automated state transition tests pass.
- [ ] The application avoids busy waiting.
- [ ] The application runs without administrator privileges in ordinary circumstances.
- [ ] README documents building, usage, testing, and limitations.
- [ ] No out-of-scope features have been implemented.

## 16. Final Deliverables

Provide:

1. Complete C++ source code.
2. CMake build configuration.
3. Automated tests.
4. README documentation.
5. A short implementation summary.
6. Build and test results, including any checks that could not be performed.
7. Known limitations or unresolved issues.

Do not replace implementation with pseudocode.

Do not leave TODO stubs for any required Phase 0 functionality.

If implementation decisions are necessary, choose the simplest correct solution consistent with these requirements.

## 17. Guiding Principle

**ZeroPad should do one thing extremely well before doing many things.**

For Phase 0, that means:

> Read the Xbox controller's A button, translate it into a correctly managed Space key press/release, and do so reliably with minimal code and overhead.

Prioritize correctness, simplicity, and native Windows integration over additional features.

Implement Phase 0 completely, verify what you can, and stop. Do not proceed to Phase 1 automatically.

