# ZeroPad — Phase 2 Implementation Instructions

## 1. Project Overview

ZeroPad is an open-source, ultra-lightweight Windows utility written in modern C++.

It converts XInput-compatible game controller inputs into keyboard inputs using native Windows APIs.

The project prioritizes:

1. Correct and reliable keyboard state management.
2. Minimal CPU and memory overhead.
3. Simple, maintainable C++20 code.
4. Native Windows integration.
5. Minimal external dependencies.
6. Straightforward configuration for nontechnical users.
7. Predictable behavior when configuration files are missing, invalid, or corrupted.

Phase 0 implemented basic A-to-Space conversion.

Phase 1 extended the engine to support digital buttons, triggers, analog stick directions, simultaneous inputs, and safe keyboard state management.

**Phase 2 must introduce persistent TOML configuration without compromising the reliability or simplicity of the existing input engine.**

Do not rewrite the input engine unless necessary to integrate configuration safely.

Do not implement a GUI in this phase.

## 2. Phase 2 Objectives

Implement:

1. Integration of the project's designated cpptoml library.
2. A versioned TOML configuration schema.
3. Strongly typed in-memory configuration.
4. TOML configuration loading and parsing.
5. Schema validation and meaningful error reporting.
6. Default configuration generation.
7. Saving configuration using a minimal TOML serializer.
8. Safe replacement of existing configuration files.
9. Applying mappings and analog thresholds to the input engine.
10. Runtime configuration reload through a simple console command.
11. Safe keyboard state reconciliation when settings change.
12. Automated tests for parsing, validation, serialization, reload, and failure recovery.

The implementation must remain lightweight and self-contained.

## 3. Technical Constraints

Target:

- Windows 11 x64
- C++20
- MSVC
- CMake
- XInput
- SendInput
- Native Windows filesystem APIs where appropriate

Do not introduce:

- Qt
- SDL
- Boost
- toml++
- Other TOML parsers
- JSON libraries
- Configuration frameworks
- External runtime services
- Background file-watching services
- Unnecessary dependencies

Preserve the existing Phase 0 and Phase 1 functionality.

Keep the input polling hot path free from file I/O, TOML parsing, serialization, and avoidable dynamic allocations.

## 4. cpptoml Integration

Use this specific repository:

https://github.com/taqu/cpptoml

**IMPORTANT: This is not the similarly named skystrife/cpptoml library.**

Do not substitute another TOML library.

The intended repository provides a lightweight TOML reader using files named:

- `cpptoml.h`
- `cpptoml.cpp`

Its documented interfaces include:

- `cpptoml::TomlParser`
- `cpptoml::TomlProxy`
- `cpptoml::TomlType`

The README demonstrates parsing an in-memory buffer using `TomlParser::parse(begin, end)` and traversing the resulting `TomlProxy` tree.

### Requirements

1. Inspect the actual library source and README before integrating it.
2. Confirm its current public API rather than assuming compatibility with other cpptoml implementations.
3. Include the library through a reproducible dependency mechanism, preferably a pinned Git submodule or an existing repository-approved approach.
4. Integrate it with the existing CMake configuration.
5. Do not require a package manager.
6. Avoid modifying upstream cpptoml source code.
7. Respect the library's license and attribution requirements.

The library is primarily a TOML reader.

Do not assume it provides a general-purpose writer.

Implement a small ZeroPad-specific serializer if no suitable writer exists.

### Parser Safety

The upstream README identifies limitations involving deeply nested structures and invalid Unicode code points.

Since configuration files are untrusted external input, do not assume parsing arbitrary files is safe.

Implement reasonable input restrictions, including:

- A maximum configuration file size of 64 KiB.
- Rejection of embedded NUL bytes.
- UTF-8 validation where needed.
- Rejection of malformed UTF-8.
- A documented, conservative limit on nesting depth.
- Controlled error handling for parse failures.

Do not attempt to build a full general-purpose TOML security framework.

