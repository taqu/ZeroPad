# ZeroPad — Phase 4 Implementation Instructions

## 1. Project Overview

ZeroPad is an open-source, ultra-lightweight Windows utility written in C++20.

It converts XInput-compatible game controller inputs into keyboard inputs using native Windows APIs.

The project prioritizes:

1. Reliable keyboard state management.
2. Low CPU and memory consumption.
3. Minimal executable size.
4. Native Windows integration.
5. Simple user interaction.
6. Minimal dependencies.
7. Predictable application behavior.
8. Maintainable, straightforward C++ code.

Previous development phases:

- **Phase 0:** Minimal A-to-Space mapping and basic input processing.
- **Phase 1:** Full XInput mapping engine, analog threshold processing, hysteresis, simultaneous inputs, and keyboard state reconciliation.
- **Phase 2:** TOML configuration using taqu/cpptoml, validation, serialization, persistence, and runtime configuration changes.
- **Phase 3:** Native Win32 settings GUI, keyboard binding editor, key capture, and safe configuration editing.

**Phase 4 must transform ZeroPad into a reliable, lightweight, system-tray-based background utility.**

Preserve the existing engine, configuration system, and settings GUI.

Do not rewrite completed components unless necessary to resolve a concrete correctness or lifecycle problem.

## 2. Phase 4 Objectives

Implement:

1. Windows notification area integration.
2. Background operation after closing the settings window.
3. A simple tray context menu.
4. Opening and focusing the existing settings window.
5. Enabling and disabling keyboard mapping from the tray.
6. Explicit application exit.
7. Single-instance enforcement.
8. Handling attempts to launch an already-running instance.
9. Recovery from controller disconnection and reconnection.
10. Sleep, hibernation, and resume handling.
11. Windows Explorer restart recovery.
12. Reliable shutdown and keyboard release behavior.
13. Minimal diagnostic logging.
14. CPU, memory, and polling performance measurements.
15. Automated lifecycle and recovery tests.

Phase 4 should make ZeroPad suitable for normal everyday use.

Do not implement Phase 5 release automation or distribution infrastructure beyond what is necessary to verify Phase 4.

## 3. Technical Constraints

Target:

- Windows 11 x64
- C++20
- MSVC
- CMake

Use:

- Win32 API
- Windows Common Controls
- Shell_NotifyIconW
- XInput
- SendInput
- Existing taqu/cpptoml configuration integration
- Standard C++ library

Do not introduce:

- Qt
- wxWidgets
- MFC
- WTL
- SDL
- Dear ImGui
- WebView2
- Electron
- Background Windows services
- Third-party event-loop frameworks
- Additional configuration libraries
- Windows drivers

Do not create a new input engine.

Continue using the existing Phase 1–3 implementation.

## 4. Application Lifecycle

The most important Phase 4 change is separating the lifetime of the application from the lifetime of the settings window.

### 4.1 Required Behavior

On normal launch:

1. Check whether ZeroPad is already running.
2. Load the active configuration.
3. Initialize the input engine.
4. Create the application's tray notification infrastructure.
5. Register the tray icon.
6. Open the settings window on an ordinary interactive launch.

While the application is running:

- The input engine continues processing controller input.
- The settings window may be opened or closed.
- Closing the settings window must not terminate the application.
- Closing the settings window must not release currently held keys solely because the window closed.
- The tray icon provides access to the application.

On explicit Exit:

1. Stop accepting new controller-driven key-down requests.
2. Safely release injected keyboard keys.
3. Stop the input engine.
4. Remove the tray icon.
5. Destroy application windows and other resources.
6. Release the single-instance resource.
7. Exit the process.

### 4.2 Window Close vs Application Exit

Define these operations separately.

**Settings window Close:**

- Discard unsaved configuration changes.
- Close or hide the settings window.
- Keep the engine running.
- Keep the tray icon active.
- Do not exit the process.

**Tray Exit:**

- Terminate ZeroPad gracefully.
- Stop keyboard mapping.
- Release held keys.
- Remove the notification icon.
- Exit the process.

Do not use the same unconditional shutdown handler for both operations.

### 4.3 Unsaved Settings

Preserve Phase 3 editing semantics.

