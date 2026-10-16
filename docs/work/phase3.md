# ZeroPad — Phase 3 Implementation Instructions

## 1. Project Overview

ZeroPad is an open-source, ultra-lightweight Windows utility written in modern C++.

Its purpose is to convert XInput-compatible game controller inputs into keyboard inputs with minimal overhead.

The project uses:

- C++20
- Windows 11 x64
- MSVC
- CMake
- XInput
- SendInput
- Native Win32 APIs
- taqu/cpptoml for TOML configuration parsing

The previous phases established:

**Phase 0:** Basic A-button-to-Space-key conversion.

**Phase 1:** Full standard XInput input processing, analog thresholds, hysteresis, multiple simultaneous inputs, and safe keyboard state management.

**Phase 2:** TOML configuration, validation, persistence, runtime reload, and safe configuration application.

**Phase 3 must implement a simple native Windows settings interface on top of the existing configuration system.**

The GUI must not replace, duplicate, or significantly restructure the existing input engine.

## 2. Core Design Philosophy

ZeroPad does not need a visually modern interface.

The priorities are:

1. Clarity.
2. Simplicity.
3. Reliability.
4. Low resource usage.
5. Native Windows behavior.
6. Keyboard accessibility.
7. Minimal implementation complexity.

Use ordinary Windows UI controls.

Do not implement:

- Graphical gamepad diagrams.
- Controller illustrations.
- Custom visual themes.
- Animations.
- Custom-rendered buttons.
- Complex navigation.
- Skins.
- Dashboard layouts.
- Browser-based interfaces.

Users should understand the configuration interface without learning a new interaction model.

**The settings window should resemble a straightforward Windows utility, not a game launcher or controller visualization application.**

## 3. Phase 3 Objectives

Implement:

1. A native Win32 settings window.
2. A controller selection control.
3. Controller connection status.
4. A table of all logical controller inputs and their assigned keyboard keys.
5. Keyboard binding editing.
6. Direct physical keyboard key capture.
7. An alternative key selection method.
8. Binding removal.
9. Default configuration restoration.
10. Editing of existing analog and polling settings.
11. Enable/disable control.
12. Save and Cancel behavior.
13. Integration with Phase 2 configuration management.
14. Safe application of configuration changes.
15. High-DPI support.
16. Keyboard navigation and basic accessibility.
17. Automated tests for UI-independent behavior.

Do not implement system tray integration.

That belongs to Phase 4.

## 4. Technical Requirements

Target:

- Windows 11 x64
- C++20
- MSVC
- CMake

Use:

- Win32 API
- Common Controls
- Standard Windows dialog controls
- Existing ZeroPad configuration classes

Recommended APIs and controls:

- `CreateWindowExW`
- `DialogBoxParamW` or `CreateDialogParamW`
- `InitCommonControlsEx`
- `WC_LISTVIEWW`
- `WC_COMBOBOXW`
- `WC_BUTTONW`
- `WC_STATICW`
- `WM_COMMAND`
- `WM_NOTIFY`
- `WM_DPICHANGED`

Prefer Unicode Windows APIs.

Do not introduce:

- Qt
- wxWidgets
- MFC
- WTL
- Dear ImGui
- WebView2
- Electron
- SDL
- Direct2D rendering infrastructure
- Third-party UI libraries

Use the Windows SDK already provided by the MSVC toolchain.

## 5. Window Design

Implement one primary settings window.

Suggested layout:

```text
+----------------------------------------------------+
| ZeroPad - Settings                                 |
+----------------------------------------------------+
|                                                    |
| Controller: [ Controller 1              v ]        |
| Status:     Connected                              |
|                                                    |
| [x] Enable keyboard mapping                        |
|                                                    |
| Controller Input           Keyboard                 |
| -------------------------------------------------- |
| Button A                   Space                    |
| Button B                   Escape                   |
| Button X                   E                        |
| Button Y                   R                        |
| D-Pad Up                   Up                       |
| D-Pad Down                 Down                     |
| Left Bumper                Q                        |
| Right Bumper               F                        |
| Left Trigger               Z                        |
| Right Trigger              X                        |
| Left Stick Up              W                        |
| Left Stick Down            S                        |
| ...                                                |
|                                                    |
| [Change Key...] [Clear Binding]                    |
|                                                    |
| Analog Settings                                    |
| Stick Press:   [0.25]  Release: [0.20]             |
| Trigger Press: [40]    Release: [25]               |
|                                                    |
| Polling Interval: [4] ms                           |
|                                                    |
| [Defaults]                     [Cancel] [Save]      |
+----------------------------------------------------+
```