If a parser limitation cannot be safely mitigated using a simple check, document the remaining limitation rather than claiming complete protection.

## 5. Configuration File Location

Use the following default location:

`%LOCALAPPDATA%\ZeroPad\config.toml`

Resolve the directory using the Windows Known Folder API.

Prefer `SHGetKnownFolderPath` with `FOLDERID_LocalAppData`.

Do not construct the location by blindly concatenating environment variables.

Create the ZeroPad directory when necessary.

### Initial Startup

If the configuration file does not exist:

1. Construct the default configuration in memory.
2. Attempt to create the configuration directory.
3. Attempt to save the default configuration.
4. Apply the defaults to the input engine.
5. Continue running even if saving the defaults fails, provided the application can otherwise operate safely.
6. Report the persistence failure without repeatedly logging it.

### Existing Configuration

If the configuration file exists:

1. Read it.
2. Parse it using cpptoml.
3. Validate its contents.
4. Construct a typed configuration.
5. Apply it only after successful validation.

### Portable Mode

Portable mode is not required for Phase 2.

Do not implement automatic configuration discovery beside the executable.

Do not introduce multiple configuration search paths.

A single explicit configuration location is preferable for now.

## 6. Configuration Schema

Use a small, explicit, versioned schema.

Suggested initial configuration:

```toml
version = 1

[general]
enabled = true
polling_interval_ms = 4

[controller]
index = 0

[analog]
stick_press_threshold = 0.25
stick_release_threshold = 0.20
trigger_press_threshold = 40
trigger_release_threshold = 25

[bindings]
A = "Space"
B = "Escape"
X = "E"
Y = "R"

DPAD_UP = "Up"
DPAD_DOWN = "Down"
DPAD_LEFT = "Left"
DPAD_RIGHT = "Right"

LB = "Q"
RB = "F"

START = "Enter"
BACK = "Tab"

LS_BUTTON = "LShift"
RS_BUTTON = "LCtrl"

LT = "Z"
RT = "X"

LS_UP = "W"
LS_DOWN = "S"
LS_LEFT = "A"
LS_RIGHT = "D"

RS_UP = "I"
RS_DOWN = "K"
RS_LEFT = "J"
RS_RIGHT = "L"
```

This schema must cover all logical inputs implemented in Phase 1.

Do not add settings for functionality that does not exist.

In particular, do not add mouse settings, macros, profiles, or Windows startup settings.

### Schema Version

Use:

`version = 1`

The version field is mandatory.

Reject unsupported versions explicitly.

Do not silently reinterpret an unknown schema version.

Do not implement a general configuration migration framework.

### Missing Fields

Use these rules:

- Missing configuration file: use defaults and attempt to create it.
- Missing optional section or field: use its documented default.
- Missing version field in an existing file: configuration error.
- Wrong value type: configuration error.
- Unknown field or section: configuration error.
- Duplicate fields: rely on TOML validity and explicitly reject duplicates if the parser does not enforce this.

Do not silently accept misspelled field names.

For Phase 2, unknown fields should be rejected rather than silently discarded during subsequent saves.

### Invalid Configuration

When an existing configuration is invalid:

- Report the reason and relevant field when possible.
- Preserve the original file.
- Do not overwrite it with defaults.
- Keep the application functional using safe in-memory defaults on initial startup.
- During runtime reload, retain the previously active configuration.
- Never apply partially validated settings.

## 7. Typed Configuration Model

Create a small C++ configuration representation.

Suggested structure:

```cpp
struct GeneralConfig {
    bool enabled = true;
    int polling_interval_ms = 4;
};

struct ControllerConfig {
    int index = 0;
};

struct AnalogConfig {
    double stick_press_threshold = 0.25;
    double stick_release_threshold = 0.20;

    int trigger_press_threshold = 40;
    int trigger_release_threshold = 25;
};

struct Config {
    static constexpr int schema_version = 1;

    GeneralConfig general;
    ControllerConfig controller;
    AnalogConfig analog;

    // Fixed-size key binding table.
};
```