When the user closes the settings window:

- Treat it as Cancel.
- Do not save unfinished edits.
- Keep the active configuration unchanged.
- Close any child editing dialogs safely.
- Release temporary key-capture suspension if active.

Do not silently save incomplete configuration edits.

## 5. System Tray Integration

Use the Windows notification area API.

Recommended APIs:

- Shell_NotifyIconW
- NOTIFYICONDATAW
- NIM_ADD
- NIM_MODIFY
- NIM_DELETE
- NIM_SETVERSION
- NOTIFYICON_VERSION_4

Use a standard Win32 message window to receive notification events.

A hidden top-level window or dedicated message-handling window is acceptable.

Do not introduce a custom tray framework.

### 5.1 Tray Icon

Provide a simple, recognizable application icon.

Requirements:

- Use a small icon resource embedded in the executable.
- Do not download icon assets at runtime.
- Avoid elaborate graphics.
- Do not implement animated tray icons.
- Use the same icon for the application window where appropriate.
- Include suitable Windows icon sizes.

A simple monochrome or conventional application icon is sufficient.

The project does not require a modern visual identity.

### 5.2 Tooltip

Display a short tooltip.

Suggested text:

`ZeroPad — Enabled`

or:

`ZeroPad — Disabled`

Update the tooltip when mapping state changes.

Do not update it every polling cycle.

Do not include rapidly changing controller data in the tooltip.

### 5.3 Icon Registration

Register the icon using Shell_NotifyIconW.

After successful NIM_ADD, call NIM_SETVERSION with NOTIFYICON_VERSION_4.

Check API return values.

Handle registration failures gracefully.

Do not assume that NIM_ADD always succeeds.

Do not repeatedly recreate the icon during ordinary operation.

### 5.4 Explorer Restart Recovery

Windows Explorer may restart independently of ZeroPad.

Register the `TaskbarCreated` window message using RegisterWindowMessageW.

When that message is received:

1. Re-register the tray icon.
2. Apply the expected notification icon version.
3. Restore the current tooltip and state.
4. Continue running the existing input engine.

Do not restart the ZeroPad process.

Do not create a second input engine.

Do not lose active configuration.

Handle repeated registration requests safely.

## 6. Tray Context Menu

Provide a simple native popup menu.

Recommended structure:

```text
ZeroPad
-------------------------
Settings
-------------------------
Disable Mapping
-------------------------
Exit
```

When mapping is disabled:

```text
ZeroPad
-------------------------
Settings
-------------------------
Enable Mapping
-------------------------
Exit
```

Use standard Windows menu APIs:

- CreatePopupMenu
- AppendMenuW
- TrackPopupMenuEx
- DestroyMenu
- SetForegroundWindow

Use the appropriate notification callback behavior for NOTIFYICON_VERSION_4.

Do not assume the mouse event encoding is identical to older notification icon versions.

### Settings

Selecting Settings must:

- Open the existing settings window if closed.
- Restore it if minimized.
- Bring it to the foreground when permitted by Windows.
- Avoid creating duplicate settings windows.

If foreground activation is restricted by Windows, do not use invasive focus-stealing workarounds.

### Enable / Disable Mapping

Selecting Disable Mapping must:

1. Request a safe engine suspension.
2. Release all keys currently held by ZeroPad.
3. Prevent new controller-driven key-down events.
4. Update the tray menu and tooltip when the transition succeeds.

Selecting Enable Mapping must:

1. Resume mapping safely.
2. Read a fresh controller state.
3. Avoid replaying stale input states.
4. Update the tray menu and tooltip when the transition succeeds.

Do not directly manipulate keyboard injection state from the tray window procedure.

Reuse the existing engine transition mechanism.

### Exit

Selecting Exit must begin the normal graceful shutdown process.

Do not immediately call ExitProcess.

Do not terminate the input worker before attempting to release its held keys.

### Tray Interactions

Required:

- Single left click: optional; preferably no action.
- Double left click: open Settings.
- Right click: open context menu.

Keep behavior consistent.

Do not add complex mouse gestures.

## 7. Mapping Enabled State

Phase 2 introduced the persistent configuration property:

`general.enabled`

Phase 4 must distinguish:

- Configured enabled state.
- Temporary runtime suspension.
- Key-capture suspension.
- Recovery or shutdown suspension.

Do not introduce multiple unrelated boolean flags with ambiguous meanings.

### Recommended State Model

Define a small runtime state model.

For example:

- Running
- Disabled
- SuspendedForCapture
- Recovering
- Stopping

These names are illustrative.

Use the simplest model compatible with the existing architecture.

### Tray Toggle Persistence

The tray Enable/Disable command should be a runtime-only override.

Do not automatically rewrite config.toml when the user toggles mapping from the tray.

Recommended behavior:

- `general.enabled` remains the persisted startup preference.
- The tray toggle affects only the current session.
- A successful Save from the settings window applies the newly saved configuration, including its enabled preference.
- On restart, the persisted configuration determines the initial enabled state.

Document this behavior clearly.

The tray tooltip and menu must reflect the effective runtime mapping state, not merely the persisted configuration flag.

When suspension is active for capture, recovery, or shutdown, do not allow a tray command to bypass that suspension.

## 8. Single-Instance Enforcement

Only one ZeroPad instance should run per interactive user session.

The goal is to avoid:

- Duplicate controller polling.
- Duplicate keyboard injection.
- Conflicting configuration writes.
- Multiple tray icons.

### 8.1 Recommended Implementation

Use a named Windows mutex or an equivalently small OS-level ownership mechanism.

Relevant APIs:

- CreateMutexW
- GetLastError
- CloseHandle

Handle:

- Successful first-instance creation.
- ERROR_ALREADY_EXISTS.
- Resource creation failures.
- Access-denied conditions.

Do not interpret every mutex creation failure as proof that another ZeroPad instance is running.

Prefer session-local instance ownership over a machine-global mutex.

Do not prevent different Windows users in separate sessions from running their own ZeroPad instances.

### 8.2 Second Launch Behavior

When ZeroPad is launched again:

1. Detect the existing instance.
2. Do not create a second input engine.
3. Send a request to the running instance to open Settings.
4. Exit the second process.

Use a minimal, bounded local process communication mechanism.

An application-specific registered window message or another appropriate Win32 mechanism is acceptable.

### 8.3 Communication Safety

Do not broadcast a generic message that unrelated applications could interpret.

Avoid accepting arbitrary external commands.

Restrict second-instance communication to opening the settings window.

Do not expose configuration editing, key injection, or process termination through an unauthenticated IPC interface.

Handle a stale or unresponsive existing instance without hanging indefinitely.

Do not implement a general IPC framework.

## 9. Controller Disconnection and Reconnection

Preserve the Phase 1 disconnection behavior.

When the active controller disconnects:

1. Recognize the disconnected state.
2. Treat all logical inputs as released.
3. Reconcile held keyboard keys.
4. Reset analog hysteresis state.
5. Retain unresolved release information if keyboard injection fails.
6. Continue checking for reconnection.
7. Update connection status when appropriate.

Do not repeatedly inject releases that have already succeeded.

Do not repeatedly log the same disconnection state.

### Reconnection

When the controller reconnects:

1. Detect the new controller state.
2. Avoid reusing stale controller snapshots.
3. Reset logical state as necessary.
4. Respect the effective mapping enabled state.
5. Resume processing normally once the engine is safe.

Do not automatically enable mapping if it was disabled by the user.

### Polling Frequency

Use the existing configured polling interval while connected.

When disconnected, use a slower retry interval if already supported.

Avoid busy waiting.

Do not introduce a dedicated connection-monitoring thread if the existing input worker can handle reconnection.

## 10. Sleep, Hibernation, and Resume

Handle Windows power-management transitions conservatively.

Relevant messages include:

- WM_POWERBROADCAST
- PBT_APMSUSPEND
- PBT_APMRESUMEAUTOMATIC
- PBT_APMRESUMESUSPEND

### 10.1 Suspend Handling

When the system announces suspension:

1. Stop accepting new mapping-driven key-down events.
2. Request release of held keyboard keys.
3. Invalidate current controller input snapshots.
4. Reset analog hysteresis state.
5. Record that resume reconciliation is required.