This is a conceptual layout.

Adapt dimensions to fit the actual controls.

### Requirements

- Use one primary window.
- Avoid tabbed interfaces unless absolutely necessary.
- Avoid unnecessary separate settings pages.
- Use conventional Windows spacing.
- Ensure every setting is visible or easily reachable.
- Use a resizable window if it simplifies practical use.
- Avoid horizontal scrolling in the binding table.
- Provide sensible minimum window dimensions.

If the available vertical space is insufficient, use a straightforward scrolling or grouping arrangement.

Do not introduce a custom layout framework.

## 6. Controller Selection

Provide a standard ComboBox with four controller slots:

- Controller 1
- Controller 2
- Controller 3
- Controller 4

These correspond to XInput indices 0–3.

The selected controller index must be loaded from the active configuration.

Changing the ComboBox selection must update only the editable configuration until Save is pressed.

### Connection Status

Display a short status:

- Connected
- Disconnected
- Checking

Connection status should refer to the controller selected in the UI.

Do not confuse the editable selection with the controller currently used by the running input engine.

If the selected controller differs from the active controller because of unsaved changes, display the status for the selected controller without changing the active mapping.

Update status at a modest interval, such as 250–500 ms.

Do not update the GUI every controller polling cycle.

Do not perform blocking device discovery or expensive operations inside the window procedure.

Do not interfere with the input engine's own polling.

## 7. Keyboard Mapping List

Use the standard Win32 ListView control.

Recommended styles:

- `LVS_REPORT`
- `LVS_SINGLESEL`
- `LVS_SHOWSELALWAYS`
- `LVS_EX_FULLROWSELECT`

Create two columns:

1. Controller Input
2. Keyboard

Display every logical input supported by Phase 1.

### Display Names

Use descriptive names rather than internal identifiers.

Examples:

| Internal identifier | Display name |
|---|---|
| A | Button A |
| B | Button B |
| DPAD_UP | D-Pad Up |
| LB | Left Bumper (LB) |
| RB | Right Bumper (RB) |
| LT | Left Trigger (LT) |
| RT | Right Trigger (RT) |
| LS_BUTTON | Left Stick Click |
| RS_BUTTON | Right Stick Click |
| LS_UP | Left Stick Up |
| RS_RIGHT | Right Stick Right |

Use a centralized mapping between:

- Internal input identifier.
- Human-readable display name.
- Current keyboard binding.

Do not duplicate controller identifiers throughout the UI source.

### Requirements

- Display all supported inputs.
- Preserve a stable, predictable row order.
- Use ordinary text.
- Do not draw gamepad icons.
- Do not use colored key badges.
- Do not use in-place free-text editing for key names.
- Do not allow the input-name column to be modified.
- Display unassigned bindings as `None`.

The displayed key names must correspond to the canonical names defined in Phase 2.

## 8. Editing Keyboard Bindings

Support two methods of changing a binding.

### Method A — Direct Key Capture

This should be the primary method.

Workflow:

1. Select a binding row.
2. Double-click the row or press Enter.
3. Open a small modal key assignment dialog.
4. Display a prompt such as:

   `Press a key to assign...`

5. Capture the user's physical keyboard input.
6. Convert the captured key into a supported canonical key identifier.
7. Display the proposed assignment.
8. Confirm or cancel the change.

Example:

```text
+----------------------------------+
| Assign Keyboard Key              |
+----------------------------------+
|                                  |
| Press a key to assign...         |
|                                  |
| Selected: Space                  |
|                                  |
| [Choose Key...]                  |
|                                  |
|             [Cancel] [Assign]    |
+----------------------------------+
```

Do not apply the binding immediately to the running input engine.

Only update the editable configuration.

### Supported Key Types