This is an illustrative structure, not a mandatory API.

Adapt it to the actual Phase 1 architecture.

Requirements:

- Keep configuration independent from cpptoml's internal data structures.
- Avoid retaining parser-owned pointers or string views after parser buffers are released.
- Use appropriate value types and fixed-size containers.
- Centralize default values.
- Ensure one authoritative default mapping table.
- Keep validation separate from the input polling loop.

The input engine must not access TOML parser objects directly.

## 8. Keyboard Binding Representation

Use a small, explicit binding syntax.

A binding maps one controller input to exactly one keyboard key.

Examples:

```toml
A = "Space"
B = "Escape"
LS_UP = "W"
RT = "X"
```

### Supported Key Names

Support at least:

- A–Z
- 0–9
- F1–F12
- Space
- Enter
- Escape
- Tab
- Backspace
- Up
- Down
- Left
- Right
- Insert
- Delete
- Home
- End
- PageUp
- PageDown
- LShift
- RShift
- LCtrl
- RCtrl
- LAlt
- RAlt

Support:

`None`

to represent an unassigned controller input.

For example:

```toml
RS_BUTTON = "None"
```

### Requirements

- Use canonical key names.
- Define deterministic parsing and serialization.
- Reject unknown key names.
- Avoid confusing numeric key names with virtual-key numeric values.
- Keep the key-name conversion table centralized.
- Do not use locale-dependent character conversion.
- Do not interpret arbitrary TOML strings as Windows virtual-key codes.

Case-sensitive canonical names are acceptable.

Document the chosen behavior.

### Key Injection Semantics

Use the existing Phase 1 keyboard output mechanism unless a correction is necessary.

Do not assume every Windows virtual key has identical scan-code behavior.

Clearly document any limitations involving extended keys or application-specific keyboard handling.

Do not introduce a second keyboard injection architecture merely for configuration.

### Key Combinations

Do not support:

- Ctrl+C
- Alt+Tab
- Shift+F1
- Multi-key sequences
- Macros

These belong to later development phases.

## 9. Configuration Validation

Validate all configuration fields before applying them.

### General Settings

`enabled`

- Boolean.
- Default: true.

`polling_interval_ms`

- Integer.
- Default: 4.
- Allowed range: 1–100.

Do not introduce high-resolution timer changes.

### Controller Settings

`index`

- Integer.
- Default: 0.
- Allowed range: 0–3.

The Phase 1 engine supports one controller at a time.

This setting selects the active controller index.

It does not enable simultaneous multi-controller input.

### Analog Settings

Stick thresholds:

- Finite numeric values.
- No NaN or infinity.
- `0.0 <= release < press <= 1.0`.

Default values:

- Press: 0.25.
- Release: 0.20.

Trigger thresholds:

- Integers.
- `0 <= release < press <= 255`.

Default values:

- Press: 40.
- Release: 25.

Ensure the values are compatible with the normalization and threshold semantics already implemented in Phase 1.

### Bindings

Validate:

- Every input identifier.
- Every key name.
- Correct TOML value types.
- Explicit unassigned values.
- Duplicate configuration entries.
- Unsupported names.

Multiple controller inputs must be allowed to map to the same keyboard key.

This is intentional.

Do not reject duplicate output keys.

### Validation Strategy

Prefer all-or-nothing validation.

Do not apply settings incrementally while parsing.

Only a fully validated configuration may replace the active configuration.

## 10. TOML Serialization

Implement a minimal serializer dedicated to ZeroPad's fixed configuration schema.

Do not create a general-purpose TOML writer.

### Requirements

- Produce valid TOML.
- Emit all supported configuration fields.
- Use deterministic field ordering.
- Use consistent whitespace and line endings.
- Serialize booleans and numbers correctly.
- Serialize supported key names using canonical representations.
- Produce UTF-8 text.
- Avoid unnecessary formatting complexity.