Do not perform lengthy or indefinite blocking work inside WM_POWERBROADCAST.

Do not assume there is sufficient time to complete arbitrarily long cleanup before suspension.

### 10.2 Resume Handling

On resume:

1. Treat previous controller state as stale.
2. Recheck the configured XInput controller.
3. Reset connection and analog processing state as appropriate.
4. Reconcile outstanding keyboard releases.
5. Resume mapping only after the engine reaches a safe state.
6. Preserve the user's effective enabled/disabled preference.

Do not inject key-down events based on cached pre-suspend controller state.

### 10.3 Repeated Resume Notifications

Windows may issue multiple related resume notifications.

Handle them idempotently.

Repeated notifications must not:

- Spawn additional polling threads.
- Create duplicate tray icons.
- Reset saved configuration.
- Generate duplicate keyboard events.

### 10.4 Unexpected Power Transitions

Do not claim guaranteed key release if Windows suspends or terminates the process without allowing cleanup to complete.

Implement best-effort recovery.

Document the limitation.

## 11. Windows Session Events

Account for ordinary Windows desktop lifecycle behavior.

Relevant situations include:

- User locks the session.
- User unlocks the session.
- User signs out.
- Windows is shutting down.

### Lock and Unlock

Do not inject new keyboard input into a locked or noninteractive session.

Prefer suspending mapping while locked and resuming safely after unlock.

If implementing session notifications, use the appropriate Windows session change APIs.

Do not build a session-monitoring subsystem beyond what is needed.

### Sign-Out and Shutdown

Handle the normal Windows shutdown notifications appropriately.

Request input engine shutdown.

Perform bounded best-effort key release.

Do not block Windows shutdown indefinitely.

Do not use forceful termination as the normal exit mechanism.

## 12. Graceful Application Shutdown

Implement one centralized shutdown path.

All explicit termination paths should use it.

Examples:

- Tray Exit.
- Application shutdown request.
- Windows session termination.
- Fatal application initialization failure after the engine has started.

### Required Shutdown Sequence

1. Mark the application as stopping.
2. Reject new engine start or resume commands.
3. Stop generating new keyboard key-down events.
4. Request desired keyboard states to become released.
5. Reconcile outstanding key-up events.
6. Apply bounded retries for failed releases.
7. Stop and join the input worker.
8. Remove the tray notification icon.
9. Destroy windows and dialogs.
10. Release OS handles and synchronization resources.
11. Exit the process.

Adapt the sequence to the existing engine architecture while preserving these guarantees.

### Failure Handling

If a key release fails:

- Do not pretend cleanup succeeded.
- Make bounded recovery attempts.
- Record the unresolved failure.
- Avoid an infinite shutdown loop.

Do not depend on C++ destructors alone to release keys.

Do not use TerminateThread.

Do not use ExitProcess as the normal application exit procedure.

### Race Conditions

Test shutdown while:

- A controller button is held.
- The settings window is open.
- Key capture is active.
- Configuration is being saved.
- The controller is disconnected.
- Resume recovery is in progress.

No new mapping-generated key-down events may occur after shutdown begins.

## 13. Engine and GUI Threading

Preserve the Phase 3 threading design where possible.

A suitable architecture is:

```text
Main / UI Thread
    |
    +-- Win32 Message Loop
    |
    +-- Settings Window
    |
    +-- System Tray
    |
    +-- Power / Session Notifications
    |
    v
Engine Command Interface
    |
    v
Input Worker Thread
    |
    +-- XInput Polling
    |
    +-- Logical Input Mapping
    |
    +-- Keyboard State Reconciliation
    |
    v
SendInput
```

Do not create an independent worker for each feature.

In particular, do not add separate permanent threads for:

- System tray.
- Controller connection.
- Power monitoring.
- Logging.
- Metrics collection.

Use Windows messages and the existing worker wherever practical.

### Thread Safety

Maintain safe synchronization between the UI thread and input worker.

Requirements:

- No unsynchronized access to mutable configuration.
- No direct manipulation of engine state from tray callbacks.
- No worker thread calls into Win32 UI controls.
- No locks held across lengthy UI operations.
- No indefinite blocking of the main message loop.
- No new key-down events after the engine enters shutdown.