Support every key name implemented in Phase 2.

At minimum:

- A–Z
- 0–9
- F1–F12
- Space
- Enter
- Escape
- Tab
- Backspace
- Arrow keys
- Insert
- Delete
- Home
- End
- PageUp
- PageDown
- Left and right Shift
- Left and right Control
- Left and right Alt

The capture mechanism must distinguish left and right modifiers where supported.

Use Windows key messages and scan-code information as appropriate.

Do not rely only on `WM_CHAR`.

Do not perform locale-sensitive character translation to infer arbitrary keyboard bindings.

### Key Capture Semantics

Key capture must use a focused dialog or control.

Do not install a global keyboard hook.

Do not capture keyboard input outside ZeroPad.

Use appropriate messages such as:

- `WM_KEYDOWN`
- `WM_SYSKEYDOWN`

Handle scan-code and extended-key information when necessary.

Suppress normal dialog shortcuts only while the capture control is explicitly active.

Clearly document how special keys, including Escape, Enter, Tab, and modifier keys, can be assigned.

### Escape Handling

Choose a predictable policy.

Recommended:

- A dedicated Cancel button always cancels.
- Escape cancels capture while no key has been selected.
- Escape can be assigned using the alternative key selection method.

Do not make important keys impossible to configure.

### Modifier Keys

Support standalone modifier keys.

For example:

- LShift
- RShift
- LCtrl
- RCtrl
- LAlt
- RAlt

Do not interpret a modifier followed by another key as a chord.

Key combinations remain out of scope.

### Unsupported Keys

If the user presses an unsupported key:

- Display a concise explanation.
- Do not silently assign an unrelated key.
- Keep the dialog active.
- Allow the user to retry or cancel.

Do not invent new key names that the Phase 2 configuration system cannot serialize.

## 9. Preventing Input Capture Conflicts

This requirement is critical.

ZeroPad is itself generating keyboard input using SendInput.

The key capture dialog must not accidentally record keyboard events injected by ZeroPad.

### Recommended Behavior

When key capture begins:

1. Request a temporary suspension of ZeroPad's keyboard mapping.
2. Release keyboard keys currently held by ZeroPad using the existing Phase 2 safe reconciliation mechanism.
3. Confirm that the application is in a safe suspended state.
4. Begin accepting physical keyboard capture.
5. Ignore injected keyboard events where they can be identified reliably.
6. Complete or cancel the capture.
7. Resume mapping only after the capture dialog has closed and physical capture keys have been released.

If outstanding injected key releases cannot be reconciled, do not begin capture.

Display an appropriate error instead.

### Important Restrictions

- Do not use a global low-level keyboard hook as the default solution.
- Do not introduce a driver.
- Do not use Raw Input unless a genuine requirement is demonstrated and documented.
- Do not allow capture mode to produce its own keyboard remapping events.
- Do not automatically save the configuration when capture completes.

If the existing Phase 2 engine cannot be safely suspended, make the smallest necessary change to support it.

Do not duplicate the engine's key-release logic inside the GUI.

### Capture Cleanup

Use an explicit capture-state mechanism.

Restore normal engine operation after:

- Successful assignment.
- Cancel.
- Dialog closure.
- Unsupported-key handling followed by cancel.
- Unexpected dialog destruction.

Do not resume with stale controller or hysteresis state.

Do not resume mapping while the physical key used for capture is still pressed.

Use a small, well-defined resume policy rather than indefinite waiting.

## 10. Alternative Key Selection

Provide a simple alternative method for users who cannot conveniently press the desired key during capture.

Use a standard ComboBox or small selection dialog.

Display all keys supported by the Phase 2 key-name table.

Example:

```text
Select Keyboard Key

[ Space                     v ]

[Cancel] [Assign]
```

Use the same canonical key identifiers as direct capture.

Do not maintain a second independent key-name database.

This method must support keys that are awkward to capture directly, including:

- Escape
- Tab
- Enter
- Modifier keys

No graphical keyboard is required.

## 11. Clearing a Binding

Provide a `Clear Binding` button.

Behavior:

1. User selects an input row.
2. User clicks Clear Binding.
3. The editable binding becomes `None`.
4. The displayed row updates.
5. The running input engine remains unchanged until Save.

