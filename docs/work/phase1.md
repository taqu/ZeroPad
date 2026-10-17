# ZeroPad — Phase 1 Implementation Instructions

## 1. Project Overview

ZeroPad is an open-source, ultra-lightweight Windows utility written in modern C++.

It converts XInput-compatible game controller inputs into keyboard inputs using native Windows APIs.

The project prioritizes:

1. Correct and reliable input conversion.
2. Minimal CPU and memory overhead.
3. Simple, maintainable C++ code.
4. Native Windows integration.
5. No unnecessary dependencies.
6. Predictable behavior under unusual input conditions.

Phase 0 implemented the initial A-button-to-Space-key conversion.

**Phase 1 must extend the existing input engine to support standard controller inputs while preserving Phase 0 functionality.**

Do not rewrite the application from scratch unless the existing implementation has fundamental correctness problems that cannot reasonably be addressed incrementally.

## 2. Phase 1 Objectives

Implement the following capabilities:

1. All standard XInput digital buttons.
2. Left and right analog triggers converted to digital key states.
3. Left and right analog sticks converted to directional key states.
4. Deadzone handling.
5. Hysteresis to prevent unstable state transitions.
6. Multiple simultaneous controller inputs.
7. Safe handling of multiple inputs mapped to the same keyboard key.
8. Correct key release during disconnection and shutdown.
9. Comprehensive automated tests for the mapping engine.

Phase 1 must remain a console-based utility.

Do not implement a GUI, configuration file system, or system tray.

## 3. Technical Constraints

Use the existing project conventions wherever possible.

Required technologies:

- Windows 11 x64
- C++20
- MSVC
- CMake
- XInput
- SendInput
- Standard C++ library

Do not introduce:

- SDL
- Qt
- Boost
- GLFW
- DirectInput
- GameInput
- Raw Input
- Third-party mapping frameworks
- External runtime dependencies

Continue using XInput controller index 0.

Do not implement multiple-controller support in this phase.

Keep the executable small and avoid allocations within the polling hot path where practical.

Do not introduce unnecessary threads or background services.

## 4. Supported Controller Inputs

Support all of the following logical inputs.

### 4.1 Digital Buttons

| Logical input | XInput source |
|---|---|
| A | XINPUT_GAMEPAD_A |
| B | XINPUT_GAMEPAD_B |
| X | XINPUT_GAMEPAD_X |
| Y | XINPUT_GAMEPAD_Y |
| DPAD_UP | XINPUT_GAMEPAD_DPAD_UP |
| DPAD_DOWN | XINPUT_GAMEPAD_DPAD_DOWN |
| DPAD_LEFT | XINPUT_GAMEPAD_DPAD_LEFT |
| DPAD_RIGHT | XINPUT_GAMEPAD_DPAD_RIGHT |
| LB | XINPUT_GAMEPAD_LEFT_SHOULDER |
| RB | XINPUT_GAMEPAD_RIGHT_SHOULDER |
| START | XINPUT_GAMEPAD_START |
| BACK | XINPUT_GAMEPAD_BACK |
| LS_BUTTON | XINPUT_GAMEPAD_LEFT_THUMB |
| RS_BUTTON | XINPUT_GAMEPAD_RIGHT_THUMB |

Use the standard XInput button bitmask.

Do not interpret undefined or reserved bits.

### 4.2 Analog Triggers

Support:

- LT
- RT

Trigger input values range from 0 to 255.

Convert them into logical pressed/released states using thresholds.

Initial suggested thresholds:

- Press: 40
- Release: 25

A trigger becomes pressed when its value reaches or exceeds the press threshold.

A pressed trigger remains pressed until its value reaches or falls below the release threshold.

Threshold values must be defined as named constants or a small configuration structure.

They are not user-configurable in Phase 1.

Validate that:

`0 <= release_threshold < press_threshold <= 255`

### 4.3 Analog Sticks

Support:

- LS_UP
- LS_DOWN
- LS_LEFT
- LS_RIGHT
- RS_UP
- RS_DOWN
- RS_LEFT
- RS_RIGHT

Each directional input must have an independent logical state.

Analog stick values use signed 16-bit coordinates.

XInput axis conventions:

- Negative X = left
- Positive X = right
- Negative Y = down
- Positive Y = up

Implement directional digital conversion.

Initial suggested thresholds:

- Press threshold: 0.25 of full-scale axis magnitude.
- Release threshold: 0.20 of full-scale axis magnitude.

Use these as initial defaults, not universal hardware requirements.

Normalize signed axis values carefully, including the asymmetric range of -32768 to +32767.

Avoid overflow when handling -32768.

Prefer axis-aligned threshold detection for this phase.

Do not introduce radial deadzone processing, analog response curves, or analog-to-mouse conversion.

### 4.4 Diagonal Input

Diagonal stick movement must work naturally.

For example:

- Moving the left stick diagonally upward-right should activate both LS_UP and LS_RIGHT.
- Moving the stick back to center should release both.

Do not restrict the stick to a single active direction.

Opposite directions on the same axis should be mutually exclusive.

Avoid emitting unnecessary key transitions when moving directly between opposite directions.

## 5. Fixed Keyboard Mapping

Phase 1 does not include a configuration system.

Define a fixed default mapping in C++.

Suggested initial mapping:

| Controller input | Keyboard output |
|---|---|
| A | Space |
| B | Escape |
| X | E |
| Y | R |
| DPAD_UP | Up |
| DPAD_DOWN | Down |
| DPAD_LEFT | Left |
| DPAD_RIGHT | Right |
| LB | Q |
| RB | F |
| START | Enter |
| BACK | Tab |
| LS_BUTTON | Left Shift |
| RS_BUTTON | Left Control |
| LT | Z |
| RT | X |
| LS_UP | W |
| LS_DOWN | S |
| LS_LEFT | A |
| LS_RIGHT | D |
| RS_UP | I |
| RS_DOWN | K |
| RS_LEFT | J |
| RS_RIGHT | L |

These mappings are initial defaults for testing.

Keep them in one centralized location rather than distributing keyboard key constants throughout the input logic.

A small compile-time mapping table is recommended.

Do not implement TOML parsing or runtime key-binding editing.

The mapping representation should be straightforward to consume by the future configuration layer, without building that layer now.

**Preserve A → Space from Phase 0.**

## 6. Input Processing Architecture

Separate the processing pipeline into three logical stages:

```text
XInput Controller State
          |
          v
Logical Controller Inputs
          |
          v
Keyboard Mapping Engine
          |
          v
Keyboard State Transitions
          |
          v
Windows SendInput
```

The implementation may use the existing files and classes from Phase 0.

Suggested responsibilities:

### Controller

- Poll XInput.
- Report connection state.
- Provide a controller snapshot.
- Avoid keyboard-related logic.

### Logical Input Processing

- Interpret digital button bitmasks.
- Apply trigger thresholds.
- Apply stick thresholds.
- Maintain hysteresis state.
- Produce logical pressed/released states.

### Mapper

- Resolve logical inputs to keyboard keys.
- Support simultaneous inputs.
- Track the desired state of each mapped keyboard key.
- Generate only necessary key transitions.

### Keyboard

- Inject key-down and key-up events through SendInput.
- Track successfully submitted keyboard events.
- Handle injection failures.

Keep the architecture minimal.

Do not introduce elaborate inheritance hierarchies or generic event-processing frameworks.

A small number of structs and classes is sufficient.

## 7. Keyboard State Management

This is one of the most important Phase 1 requirements.

Multiple controller inputs may map to the same keyboard key.

The application must not release a key while another controller input still requires it.

### 7.1 Example

Suppose both A and B map to Space.

Sequence:

1. A pressed → Space down.
2. B pressed → no additional Space down.
3. A released → Space remains down.
4. B released → Space up.

The implementation must behave correctly regardless of the order in which controller inputs are pressed or released.

### 7.2 Recommended Design

Maintain a logical desired-key state independently from the successfully injected-key state.

For each keyboard key:

- Determine whether any active controller input requires that key.
- Compare the desired state with the recorded injected state.
- Generate a transition only when they differ.
- Update the injected state only after successful submission.

A small reference-counting implementation is acceptable.

Alternatively, derive each key's desired state from the active logical inputs on every polling iteration.

**Prefer deriving desired states over maintaining unnecessary mutable counters if doing so keeps the code simpler.**

### 7.3 Duplicate Key Events

Avoid duplicate key-down and key-up events.

For each key, generate:

- One key-down when its desired state changes from released to pressed.
- One key-up when its desired state changes from pressed to released.

Do not repeatedly inject events for unchanged states.

### 7.4 Simultaneous Inputs

Support combinations such as:

- A + B
- LT + RT
- LS_UP + LS_RIGHT
- A + LT + LS_UP
- Multiple D-Pad directions
- Stick directions and digital buttons simultaneously

Do not artificially limit the number of simultaneously active logical inputs.

## 8. Analog Input Stability

Analog controller inputs can fluctuate near their neutral position or threshold boundaries.

Prevent unstable key transitions.

### 8.1 Hysteresis

Implement separate press and release thresholds.

Example:

```text
Neutral
   |
   | value >= 0.25
   v
Pressed
   |
   | value <= 0.20
   v
Released
```

Values between the two thresholds preserve the previous logical state.

Apply this independently to:

- LT
- RT
- Each stick direction

### 8.2 Neutral Position

Small stick movements must not generate keyboard events.

When a stick returns to its neutral position, all associated directional keys must be released.

### 8.3 Threshold Boundaries

Define exact inclusive/exclusive threshold behavior.

Ensure the implementation and tests agree at the threshold boundaries.

Use named constants rather than unexplained numeric literals.

### 8.4 Rapid Direction Changes

Test cases such as:

- Left → Center → Right
- Up → Down
- Up-right → Up-left
- Trigger pressed → threshold boundary → released

Avoid spurious additional key events.

## 9. Disconnection and Reconnection

Preserve and extend Phase 0 disconnection handling.

When the controller disconnects:

1. Treat every logical controller input as released.
2. Update desired keyboard states accordingly.
3. Attempt to release all keys held by ZeroPad.
4. Reset analog hysteresis states.
5. Continue checking for controller reconnection.

Do not leave keys held after a normal disconnection.

On reconnection:

- Begin with fresh controller state.
- Read the current buttons and axes.
- Apply the same transition rules as normal operation.
- Avoid replaying stale states from before disconnection.

Use a slower polling interval when disconnected if already supported by Phase 0.

Do not repeatedly log the same disconnection state.

## 10. Keyboard Injection Failures

Continue using SendInput.

Check its return value.

For each keyboard key, maintain the distinction between:

- Desired key state.
- Last successfully submitted key state.

If a key-down submission fails, do not record a successful key-down.

If a key-up submission fails, do not record a successful key-up.

Use bounded or rate-limited retry behavior.

Avoid uncontrolled retry loops.

Retry only when an outstanding state mismatch still requires an event.

Do not replay stale key-down events after the controller has been disconnected or released.

If a release failure cannot be resolved immediately, retain sufficient state to retry it rather than silently forgetting that a release is needed.

Do not claim that SendInput success guarantees a target application consumed the event.

Windows input integrity restrictions must remain documented.

## 11. Shutdown and Cleanup

Ensure graceful shutdown releases all keys successfully injected by ZeroPad.

Requirements:

- Respect normal console shutdown requests.
- Stop generating new controller-driven key-down events.
- Force the desired keyboard state to released.
- Attempt to submit the necessary key-up events.
- Apply a bounded best-effort retry policy for failed releases.
- Report unresolved cleanup failures.
- Terminate without indefinite blocking.