Use a small, explicit command mechanism.

Avoid complex actor frameworks or event buses.

## 14. Recovery from Unexpected Errors

Implement conservative error handling for recoverable situations.

Examples:

- XInput temporarily unavailable.
- Controller disconnects.
- Tray icon registration fails.
- Explorer restarts.
- SendInput fails.
- Configuration file becomes temporarily inaccessible.
- Mapping suspension fails.

### Error Handling Policy

For recoverable errors:

1. Record the error when useful.
2. Enter a safe state.
3. Retry only when reasonable.
4. Avoid high-frequency retries.
5. Restore normal operation once the condition is resolved.

For unrecoverable initialization errors:

1. Report the error.
2. Clean up initialized resources.
3. Exit gracefully.

Do not add complex self-healing infrastructure.

Do not restart the process automatically.

Do not introduce a watchdog service.

## 15. Minimal Diagnostic Logging

Add basic diagnostics suitable for an everyday utility.

Do not introduce a logging framework.

A lightweight logging mechanism is sufficient.

### Recommended Log Events

- Application startup.
- Application shutdown.
- Configuration load failure.
- Controller connected.
- Controller disconnected.
- Mapping enabled or disabled.
- Keyboard injection failure.
- Tray icon initialization failure.
- Unexpected recovery failure.

Do not log every controller poll.

Do not log every successful key press or release.

### Logging Behavior

Prefer no continuous disk logging during normal successful operation.

Diagnostic logging may be enabled explicitly for troubleshooting.

If file logging is supported:

- Use a file under `%LOCALAPPDATA%\ZeroPad`.
- Bound file growth.
- Avoid excessive writes.
- Do not block the polling hot path.
- Do not record unnecessary personal information.

Do not implement a sophisticated log rotation framework.

A small fixed-size or bounded diagnostic log is sufficient.

## 16. Performance Requirements

The background application must remain lightweight.

Primary goals:

- Minimal CPU usage while idle.
- Minimal memory usage.
- No unnecessary wakeups.
- No per-frame dynamic allocation where practical.
- No repeated configuration parsing.
- No per-frame logging.
- No high-frequency UI updates.
- No unnecessary threads.

Preliminary targets:

| Metric | Target |
|---|---|
| Release executable | Approximately 1 MB or less |
| Resident memory | Approximately 10 MB or less |
| Connected polling | Configurable, default 4 ms |
| Disconnected polling | Reduced polling frequency |
| Idle CPU usage | As low as reasonably practical |

These are engineering targets, not unconditional acceptance criteria.

Do not sacrifice correctness or maintainability solely to meet a specific binary size.

### Timing

Do not assume a 4 ms sleep produces exact 250 Hz polling.

Measure actual polling intervals.

Do not increase system-wide timer resolution without evidence that it is necessary.

Avoid spin loops.

## 17. Performance Measurement

Add a minimal, reproducible performance evaluation procedure.

Measure:

1. Release executable size.
2. Process working set.
3. Private committed memory.
4. Idle CPU usage.
5. CPU usage with a connected controller.
6. CPU usage with a disconnected controller.
7. Actual polling intervals.
8. Controller-to-SendInput processing latency where measurable.

### Measurement Method

Use Windows performance counters or appropriate Win32 process APIs.

Do not implement a built-in performance dashboard.

A small optional diagnostic mode or standalone test utility is acceptable.

### Input Latency

Distinguish:

- Polling interval.
- Time spent processing a sampled controller state.
- Time until SendInput is called.
- End-to-end delay observed by a target application.

Do not claim to measure physical controller-to-application latency if the instrumentation only measures part of the pipeline.

Use a monotonic high-resolution clock such as QueryPerformanceCounter where appropriate.

Avoid adding performance instrumentation to the normal hot path unless it has negligible cost.

### Reporting

Provide a short performance report with:

- Test environment.
- Build configuration.
- Measurement method.
- Sample duration.
- Measured values.
- Known measurement limitations.

Do not fabricate measurements.

If performance measurements cannot be performed, report them as unverified.

## 18. Automated Tests

Expand the existing CTest-based test suite.

Tests should avoid dependence on real hardware wherever possible.