Do not require a confirmation dialog for every cleared binding.

Cancel must still restore the previous saved configuration.

## 12. General Settings

Provide a checkbox:

`Enable keyboard mapping`

It controls the existing Phase 2 `enabled` setting.

Changing this checkbox must not immediately enable or disable the live engine.

It must follow Save/Cancel semantics.

Do not add Windows startup configuration.

That belongs to Phase 4 or later.

## 13. Analog Settings

Expose the existing Phase 2 analog thresholds.

Required fields:

- Stick press threshold
- Stick release threshold
- Trigger press threshold
- Trigger release threshold

Use standard numeric edit controls.

Suggested ranges:

| Setting | Range |
|---|---|
| Stick press | 0.0–1.0 |
| Stick release | 0.0–1.0 |
| Trigger press | 0–255 |
| Trigger release | 0–255 |

The existing Phase 2 validation rules remain authoritative:

- Stick release must be less than stick press.
- Trigger release must be less than trigger press.
- All numbers must be finite and within the supported range.

Do not duplicate complicated numeric validation logic in the GUI.

Use the existing configuration validator after converting input text into typed values.

### Input Errors

If validation fails:

- Do not save.
- Do not apply settings.
- Display a concise error.
- Keep the settings window open.
- Preserve the user's unsaved input.
- Focus the relevant field where practical.

Do not silently clamp invalid values.

## 14. Polling Interval

Expose the existing `polling_interval_ms` setting.

Use a numeric edit control.

Allowed range:

`1–100 ms`

Default:

`4 ms`

Display the unit clearly.

Do not add a performance tuning panel.

Do not introduce advanced scheduling options.

## 15. Configuration Editing Model

Maintain a strict distinction between:

- Persisted configuration.
- Active engine configuration.
- Editable GUI configuration.

### Editable Configuration

When the settings window opens:

1. Obtain a snapshot of the current active configuration.
2. Create a separate editable copy.
3. Populate all controls from the editable copy.
4. Allow the user to make changes without affecting the active engine.

Do not bind UI controls directly to mutable engine state.

Do not retain references to parser-owned TOML structures.

Use the typed configuration model created in Phase 2.

### Cancel

When the user presses Cancel:

- Discard unsaved GUI changes.
- Do not write the configuration file.
- Do not alter the running configuration.
- Close the settings window.

If mapping was temporarily suspended for key capture, ensure it has already been resumed safely.

### Window Close

Treat the standard window close button as Cancel.

Do not silently save changes on close.

Do not terminate the entire application unexpectedly when closing a child editing dialog.

### Defaults

The Defaults button must:

1. Load the centralized Phase 2 default configuration values into the editable copy.
2. Update all visible controls.
3. Mark the settings as modified.
4. Leave the active configuration unchanged.
5. Avoid writing any files until Save.

Do not duplicate hardcoded default values inside the GUI.

A confirmation dialog is optional and not necessary if Cancel remains available.

## 16. Save Behavior

Save must use the Phase 2 validation, persistence, and safe application mechanisms.

### Required Sequence

1. Read all editable control values.
2. Construct a candidate configuration.
3. Validate the complete candidate.
4. If validation fails, report the error and stop.
5. Compare the candidate with the current active configuration.
6. If necessary, request a safe configuration transition.
7. Persist the candidate using the existing Phase 2 safe-save mechanism.
8. Ensure the active engine and persisted configuration have a clearly defined, consistent result.
9. Report success only when the requested save and activation have actually succeeded.

Do not directly mutate individual engine settings from Win32 event handlers.

### Failure Handling

The implementation must explicitly handle:

- Validation failure.
- Keyboard release failure.
- Configuration application failure.
- Configuration file write failure.
- Configuration replacement failure.
- Concurrent configuration changes.

Avoid situations where the UI reports a successful Save while only part of the operation completed.

### Transaction Semantics

The preferred implementation should provide a small application-level operation for saving and applying configuration.

Use the existing Phase 2 primitives rather than reimplementing them.

It is acceptable to introduce a small coordination layer to manage the transition.

Define and document the ordering of persistence and application.