The serializer only needs to handle known ZeroPad configuration types.

It must not need to serialize arbitrary nested TOML documents.

### Round-Trip Guarantee

For a valid configuration:

`parse(serialize(config)) == config`

The comparison must be semantic.

Whitespace and comments do not need to be preserved.

### Comments

Preserving user comments is not required in Phase 2.

The serializer may produce a canonical configuration file.

Document that a save operation may normalize formatting and remove comments.

Do not rewrite a configuration file unnecessarily during ordinary startup or reload.

## 11. Safe Configuration Saving

Configuration saving must not unnecessarily risk destroying a previously valid file.

Use a temporary file in the same directory as the target configuration.

Suggested procedure:

1. Validate the configuration object.
2. Serialize it in memory.
3. Create a temporary file in the destination directory.
4. Write the complete serialized contents.
5. Verify successful writing.
6. Close the file handles.
7. Replace the existing configuration file using an appropriate Windows file replacement operation.
8. Clean up temporary files after failures where safe.

Use Windows APIs such as:

- `ReplaceFileW`
- `MoveFileExW`

Choose the appropriate operation depending on whether the destination already exists.

Do not delete the original file before a valid replacement is ready.

Do not fall back to an unsafe delete-then-write operation.

### Save Failure

If saving fails:

- Return a meaningful error.
- Preserve the previous valid configuration when possible.
- Do not claim the new configuration was saved.
- Do not leave the in-memory active configuration in an undocumented partial state.

### Crash Safety

Use same-directory replacement to reduce partial-write risks.

Do not claim guaranteed durability against power loss or every filesystem failure.

Keep failure-handling logic simple and testable.

## 12. Runtime Configuration Reload

Implement a minimal explicit reload mechanism.

Since Phase 2 has no GUI, use a console command.

Suggested command:

`r`

to reload the configuration.

Also provide:

`q`

to quit gracefully.

### Requirements

- Do not introduce a separate management service.
- Avoid unnecessary background threads.
- If practical, process console input without blocking the controller polling loop.
- Keep the command handling minimal.
- Do not introduce a generic command interpreter.

Windows console input APIs or a simple nonblocking console-input technique are acceptable.

### Reload Procedure

When a reload is requested:

1. Read the configuration file into a temporary buffer.
2. Parse using cpptoml.
3. Validate the complete configuration.
4. Construct a complete candidate configuration.
5. Compare it with the active configuration.
6. Apply the candidate using a safe transition.
7. Report success or failure.

Do not replace the active configuration if parsing or validation fails.

### Unchanged Configuration

If the new configuration is semantically identical to the active configuration:

- Do not generate keyboard transitions.
- Do not reset analog state unnecessarily.
- Do not interrupt normal input processing.

## 13. Safe Configuration Application

This is a critical requirement.

Configuration changes must not leave keyboard keys incorrectly pressed.

### Example

Current mapping:

```toml
A = "Space"
```

New mapping:

```toml
A = "Enter"
```

If the controller's A button is currently held when the configuration is reloaded, the transition must be safe.

### Recommended Strategy

Use a release-first configuration transition.

1. Stop issuing new controller-driven key-down events.
2. Set all keyboard desired states to released.
3. Attempt to release all keys held by ZeroPad.
4. Reconcile outstanding key-up operations.
5. Reset logical input and analog hysteresis state.
6. Install the new validated configuration.
7. Process a fresh controller snapshot under the new settings.
8. Resume normal operation.

This conservative strategy is preferred over trying to preserve every held key across a remapping operation.

### Handling Release Failures

Do not begin submitting new key-down events if previously held keys could not be released successfully during the transition.

Retain enough state to retry outstanding releases.

Use bounded, rate-limited recovery.

Do not discard unresolved release state.

Do not allow a configuration reload to silently bypass the keyboard state reconciliation implemented in Phase 1.