Use existing mock controller and keyboard interfaces.

### 18.1 Tray State Tests

Verify:

1. Tray actions map to the correct application commands.
2. Settings opens the existing window.
3. Repeated Settings requests do not create duplicate windows.
4. Disable requests safe mapping suspension.
5. Enable requests safe mapping resumption.
6. Exit requests graceful shutdown.
7. Tooltip state reflects effective mapping state.
8. Failed engine transitions do not falsely update enabled status.

### 18.2 Application Lifecycle Tests

Verify:

1. Application startup initializes the required components.
2. Closing Settings does not stop the engine.
3. Closing Settings does not modify active mappings.
4. Closing Settings discards unsaved edits.
5. Reopening Settings loads the current active configuration.
6. Repeated open/close cycles do not leak application-level resources.
7. Explicit Exit stops the engine.
8. No mapping-driven key-down occurs after shutdown begins.

### 18.3 Single-Instance Tests

Verify:

1. First instance obtains ownership.
2. Second instance does not start another engine.
3. Second instance requests the original settings window.
4. An unresponsive first instance does not hang the second instance indefinitely.
5. Mutex acquisition errors are distinguished from an already-running instance.
6. Ownership resources are released on normal shutdown.

### 18.4 Controller Recovery Tests

Verify:

1. Disconnect releases held keys.
2. Disconnect while disabled does not generate new presses.
3. Reconnection reads a fresh state.
4. Reconnection does not reuse stale stick values.
5. Repeated disconnect events are harmless.
6. Keyboard release failures remain tracked.
7. Successful recovery restores correct behavior.

### 18.5 Power Recovery Tests

Simulate power events where possible.

Verify:

1. Suspend requests mapping suspension.
2. Suspend invalidates previous controller state.
3. Resume requests fresh controller sampling.
4. Resume respects disabled state.
5. Repeated resume notifications are idempotent.
6. Pending keyboard releases are reconciled.
7. No stale pre-suspend key-down events are replayed.
8. No duplicate input threads are created.

### 18.6 Shutdown Tests

Verify:

1. Shutdown while a key is held requests release.
2. Shutdown while disabled remains safe.
3. Shutdown during capture mode restores consistent state.
4. Shutdown during configuration activity avoids deadlock.
5. Failed key-up operations use bounded recovery.
6. Worker termination completes.
7. The tray icon cleanup path is invoked.
8. No unsafe events are generated after shutdown begins.

### 18.7 Regression Tests

All previous Phase 0, Phase 1, Phase 2, and Phase 3 tests must continue to pass.

Do not weaken tests to accommodate lifecycle changes.

## 19. Manual Verification

Perform manual testing on Windows 11 where available.

### Tray Behavior

- Launch ZeroPad.
- Verify tray icon appears.
- Close Settings.
- Verify the process remains running.
- Verify controller mapping still works.
- Open Settings from the tray.
- Verify no second settings window is created.
- Disable mapping.
- Verify held keys are released.
- Re-enable mapping.
- Verify normal mapping resumes.
- Exit from the tray.
- Verify the process terminates.

### Single Instance

- Launch ZeroPad.
- Launch the same executable again.
- Verify that the existing settings window opens.
- Verify only one engine remains active.
- Verify only one functional tray icon is present.

### Controller Recovery

- Connect a controller.
- Hold a mapped button.
- Disconnect the controller.
- Verify keyboard release.
- Reconnect the controller.
- Verify normal mapping.
- Repeat several times.

### Power Recovery

- Run ZeroPad.
- Put Windows into sleep mode.
- Resume Windows.
- Verify the controller is detected.
- Verify keyboard mapping works.
- Verify no keys remain incorrectly held.
- Repeat with mapping disabled.

### Explorer Restart

- Run ZeroPad.
- Restart Windows Explorer using a controlled manual procedure.
- Verify the notification icon is restored.
- Verify the input engine continues running.
- Verify the tray menu remains functional.

### Window Lifecycle

- Open and close Settings repeatedly.
- Edit configuration and press Cancel.
- Verify no unsaved changes are applied.
- Open key capture and cancel.
- Verify engine suspension state is restored.
- Exit while Settings is open.

Do not claim manual tests passed unless they were actually performed.