If a transition fails after persistence, the application must report that the configuration was saved but could not be activated, retain a safe engine state, and provide an explicit recovery path.

If persistence fails after an in-memory change, either safely restore the previous configuration or retain the new state with a clear unsaved status.

Never silently claim both operations succeeded.

Do not implement a generic transaction framework.

### No-Change Save

If the editable configuration is semantically identical to the active and persisted configuration:

- Do not unnecessarily rewrite the TOML file.
- Do not generate keyboard release/repress events.
- Close the settings window normally.

## 17. External Configuration Changes

Phase 2 supports reloading TOML configuration.

The GUI must not silently overwrite external changes made while the settings window is open.

Use a lightweight change-detection strategy.

For example:

- Maintain a configuration revision counter.
- Record the revision when editing begins.
- Detect whether the active configuration changed before Save.

If the configuration changed externally:

- Do not blindly overwrite it.
- Inform the user.
- Offer to reload the current configuration and discard unsaved changes, or cancel the Save operation.

Do not implement complex three-way merging.

A revision counter or equivalent simple mechanism is sufficient.

## 18. Integration with the Running Input Engine

The existing input engine must continue functioning while the settings window is open, except during explicit temporary suspension or safe configuration application.

### Responsiveness

The GUI must remain responsive.

Do not run the 4 ms XInput polling loop inside the Win32 message loop.

Prefer reusing the existing polling loop in a dedicated worker thread, with the GUI on the main thread.

However:

- Inspect the Phase 2 architecture before changing the threading model.
- Reuse an existing worker thread if present.
- Do not create multiple polling workers.
- Do not introduce thread pools or asynchronous frameworks.
- Avoid holding locks during file I/O or window message dispatch.
- Avoid blocking the GUI on indefinite worker-thread operations.

A single engine worker thread is sufficient if separate execution is necessary.

### Thread Safety

Cross-thread communication must be explicit.

Use a small command or synchronization mechanism for:

- Configuration application.
- Temporary mapping suspension.
- Mapping resumption.
- Engine status queries.
- Shutdown.

Do not read or write mutable engine configuration concurrently without synchronization.

Do not call UI controls directly from a worker thread.

Use normal Win32 message passing or a small synchronized snapshot mechanism to update the GUI.

Keep synchronization simple and auditable.

## 19. GUI Lifecycle

Phase 3 introduces the settings window but not the system tray.

Use a straightforward application lifecycle.

Recommended behavior:

- Launch ZeroPad.
- Start the input engine.
- Open the settings window.
- Closing the primary settings window ends the application gracefully.
- Release held keys before exit.

Do not create a hidden background application with no way for the user to reopen the settings window.

Background tray operation belongs to Phase 4.

If console commands remain from Phase 2, preserve them only where practical and avoid conflicting shutdown paths.

It is acceptable for the normal Phase 3 build to use the Windows GUI subsystem, provided existing automated tests and developer diagnostics remain accessible.

Do not maintain two large independent application implementations.

## 20. DPI Support

Use modern Windows DPI awareness.

Prefer:

`PerMonitorV2`

Declare DPI awareness in the application manifest where appropriate.

Support:

- 100% scaling.
- 125% scaling.
- 150% scaling.
- 200% scaling.

Use DPI-aware font and control sizing.

Handle `WM_DPICHANGED` where required.

Avoid fixed pixel assumptions that cause clipped labels or buttons.

Use normal system fonts.

Do not bundle custom fonts.

Do not build a general-purpose DPI layout framework.

## 21. Keyboard Navigation

The GUI must be usable without a mouse.

Required behavior:

- Tab moves through interactive controls.
- Shift+Tab moves backward.
- Arrow keys navigate the binding list.
- Enter edits the selected binding.
- Space activates focused buttons or checkboxes using normal Windows behavior.
- Escape cancels editing dialogs according to the defined capture policy.
- Cancel discards unsaved changes.
- Save is accessible from the keyboard.

Use proper control tab order.

Provide visible focus indicators through standard Windows controls.

Do not invent custom keyboard navigation conventions.

## 22. Basic Accessibility

Rely on standard Windows controls wherever possible.

Requirements:

- Associate labels with their corresponding controls.
- Use meaningful control text.
- Provide logical keyboard navigation.
- Keep standard focus indicators.
- Do not encode information only through color.
- Ensure controller status can be understood from text.
- Avoid relying on icons as the only explanation of functionality.

Do not implement a custom accessibility framework.

Do not claim comprehensive screen-reader certification without testing.

## 23. Resource Usage

ZeroPad must remain lightweight.

Requirements:

- No embedded browser.
- No custom rendering framework.
- No continuous animation.
- No high-frequency GUI redraws.
- No per-frame ListView reconstruction.
- No unnecessary background threads.
- No repeated TOML parsing during idle operation.

Controller status updates should occur at a modest GUI refresh interval.

The input engine's performance must not depend on whether the settings window is visible.

Measure and report memory usage when practical.

Do not sacrifice functional correctness to meet arbitrary binary size targets.

## 24. Suggested Source Organization

Preserve the existing repository structure.

Add only the files necessary for the settings UI.

Example:

```text
ZeroPad/
├── CMakeLists.txt
├── include/
│   └── zeropad/
│       ├── settings_window.hpp
│       ├── binding_dialog.hpp
│       └── app_controller.hpp
├── src/
│   ├── settings_window.cpp
│   ├── binding_dialog.cpp
│   ├── app_controller.cpp
│   └── main.cpp
├── resources/
│   ├── zeropad.rc
│   └── zeropad.manifest
└── tests/
    └── settings_tests.cpp
```

File names are suggestions.

Do not create a large GUI framework.

Keep UI message handling understandable.

Use resource-based dialog templates where they simplify layout and maintenance.

Keep input engine logic outside the GUI code.

## 25. Automated Tests

Extend the existing test suite.

Most tests should operate without displaying a real window.

### 25.1 Configuration Editing Tests

Verify:

1. Opening an editor creates a configuration copy.
2. Editing does not mutate the active configuration.
3. Cancel discards edits.
4. Defaults loads centralized default settings.
5. Save validates the complete candidate.
6. Invalid Save leaves the active configuration unchanged.
7. Unchanged Save avoids unnecessary engine transitions.
8. External configuration revision changes are detected.

### 25.2 Binding Editing Tests

Verify:

1. Every supported logical input appears in the mapping model.
2. Every canonical key name can be selected.
3. Unsupported keys are rejected.
4. Clear Binding produces `None`.
5. Duplicate output keys remain valid.
6. A captured key maps to the correct canonical identifier.
7. Left and right modifiers are distinguished.
8. Canceling key capture leaves the binding unchanged.
9. Unsupported physical keys do not produce an invalid assignment.

### 25.3 Capture Safety Tests

Using a mocked input engine, verify:

1. Key capture requests temporary mapping suspension.
2. Held keys are released before capture begins.
3. Failed suspension prevents capture.
4. Capture cancellation resumes mapping safely.
5. Successful assignment resumes mapping safely.
6. Injected mapping events are not accepted as physical capture input where distinguishable.
7. Dialog destruction does not leave mapping permanently suspended.
8. Normal controller processing resumes after capture.
9. Physical capture keys do not accidentally become lingering engine inputs.

### 25.4 Save and Apply Tests

Verify:

1. Successful validation, persistence, and application.
2. Validation failure.
3. Persistence failure.
4. Engine transition failure.
5. Failed key release during application.
6. Retry or recovery behavior.
7. No-change Save.
8. Duplicate-key mappings after Save.
9. Controller index change.
10. Analog threshold change.
11. Disabling the engine.
12. Re-enabling the engine.
13. Detection of conflicting external configuration changes.

### 25.5 Threading Tests

Where applicable, verify:

1. Configuration changes are safely transferred to the worker thread.
2. UI status reads use safe snapshots.
3. Shutdown does not deadlock.
4. The engine stops before its dependent resources are destroyed.
5. Suspension and resumption work during normal polling.
6. No concurrent mutable configuration access occurs without synchronization.

Do not introduce timing-dependent tests that fail unpredictably on slower machines.

### 25.6 Regression Tests

All Phase 0, Phase 1, and Phase 2 tests must continue to pass.