### Disabled State

When:

```toml
enabled = false
```

ZeroPad must:

- Release currently held keys.
- Stop producing new keyboard events from controller activity.
- Continue running.
- Continue accepting configuration reload commands.

Re-enabling the application must use a fresh controller snapshot.

### Controller Index Changes

If the selected controller index changes:

- Release existing held keys.
- Reset the previous controller's logical state.
- Begin polling the newly selected controller.

Do not reuse stale controller state.

### Analog Threshold Changes

Apply new thresholds only after a safe state transition.

Reset hysteresis state appropriately.

## 14. Integration with the Existing Engine

Do not duplicate the Phase 1 mapping engine.

Replace the hardcoded binding source with values from the typed configuration.

Preserve:

- Logical input processing.
- Duplicate-key safety.
- Keyboard state reconciliation.
- Analog hysteresis.
- Controller disconnect handling.
- Graceful shutdown.
- Existing SendInput failure handling.

The configuration system should be outside the hot path.

Suggested structure:

```text
config.toml
    |
    v
cpptoml Parser
    |
    v
Schema Validation
    |
    v
Typed Config
    |
    v
Safe Config Application
    |
    v
Existing Phase 1 Input Engine
    |
    v
SendInput
```

Do not make the input engine parse TOML directly.

## 15. Suggested Source Structure

Preserve the current repository organization.

Add files only where useful.

Suggested additions:

```text
ZeroPad/
├── include/
│   └── zeropad/
│       ├── config.hpp
│       ├── config_io.hpp
│       └── key_names.hpp
├── src/
│   ├── config.cpp
│   ├── config_io.cpp
│   └── key_names.cpp
├── third_party/
│   └── cpptoml/
└── tests/
    └── config_tests.cpp
```

These names are suggestions.

Do not split trivial implementations across excessive files.

A smaller organization is acceptable if it is cleaner.

## 16. Automated Tests

Extend the existing test suite.

Configuration tests must not require:

- Physical controller hardware.
- Actual keyboard injection.
- Administrator privileges.
- GUI interaction.

### 16.1 Default Configuration Tests

Verify:

1. Default configuration contains every Phase 1 logical input.
2. Default mappings match the previous implementation.
3. Default analog thresholds match Phase 1.
4. Default configuration passes validation.
5. Serialization of defaults produces valid TOML.
6. Re-parsing serialized defaults yields equivalent values.

### 16.2 Parsing Tests

Verify:

1. Valid complete configuration.
2. Valid partial configuration with optional fields omitted.
3. Missing required version.
4. Unsupported version.
5. Invalid TOML syntax.
6. Unknown sections.
7. Unknown fields.
8. Duplicate fields.
9. Incorrect value types.
10. Invalid keyboard names.
11. Explicit unassigned bindings.
12. Empty configuration file.
13. Oversized file.
14. Embedded NUL bytes.
15. Malformed UTF-8.
16. Excessively nested input.

Do not claim that parser safety tests provide complete protection against all malformed TOML inputs.

### 16.3 Numeric Validation Tests

Verify:

- Polling interval lower and upper bounds.
- Controller index range.
- Trigger threshold bounds.
- Trigger press/release ordering.
- Stick threshold bounds.
- Stick press/release ordering.
- Negative values.
- Values greater than allowed limits.
- Non-finite floating-point values.
- Wrong numeric types.
- Boundary values.

### 16.4 Binding Tests

Verify:

1. Every supported logical controller input can be configured.
2. Every supported key name can be parsed.
3. Invalid key names are rejected.
4. `None` disables a binding.
5. Multiple inputs may share one key.
6. Canonical key-name serialization is deterministic.
7. Default mapping preserves A-to-Space functionality.

### 16.5 Serialization Tests

Verify:

1. Default configuration round-trip.
2. Custom configuration round-trip.
3. Unassigned binding round-trip.
4. Floating-point threshold round-trip.
5. Stable serialization output.
6. Correct TOML syntax.
7. No unwanted extra fields.