## 20. Source Organization

Preserve the existing project structure.

Suggested additions:

```text
ZeroPad/
├── include/
│   └── zeropad/
│       ├── tray.hpp
│       ├── app_lifecycle.hpp
│       ├── single_instance.hpp
│       └── diagnostics.hpp
├── src/
│   ├── tray.cpp
│   ├── app_lifecycle.cpp
│   ├── single_instance.cpp
│   └── diagnostics.cpp
├── resources/
│   ├── zeropad.ico
│   └── zeropad.rc
└── tests/
    └── lifecycle_tests.cpp
```

These names are suggestions, not mandatory architecture.

Keep files small and focused.

Do not introduce a general application framework.

If the existing Phase 3 application controller can manage lifecycle commands, extend it instead of duplicating its responsibilities.

## 21. Build Requirements

Preserve:

- C++20.
- CMake.
- MSVC.
- Windows 11 x64.
- Existing configuration and input engine dependencies.
- Existing automated tests.

Link the required Windows system libraries.

Use Unicode Win32 APIs.

Preserve Windows GUI subsystem behavior introduced in Phase 3.

Required build commands:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Do not require external package managers.

Do not introduce new runtime dependencies.

Do not implement a Windows installer.

## 22. Documentation

Update README with:

- How to start ZeroPad.
- How background operation works.
- How to open Settings from the tray.
- How to enable and disable mapping.
- How to exit ZeroPad.
- How single-instance behavior works.
- How configuration persistence differs from runtime tray toggles.
- How controller reconnection is handled.
- What happens during Windows sleep and resume.
- Known SendInput limitations.
- Known input cleanup limitations.
- How to enable diagnostics, if supported.
- Basic resource usage measurements where verified.

Keep the README user-oriented and concise.

Do not claim Windows startup integration exists.

Do not document Phase 5 release features as implemented.

## 23. Explicitly Out of Scope

Do not implement:

- Windows startup registration.
- Windows services.
- Automatic updates.
- Installer packages.
- Multiple profiles.
- Automatic profile switching.
- Mouse emulation.
- Keyboard macros.
- Rapid-fire functions.
- Custom GUI themes.
- Controller diagrams.
- Multiple simultaneous controllers.
- GameInput support.
- DirectInput support.
- Generic HID support.
- Virtual controller drivers.
- Global keyboard shortcuts.
- System-wide keyboard hooks.
- Plugin infrastructure.
- Cloud synchronization.
- Telemetry.
- Crash reporting services.
- Performance dashboards.
- GitHub release automation.

Do not create placeholders for these features.

## 24. Implementation Workflow

Follow this sequence.

### Step 1 — Inspect Existing Code

Inspect the entire Phase 3 implementation.

Identify:

- Main application lifecycle.
- Window message loop.
- Input worker.
- Configuration ownership.
- Safe suspension and resumption.
- Keyboard state tracking.
- Settings window creation and destruction.
- Shutdown handling.
- Existing automated tests.

Determine the smallest changes needed.

### Step 2 — Separate Window and Process Lifetimes

Modify application lifecycle so that closing Settings does not terminate ZeroPad.

Keep the existing engine running.

Ensure explicit Exit remains available.

### Step 3 — Implement System Tray

Add Shell_NotifyIconW integration.

Implement:

- Icon registration.
- Tooltip.
- Context menu.
- Settings action.
- Enable/Disable action.
- Exit action.
- Icon removal.

### Step 4 — Implement Explorer Recovery

Handle TaskbarCreated.

Verify icon re-registration.

Avoid duplicating engine or UI objects.

### Step 5 — Implement Single Instance

Add session-local instance ownership.

Ensure a second launch opens the existing Settings window rather than running another engine.

Use bounded communication and error handling.

### Step 6 — Centralize Lifecycle Transitions

Create or extend a small lifecycle coordinator.

Handle:

- Running.
- Disabled.
- Capture suspension.
- Recovery.
- Stopping.

Avoid conflicting state flags.

### Step 7 — Implement Power Recovery

Handle suspend and resume notifications.

Reset stale controller state.

Resume only after safe reconciliation.

### Step 8 — Verify Controller Recovery