Do not weaken existing assertions.

Use CTest for automated execution.

## 26. Manual GUI Verification

Perform manual verification on Windows 11 when the environment permits.

### Basic UI

- Window opens correctly.
- All supported inputs appear.
- Columns are readable.
- Resizing behaves sensibly.
- No controls overlap.
- Keyboard navigation works.
- Default values load correctly.

### Binding Editing

- Double-click opens key capture.
- Enter opens key capture.
- Physical key assignment works.
- Modifier assignment works.
- Alternative key selection works.
- Clear Binding works.
- Cancel does not modify the binding.
- Unsupported keys are handled properly.

### Configuration

- Save persists changes.
- Restart loads saved configuration.
- Cancel discards changes.
- Defaults updates editable values.
- Invalid numeric values are rejected.
- Invalid TOML remains protected.
- External configuration changes are not silently overwritten.

### Input Engine

- Mapping remains operational while the settings window is open.
- Key capture does not accidentally record ZeroPad-generated keys.
- Held keys are released safely during configuration changes.
- Connection status updates correctly.
- Controller index changes work.
- Disabling mapping releases keys.
- Re-enabling mapping works.
- Closing ZeroPad releases held keys.

### DPI

Verify at least:

- 100%
- 150%
- 200%

Check for clipping, overlapping controls, and unreadable text.

If manual verification cannot be performed, explicitly report the unverified cases.

## 27. Build Requirements

Update the existing CMake configuration.

Requirements:

- Preserve C++20.
- Preserve MSVC support.
- Include Win32 GUI resources.
- Link required Windows libraries.
- Initialize Common Controls properly.
- Include the application manifest.
- Preserve CTest support.
- Keep Debug and Release configurations working.

Support:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Do not introduce a package manager.

Do not download UI dependencies during configuration.

## 28. Documentation

Update README with:

- Description of the settings UI.
- How to select a controller.
- How to change a keyboard binding.
- How to clear a binding.
- How to use direct key capture.
- How to use alternative key selection.
- How to configure analog thresholds.
- How Save, Cancel, and Defaults behave.
- Where configuration files are stored.
- How controller connection status is displayed.
- Known SendInput limitations.
- How to exit the application.
- Known Phase 3 limitations.

Keep documentation concise and user-oriented.

Do not document unimplemented Phase 4 features as available.

## 29. Explicitly Out of Scope

Do not implement:

- System tray.
- Background tray-only operation.
- Windows startup integration.
- Multiple profiles.
- Graphical gamepad diagrams.
- Controller illustrations.
- Live graphical button indicators.
- Custom themes.
- Dark-mode framework.
- Custom rendering.
- Direct2D UI.
- GameInput.
- DirectInput.
- Raw HID controller support.
- Mouse emulation.
- Macros.
- Key combinations.
- Rapid-fire functionality.
- Automatic updates.
- Cloud synchronization.
- Plugin architecture.
- Installer.

Do not create placeholder implementations for future features.

## 30. Implementation Workflow

Follow this sequence.

### Step 1 — Inspect the Existing Repository

Examine the Phase 2 implementation.

Identify:

- Input engine lifecycle.
- Controller polling.
- Configuration structures.
- TOML loading and saving.
- Safe configuration application.
- Runtime reload mechanism.
- Error handling.
- Existing tests.

Preserve useful existing code.

### Step 2 — Design the Minimal UI Boundary

Define how the GUI reads and edits configuration.

Introduce only the necessary application coordination interface.

Keep Win32 control handling separate from engine state management.

### Step 3 — Create the Settings Window

Implement the native primary window.

Add:

- Controller selection.
- Connection status.
- Enable checkbox.
- Binding ListView.
- Analog fields.
- Polling interval.
- Save, Cancel, and Defaults.

Verify layout and keyboard navigation.

### Step 4 — Implement the Binding List

Populate the ListView from the existing logical input model.

Display current mappings using the Phase 2 canonical key-name table.

### Step 5 — Implement Key Assignment

Create the small key capture dialog.

Add alternative key selection.

Implement Clear Binding.

Test key-name conversion independently.

### Step 6 — Implement Safe Capture Suspension