### 16.6 File I/O Tests

Use temporary directories.

Verify:

1. Missing configuration file.
2. Successful initial file creation.
3. Existing valid configuration.
4. Existing invalid configuration.
5. Saving to an existing file.
6. Saving a new file.
7. Write failures where they can be reliably simulated.
8. Failed replacement leaves the previous valid configuration intact where the filesystem allows.
9. Temporary-file cleanup.
10. Configuration directory creation failure.

Do not create test files in the user's real ZeroPad configuration directory.

### 16.7 Reload Tests

Use a mocked controller and keyboard output.

Verify:

1. Valid reload succeeds.
2. Invalid reload preserves active configuration.
3. Unchanged reload produces no unnecessary keyboard events.
4. Remapping a held button releases the old key.
5. A new mapping becomes active after reconciliation.
6. Disabling releases held keys.
7. Re-enabling resumes processing.
8. Changing controller index resets previous input state.
9. Changing analog thresholds resets hysteresis correctly.
10. Duplicate-key behavior remains correct after reload.
11. Failed key-up prevents unsafe new key-down events.
12. Retry success permits normal processing to resume.

### 16.8 Regression Tests

All Phase 0 and Phase 1 tests must continue to pass.

Do not weaken existing assertions merely to make tests pass.

## 17. Manual Verification

When Windows and a controller are available, perform these checks:

1. Delete the configuration file.
2. Launch ZeroPad.
3. Verify default configuration creation.
4. Verify A-to-Space.
5. Edit A to use Enter.
6. Reload configuration.
7. Verify A-to-Enter.
8. Assign the same key to two buttons.
9. Verify correct shared-key behavior.
10. Hold a button while reloading a changed mapping.
11. Verify that the old key is released.
12. Introduce an invalid TOML setting.
13. Verify that reload is rejected without losing the previous mapping.
14. Restore a valid configuration.
15. Set `enabled = false` and reload.
16. Verify that keyboard injection stops.
17. Re-enable and verify normal operation.
18. Disconnect the controller while holding an input.
19. Verify key release.
20. Quit gracefully.

If hardware is unavailable, report the manual checks as unverified.

## 18. Documentation Requirements

Update README with:

- Configuration file location.
- How the file is created.
- Full configuration example.
- Supported controller input identifiers.
- Supported keyboard key names.
- Analog threshold behavior.
- How to disable a binding.
- How to reload configuration.
- How to disable and re-enable ZeroPad.
- How invalid settings are handled.
- What happens during a configuration reload.
- Known limitations.

Clearly explain that Phase 2 still uses a console interface.

The eventual Win32 settings UI will be implemented in Phase 3.

Users should not need to edit TOML manually once the later configuration UI is completed.

## 19. Explicitly Out of Scope

Do not implement:

- Win32 settings window
- System tray
- Controller visualization
- Gamepad diagrams
- Portable configuration mode
- Automatic file watching
- User profiles
- Multiple configuration files
- Configuration migrations
- Multiple simultaneous controllers
- Mouse emulation
- Key combinations
- Macros
- Turbo / rapid-fire
- GameInput
- DirectInput
- Raw HID
- Windows startup integration
- Automatic updates
- Generic TOML writer library
- Generic settings framework

Do not introduce architectural complexity for features that do not yet exist.

## 20. Implementation Workflow

Follow this order.

### Step 1 — Repository Inspection

Inspect the existing Phase 1 code.

Understand:

- Logical controller inputs.
- Mapping table.
- Keyboard state tracking.
- Analog thresholds.
- Controller polling.
- Shutdown handling.
- Current tests.
- CMake organization.

Identify the smallest integration points.

### Step 2 — Inspect cpptoml

Inspect the actual taqu/cpptoml source.

Confirm API usage, memory ownership, parser limitations, and build requirements.

Integrate the library without modifying it.