Review existing disconnect and reconnect handling.

Correct only issues exposed by Phase 4 requirements.

### Step 9 — Improve Shutdown

Consolidate explicit exit paths.

Verify key release, worker termination, and resource cleanup.

### Step 10 — Add Diagnostics

Implement minimal event-based diagnostics.

Avoid disk activity in the polling hot path.

### Step 11 — Measure Performance

Build Release.

Measure executable size, memory usage, CPU usage, and polling intervals.

Document measurement methodology.

### Step 12 — Expand Tests

Implement automated lifecycle, tray command, single-instance, recovery, and shutdown tests.

Run all previous regression tests.

### Step 13 — Manual Verification

Perform available Windows 11 integration tests.

Clearly document tests that could not be performed.

### Step 14 — Update Documentation

Update README with background operation and exit procedures.

Summarize known limitations.

Stop after completing Phase 4.

## 25. Definition of Done

Phase 4 is complete only when:

- [ ] ZeroPad registers a working system tray icon.
- [ ] Closing Settings keeps ZeroPad running.
- [ ] Input mapping continues after Settings closes.
- [ ] Settings can be reopened from the tray.
- [ ] Repeated Settings requests do not create duplicate windows.
- [ ] Tray Enable/Disable works safely.
- [ ] Disabled state prevents new key-down events.
- [ ] Runtime tray toggles do not unexpectedly overwrite configuration.
- [ ] Tray Exit terminates the application gracefully.
- [ ] Held keys are released on normal shutdown.
- [ ] Explorer restart restores the tray icon.
- [ ] Single-instance enforcement works.
- [ ] Second launch opens the existing Settings window.
- [ ] Controller disconnection is handled safely.
- [ ] Controller reconnection restores normal input processing.
- [ ] Suspend and resume handling is implemented.
- [ ] Stale controller states are not replayed after resume.
- [ ] Session shutdown is handled on a best-effort basis.
- [ ] Engine lifecycle transitions are thread-safe.
- [ ] No unnecessary permanent threads are introduced.
- [ ] Diagnostic logging is bounded.
- [ ] No continuous per-frame disk logging occurs.
- [ ] CPU and memory usage are measured where possible.
- [ ] Polling performance is measured where possible.
- [ ] Phase 0 regression tests pass.
- [ ] Phase 1 regression tests pass.
- [ ] Phase 2 regression tests pass.
- [ ] Phase 3 regression tests pass.
- [ ] Phase 4 automated tests pass.
- [ ] Windows 11 x64 Release build succeeds.
- [ ] README is updated.
- [ ] No Phase 5 functionality is implemented.

## 26. Final Deliverables

Provide:

1. Updated ZeroPad application source code.
2. System tray implementation.
3. Single-instance handling.
4. Updated application lifecycle management.
5. Power and session recovery handling.
6. Safe shutdown improvements.
7. Minimal diagnostic logging.
8. Performance measurement results, if available.
9. Updated automated tests.
10. Updated CMake configuration, if needed.
11. Updated resources and application icon.
12. Updated README.
13. Build and test results.
14. Summary of architectural changes.
15. Known limitations and unresolved issues.

Distinguish verified functionality from unverified functionality.

Do not fabricate performance measurements or test results.

## 27. Final Engineering Principles

**ZeroPad is a utility, not a framework.**

Avoid unnecessary abstraction layers and service infrastructure.

**Settings window lifetime is independent of application lifetime.**

Closing Settings must not stop keyboard mapping.

**One process, one engine, one tray icon.**

Prevent duplicate instances and redundant input processing.

**Keyboard state safety is mandatory.**

Disconnect, suspend, disable, and exit operations must reconcile outstanding keys as safely as possible.

**Recovery must be idempotent.**

Repeated notifications must not create duplicate resources or replay stale input.

**Keep the polling hot path minimal.**

No unnecessary allocations, disk writes, or GUI updates.

**Prefer native Windows APIs.**

Do not introduce third-party frameworks.

**Preserve all previous phases.**

Existing input mapping, configuration, and GUI functionality must continue to work.

**Stop at Phase 4.**

Implement, test, and document Phase 4 completely.

Do not automatically proceed to Phase 5.