Integrate temporary mapping suspension.

Ensure ZeroPad does not capture its own injected keyboard events.

Test cancellation and cleanup paths.

### Step 7 — Implement Editable Configuration

Use an isolated editable configuration copy.

Implement Cancel and Defaults.

Ensure unsaved edits never affect active input mapping.

### Step 8 — Implement Save and Apply

Reuse Phase 2 validation, persistence, and safe transition operations.

Handle errors without misleading success messages.

Detect external configuration changes.

### Step 9 — Integrate the Engine Lifecycle

Keep input processing operational while the GUI is active.

Implement safe thread coordination if required.

Ensure application shutdown releases held keys.

### Step 10 — Add DPI and Keyboard Navigation

Handle DPI changes.

Verify keyboard focus and tab order.

Fix clipped or overlapping controls.

### Step 11 — Expand Tests

Add configuration editing, capture, save/apply, and lifecycle tests.

Run all previous regression tests.

### Step 12 — Build, Verify, and Document

Build Release.

Run CTest.

Perform manual verification where possible.

Update README.

Report remaining limitations.

## 31. Definition of Done

Phase 3 is complete only when:

- [ ] Native Win32 settings window works.
- [ ] No third-party UI framework is introduced.
- [ ] All logical controller inputs are listed.
- [ ] Current keyboard bindings are displayed correctly.
- [ ] Controller selection is available.
- [ ] Controller connection status is displayed.
- [ ] Direct keyboard capture works.
- [ ] Alternative key selection works.
- [ ] Bindings can be cleared.
- [ ] Supported modifier keys can be assigned.
- [ ] Input capture does not accidentally record ZeroPad-generated keys.
- [ ] Mapping suspension and resumption are safe.
- [ ] Analog thresholds can be edited.
- [ ] Polling interval can be edited.
- [ ] Keyboard mapping can be enabled and disabled.
- [ ] Save validates and persists configuration correctly.
- [ ] Cancel discards unsaved changes.
- [ ] Defaults restores default editable values.
- [ ] Invalid values are reported clearly.
- [ ] External configuration changes are handled safely.
- [ ] Configuration changes do not leave incorrectly held keys.
- [ ] Input engine remains operational while the UI is open.
- [ ] GUI remains responsive.
- [ ] Application closes cleanly.
- [ ] DPI scaling works on Windows 11.
- [ ] Keyboard navigation is functional.
- [ ] Phase 0 tests pass.
- [ ] Phase 1 tests pass.
- [ ] Phase 2 tests pass.
- [ ] Phase 3 tests pass.
- [ ] Release build succeeds.
- [ ] README is updated.
- [ ] No Phase 4 functionality has been implemented.

## 32. Final Deliverables

Provide:

1. Complete Win32 settings UI implementation.
2. Binding editor and key capture dialog.
3. Alternative key selector.
4. Safe mapping suspension integration.
5. Configuration editing and Save/Cancel support.
6. Updated application lifecycle handling.
7. Updated CMake build configuration.
8. Windows resource files and manifest.
9. Automated tests.
10. Updated README.
11. Build and test results.
12. Manual verification results where available.
13. Short summary of architectural changes.
14. Known limitations and unresolved issues.

Do not replace implementation with pseudocode.

Do not claim unexecuted tests passed.

Do not automatically implement Phase 4.

## 33. Final Engineering Principles

**Clarity over appearance.**

Use ordinary Windows controls and obvious interactions.

**The GUI is a configuration editor, not an input engine.**

Do not duplicate controller processing or keyboard mapping logic.

**Unsaved changes must not affect active mappings.**

Maintain an isolated editable configuration.

**Keyboard safety is mandatory.**

Never sacrifice correct key-release behavior for UI responsiveness or convenience.

**Do not capture ZeroPad's own injected keyboard input.**

Temporarily suspend mapping during direct key capture.

**Keep resource usage minimal.**

Avoid unnecessary threads, redraws, allocations, and dependencies.

**Preserve earlier phases.**

All previously implemented functionality and tests must remain valid.

**Stop at Phase 3.**

Finish the native configuration UI, verify it, and stop.

System tray integration and background application behavior belong to Phase 4.