Do not assume that destructors or console handlers will run during forced process termination.

Do not perform complex input processing directly inside a console control callback.

## 12. Performance Requirements

Preserve ZeroPad's ultra-lightweight design.

Requirements:

- No busy-wait loops.
- No avoidable heap allocations in the polling hot path.
- No per-frame logging.
- No per-frame dynamic mapping construction.
- No unnecessary background threads.
- No periodic processing unrelated to controller input.

A simple array-based design is encouraged.

The number of supported logical controller inputs is small and fixed.

Prefer fixed-size containers such as `std::array` where appropriate.

Avoid introducing `std::map`, `std::unordered_map`, or dynamic allocation unless there is a clear benefit.

Keep the existing polling interval unless measurements justify changing it.

Do not modify system-wide timer resolution merely to improve an unmeasured latency target.

## 13. Automated Tests

Expand the Phase 0 test suite.

Tests must execute without:

- Physical controller hardware.
- Actual keyboard injection.
- A GUI.
- Administrator privileges.

Provide synthetic controller snapshots and a mock keyboard output interface.

### 13.1 Digital Input Tests

Verify:

1. Each digital button is recognized.
2. Press produces the expected key-down.
3. Release produces the expected key-up.
4. Holding a button does not repeat key-down.
5. Multiple buttons work simultaneously.
6. Button changes in the same polling snapshot are handled correctly.

### 13.2 Trigger Tests

Verify:

1. Values below the press threshold remain released.
2. Crossing the press threshold generates key-down.
3. Values within the hysteresis band preserve the previous state.
4. Crossing the release threshold generates key-up.
5. LT and RT operate independently.
6. Values 0 and 255 are handled correctly.
7. Repeated threshold fluctuations do not produce chatter.

### 13.3 Stick Tests

Verify:

1. Neutral stick produces no events.
2. Up, down, left, and right work.
3. Diagonal movement activates two directions.
4. Returning to neutral releases active keys.
5. Direct opposite-direction movement works.
6. Both sticks work independently.
7. Hysteresis prevents boundary chatter.
8. Signed minimum and maximum axis values are handled safely.
9. Stick clicks remain independent from stick movement.

### 13.4 Duplicate Mapping Tests

Create test mappings where two or more controller inputs share the same keyboard key.

Verify:

1. First active input generates key-down.
2. Additional active inputs do not generate duplicate key-down.
3. Releasing one input does not release the shared key.
4. Releasing the final active input generates key-up.
5. Press and release changes occurring in the same snapshot do not create unnecessary release/repress events.
6. Disconnection releases shared keys exactly once when injection succeeds.

### 13.5 Failure Recovery Tests

Verify:

1. Failed key-down does not incorrectly mark the key as injected.
2. Failed key-up remains pending.
3. Subsequent successful retry reconciles the state.
4. Multiple failed keys are handled independently.
5. Controller disconnection does not leave pending key-down requests.
6. Shutdown attempts to release all held keys.
7. Retry behavior is bounded and does not busy-loop.

### 13.6 Regression Tests

All Phase 0 tests must continue to pass.

A → Space behavior must remain unchanged.

Use CTest for automated test execution.

Do not add a large third-party test framework.

## 14. Suggested Implementation Sequence

Follow this sequence.

### Step 1 — Inspect Existing Code

Read the Phase 0 implementation.

Identify existing:

- Controller polling logic.
- Mapping logic.
- Keyboard injection code.
- Shutdown handling.
- Tests.
- CMake configuration.

Reuse the existing design where appropriate.

### Step 2 — Define Logical Inputs

Introduce a compact representation of the supported controller inputs.

An enum and fixed-size arrays are recommended.

Ensure the representation includes all digital buttons, triggers, and stick directions.

### Step 3 — Implement Digital Button Processing