### Step 3 — Define the Configuration Model

Create typed configuration structures.

Centralize defaults and key-name conversion.

Ensure that all Phase 1 logical inputs are represented.

### Step 4 — Implement Parsing and Validation

Read TOML using cpptoml.

Convert the parse tree into the typed configuration.

Validate every field.

Reject incomplete or invalid semantics according to the schema rules.

### Step 5 — Implement Serialization

Write the minimal ZeroPad-specific TOML serializer.

Add round-trip tests.

### Step 6 — Implement File Persistence

Add configuration path resolution.

Implement default file creation and safe saving.

Test error handling.

### Step 7 — Connect Configuration to the Input Engine

Replace fixed mappings and thresholds with typed configuration values.

Preserve all existing state-management behavior.

### Step 8 — Implement Safe Reload

Add the minimal console reload command.

Implement release-first configuration application.

Handle release failures safely.

### Step 9 — Expand Tests

Implement parsing, validation, serialization, file I/O, and reload tests.

Run all existing tests.

### Step 10 — Verify and Document

Run a Release build.

Run CTest.

Perform manual checks if possible.

Update README.

Summarize limitations.

## 21. Definition of Done

Phase 2 is complete only when:

- [ ] The intended taqu/cpptoml library is integrated.
- [ ] No alternative TOML parser is introduced.
- [ ] Default configuration is represented by typed C++ data.
- [ ] All Phase 1 bindings can be configured.
- [ ] Analog thresholds can be configured.
- [ ] Polling interval can be configured.
- [ ] Controller index can be configured.
- [ ] Configuration can enable or disable input conversion.
- [ ] Missing configuration files are handled.
- [ ] Existing valid configuration files load correctly.
- [ ] Invalid configuration files are not overwritten.
- [ ] Configuration validation is complete.
- [ ] Configuration serialization is deterministic.
- [ ] Configuration round-trip tests pass.
- [ ] Saving does not directly truncate the existing file.
- [ ] Runtime reload is supported.
- [ ] Invalid reload preserves the active configuration.
- [ ] Reloading does not leave incorrectly held keyboard keys.
- [ ] Failed releases are handled conservatively.
- [ ] No file I/O occurs in the input polling hot path.
- [ ] All existing Phase 0 tests pass.
- [ ] All existing Phase 1 tests pass.
- [ ] New Phase 2 tests pass.
- [ ] CMake Release build succeeds on Windows 11 x64.
- [ ] README documents configuration behavior.
- [ ] No out-of-scope functionality has been implemented.

## 22. Final Deliverables

Provide:

1. Updated C++ source files.
2. Updated headers.
3. Integrated cpptoml dependency.
4. Updated CMake configuration.
5. Configuration parser and validator.
6. Configuration serializer.
7. Safe configuration persistence.
8. Runtime reload mechanism.
9. Expanded automated tests.
10. Updated README.
11. Build and test results.
12. Summary of architectural changes.
13. Known limitations and unresolved issues.

If the development environment cannot build or test Windows-specific code, distinguish actual verification from assumptions.

Do not claim tests passed unless they were executed successfully.

## 23. Final Engineering Principles

**Configuration is not part of the hot path.**

Parse once and use validated, typed data during normal operation.

**Invalid configuration must never partially modify the active engine.**

Use candidate configurations and all-or-nothing validation.

**Keyboard safety is more important than preserving continuous input during reload.**

Prefer releasing keys before applying changed mappings.

**Saving must protect existing valid configuration data.**

Use temporary files and controlled replacement.

**Keep cpptoml isolated.**

The input engine should not know about TOML internals.

**Avoid overengineering.**

Do not build a generic settings framework, configuration database, or extensible serialization system.

**Preserve Phase 0 and Phase 1.**

All existing behavior and tests must remain valid.

**Stop at Phase 2.**

Complete and verify the configuration layer, then stop.

Do not proceed to Phase 3 automatically.