Support all standard XInput digital buttons.

Verify that the existing A → Space behavior remains correct.

### Step 4 — Implement Trigger Processing

Add LT and RT.

Implement press/release thresholds and hysteresis.

Add trigger tests.

### Step 5 — Implement Stick Processing

Add both analog sticks.

Implement directional conversion with thresholds and hysteresis.

Add stick tests.

### Step 6 — Generalize Keyboard Mapping

Replace the Phase 0 hardcoded A-only mapping with the fixed Phase 1 mapping table.

Implement aggregate desired-key state calculation.

Ensure duplicate mappings are safe.

### Step 7 — Improve Keyboard State Tracking

Extend the keyboard injection logic to support multiple simultaneously held keys.

Handle failures and retries without losing state consistency.

### Step 8 — Verify Disconnect and Shutdown

Ensure all held keys are released during normal disconnection and shutdown.

Verify reconnection behavior.

### Step 9 — Expand Automated Tests

Implement the tests described above.

Run all Phase 0 and Phase 1 tests.

### Step 10 — Update Documentation

Update README and any relevant developer documentation.

Keep the documentation concise.

## 15. Explicitly Out of Scope

Do not implement:

- Win32 settings UI
- Gamepad diagrams or graphical controller visualization
- System tray integration
- TOML configuration
- cpptoml integration
- User-editable bindings
- Multiple profiles
- Mouse movement
- Mouse buttons
- Macros
- Turbo / rapid-fire
- Automatic Windows startup
- Multiple simultaneous controllers
- GameInput
- DirectInput
- Raw HID
- Custom drivers
- Background Windows services
- Automatic update systems

Do not implement Phase 2 or later features.

Do not add placeholder classes for them.

## 16. Definition of Done

Phase 1 is complete when:

- [ ] All 14 XInput digital buttons work.
- [ ] Both analog triggers work.
- [ ] Both analog sticks support four directional inputs each.
- [ ] Hysteresis prevents unstable analog state transitions.
- [ ] Diagonal stick movement works.
- [ ] Multiple simultaneous inputs work.
- [ ] Duplicate keyboard mappings do not generate incorrect releases.
- [ ] Unchanged logical states do not generate redundant keyboard events.
- [ ] Controller disconnection releases held keys under normal operating conditions.
- [ ] Controller reconnection restores correct input processing.
- [ ] Graceful shutdown attempts to release all held keys.
- [ ] Failed SendInput operations do not silently corrupt tracked state.
- [ ] The mapping engine is testable without Windows input injection.
- [ ] All Phase 0 regression tests pass.
- [ ] All new Phase 1 tests pass.
- [ ] CMake builds successfully on Windows 11 x64.
- [ ] README describes the implemented functionality.
- [ ] No out-of-scope functionality has been added.

## 17. Final Deliverables

Provide:

1. Updated C++ source code.
2. Updated headers, if needed.
3. Updated CMake configuration, if needed.
4. Expanded automated tests.
5. Updated README.
6. Build results.
7. Test results.
8. A concise summary of architectural changes.
9. Any unresolved issues or known limitations.

If the development environment does not support Windows builds or physical controller testing, clearly distinguish verified results from unverified behavior.

Do not claim tests passed unless they were actually executed successfully.

## 18. Final Engineering Principles

Maintain the following principles throughout implementation:

**Correctness before optimization.**

Never compromise keyboard state consistency for minor performance improvements.

**Small and predictable state machines.**

Prefer simple, explicit state transitions over generalized processing frameworks.

**No unnecessary abstractions.**

ZeroPad is a focused input conversion utility, not a general-purpose input framework.

**No unnecessary dependencies.**

Prefer C++20 and native Windows APIs.

**Preserve Phase 0 reliability.**

Existing functionality must continue to work.

**Stop at Phase 1.**

Implement and verify Phase 1 completely, then stop.

Do not begin Phase 2 automatically.
