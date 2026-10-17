# ZeroPad — Phase 5 Implementation Instructions

## 1. Project Overview

ZeroPad is an open-source, ultra-lightweight Windows utility written in C++20.

It converts XInput-compatible game controller inputs into keyboard inputs using native Windows APIs.

The project's primary goals are:

1. Correct and reliable input conversion.
2. Minimal CPU and memory overhead.
3. Small executable size.
4. Simple native Windows settings.
5. Predictable background operation.
6. No unnecessary runtime dependencies.
7. Straightforward installation-free distribution.
8. Long-term maintainability.

Previous development phases:

- **Phase 0:** Minimal XInput-to-keyboard conversion.
- **Phase 1:** Full controller mapping engine, analog processing, hysteresis, and keyboard state management.
- **Phase 2:** TOML configuration using taqu/cpptoml, validation, persistence, and safe configuration changes.
- **Phase 3:** Native Win32 settings GUI and keyboard binding editor.
- **Phase 4:** System tray integration, background operation, lifecycle management, disconnection recovery, and power-management handling.

**Phase 5 must prepare ZeroPad for its first public stable release, v1.0.0.**

This is primarily a quality assurance, documentation, packaging, and release engineering phase.

Do not introduce unrelated features.

## 2. Phase 5 Objectives

Complete the following:

1. Review and stabilize the existing codebase.
2. Fix reproducible defects and reliability problems.
3. Verify input state management under failure conditions.
4. Perform comprehensive automated regression testing.
5. Perform Windows 11 integration testing where possible.
6. Verify GUI and system tray behavior.
7. Measure and document resource usage.
8. Review executable dependencies.
9. Establish reproducible Release build instructions.
10. Implement GitHub Actions CI.
11. Create a portable ZIP distribution.
12. Review OSS license compliance.
13. Improve README and user-facing documentation.
14. Document Windows security and compatibility limitations.
15. Prepare v1.0.0 release notes.
16. Establish explicit release acceptance criteria.
17. Produce a final release-readiness report.

Do not publish the release automatically without explicit authorization.

## 3. Technical Constraints

Target platform:

- Windows 11 x64
- C++20
- MSVC
- CMake
- XInput
- SendInput
- Win32 API
- Windows Common Controls
- taqu/cpptoml

Preserve the current dependency policy.

Do not introduce:

- Qt
- SDL
- Boost
- MFC
- WebView2
- Electron
- Additional runtime services
- Third-party UI frameworks
- New controller input frameworks
- General-purpose configuration systems

Avoid changing the architecture unless a concrete defect requires it.

Do not redesign the application during release stabilization.

## 4. Release Scope

The target release is:

**ZeroPad v1.0.0**

The release should provide:

- XInput-compatible controller support.
- Configurable keyboard bindings.
- Digital button mapping.
- Analog trigger mapping.
- Analog stick directional mapping.
- Analog thresholds and hysteresis.
- Safe simultaneous keyboard input.
- Native Win32 settings.
- TOML configuration.
- System tray operation.
- Runtime mapping enable/disable.
- Controller disconnection and reconnection handling.
- Graceful shutdown.
- Windows 11 x64 support.

Do not advertise functionality that is not actually implemented and verified.

### Explicitly Excluded

Do not add:

- Mouse emulation.
- Macros.
- Turbo / rapid-fire.
- Multiple profiles.
- Automatic profile switching.
- DirectInput support.
- GameInput support.
- Generic HID support.
- Virtual gamepad drivers.
- Custom GUI themes.
- Gamepad diagrams.
- Automatic updating.
- Windows startup integration, unless already implemented and explicitly accepted.
- Cloud synchronization.
- An installer.

Phase 5 is not a feature expansion phase.

## 5. Repository Audit

Begin by inspecting the current repository.

Review:

- Project structure.
- CMake configuration.
- Application entry point.
- Input engine.
- Keyboard injection.
- Configuration management.
- Native GUI.
- System tray.
- Threading.
- Shutdown handling.
- Tests.
- Documentation.
- Third-party dependencies.
- Existing license files.

Identify:

1. Known defects.
2. Unhandled failure paths.
3. Resource lifetime problems.
4. Potential threading races.
5. Excessive allocations.
6. Unnecessary dependencies.
7. Missing tests.
8. Incorrect or outdated documentation.
9. Release-blocking issues.

Create a concise prioritized issue list.

Classify findings as:

- **Critical:** Unsafe behavior, severe resource leaks, or failures that prevent basic operation.
- **High:** Common workflows fail, input state becomes inconsistent, or settings can be lost.
- **Medium:** Recoverable defects, unclear user feedback, or significant performance regressions.
- **Low:** Cosmetic issues, minor documentation improvements, or nonessential optimizations.

Fix Critical and High issues before release.

Do not perform unrelated refactoring merely to improve aesthetics or code organization.

## 6. Keyboard Input Safety Review

Keyboard state management is the highest-priority reliability requirement.

Review the full processing pipeline:

```text
XInput
   |
   v
Logical Controller State
   |
   v
Mapping Engine
   |
   v
Desired Keyboard State
   |
   v
SendInput Reconciliation
   |
   v
Windows Keyboard Input
```

### Required Guarantees

Under normal operating conditions:

- No duplicate key-down events for unchanged states.
- No premature release of shared keyboard mappings.
- No stale controller states after disconnection.
- No stale controller states after system resume.
- No new key-down events after shutdown begins.
- No new mapping-generated input while mapping is disabled.
- No unreconciled mapping changes silently treated as successful.
- No configuration reload that bypasses the existing safety mechanism.

### Required Scenarios

Verify:

1. Multiple buttons pressed simultaneously.
2. Multiple inputs assigned to the same keyboard key.
3. Rapid button press/release.
4. Trigger movement around threshold boundaries.
5. Analog stick diagonal movement.
6. Rapid opposite-direction changes.
7. Controller disconnect while holding several inputs.
8. Controller reconnect after disconnection.
9. Mapping disabled while holding inputs.
10. Mapping re-enabled after suspension.
11. Settings saved while a button is held.
12. Keyboard capture begins while mapping is active.
13. Sleep and resume while inputs are held.
14. Application exits while keys are held.
15. SendInput fails during key-down.
16. SendInput fails during key-up.

### Important Limitation

Do not promise guaranteed key release after:

- Forced process termination.
- Process crashes.
- Sudden power loss.
- Windows refusing injected keyboard input.
- External applications ignoring or intercepting events.

Implement and document best-effort recovery.

Do not claim that successful SendInput submission guarantees an application consumed the event.

## 7. Automated Regression Testing

Run every existing test from Phases 0–4.

Organize tests logically.

Recommended categories:

- Controller input decoding.
- Analog threshold processing.
- Input mapping.
- Keyboard state reconciliation.
- Configuration parsing.
- Configuration validation.
- Configuration serialization.
- Configuration persistence.
- Configuration reload.
- GUI-independent editing logic.
- Keyboard capture logic.
- Application lifecycle.
- System tray commands.
- Single-instance handling.
- Power and disconnection recovery.

### Additional Tests

Add tests for uncovered release-critical behavior.

Prioritize:

- Boundary conditions.
- Error handling.
- Resource cleanup.
- State transition ordering.
- Configuration corruption.
- Repeated start/stop cycles.
- Failure recovery.
- Concurrent shutdown and configuration changes.

### Testing Requirements

- Use CTest.
- Preserve existing test behavior.
- Avoid physical controller dependencies in unit tests.
- Avoid real keyboard injection in unit tests.
- Use deterministic synthetic controller snapshots.
- Use mock keyboard output where appropriate.
- Avoid arbitrary timing assumptions.
- Avoid flaky tests.
- Do not weaken assertions merely to make tests pass.

### Test Reporting

Report:

- Number of tests executed.
- Number passed.
- Number failed.
- Tests skipped.
- Tests not executable in the current environment.
- Any observed nondeterministic failures.

Do not claim all tests passed unless they actually ran successfully.

## 8. Windows 11 Integration Testing

Perform integration testing on Windows 11 x64 where possible.

Use at least one real XInput-compatible controller if hardware is available.

### Controller Tests

Verify:

- Controller detection.
- All supported digital buttons.
- Both analog triggers.
- Both analog sticks.
- Simultaneous input.
- Diagonal movement.
- Hysteresis.
- Duplicate keyboard mappings.
- Controller disconnection.
- Controller reconnection.

### GUI Tests

Verify:

- Settings window opens.
- All bindings are displayed.
- Key capture works.
- Alternative key selection works.
- Unassigned bindings work.
- Save persists configuration.
- Cancel discards changes.
- Defaults restores editable values.
- Invalid settings are rejected.
- DPI scaling is usable.
- Keyboard navigation works.

### Tray Tests

Verify:

- Tray icon appears.
- Settings can be reopened.
- Closing Settings does not stop mapping.
- Disable releases held keys.
- Enable resumes mapping.
- Only one functional instance runs.
- Second launch reopens Settings.
- Explorer restart restores the tray icon.
- Exit terminates the process.

### Lifecycle Tests

Verify:

- Normal startup.
- Graceful shutdown.
- Shutdown with held keys.
- Sleep and resume.
- Repeated controller connection changes.
- Configuration reload while running.
- Repeated settings window opening and closing.
- Recovery after transient failures.

Do not report manual tests as passed when only unit tests were executed.

Clearly distinguish hardware-tested functionality from code-reviewed behavior.

## 9. Long-Duration Stability Testing

Evaluate ordinary extended use.

Recommended scenarios:

1. Idle with a connected controller.
2. Idle without a connected controller.
3. Repeated controller input.
4. Repeated disconnection and reconnection.
5. Repeated settings window open/close.
6. Repeated enable/disable transitions.

A practical initial target is a continuous multi-hour run, preferably including an overnight idle test.

Record actual duration.

Do not invent measurements.

Monitor:

- Process memory.
- Handle count.
- Thread count.
- CPU usage.
- Unexpected process termination.
- Error logging.
- Keyboard state inconsistencies.

### Stability Criteria

Investigate:

- Sustained memory growth.
- Persistent handle growth.
- Unexpected worker threads.
- Increasing CPU usage over time.
- Repeated failure messages.
- Stuck keyboard states.
- Input processing stalls.

Do not assume every observed variation is a memory leak.

Separate ordinary allocator or operating-system behavior from reproducible resource leaks.

## 10. Performance Evaluation

ZeroPad must remain lightweight.

Measure performance using a Release build.

### Required Metrics

| Metric | Measurement |
|---|---|
| Executable size | Final ZeroPad.exe file size |
| Working set | Process working-set memory |
| Private memory | Private committed bytes |
| CPU usage | Idle and active |
| Handle count | Startup and extended operation |
| Thread count | Normal background operation |
| Polling interval | Measured interval distribution |
| Input processing | Sample-to-SendInput processing time |

### Target Values

Aspirational targets:

- Executable size: approximately 1 MB or less.
- Resident memory: approximately 10 MB or less.
- Default polling interval: 4 ms.
- Minimal idle CPU usage.

These are not absolute release-blocking thresholds unless the existing project defines stricter requirements.

Correctness is more important than achieving arbitrary size targets.

### Measurement Conditions

Record:

- Windows version.
- CPU model.
- Compiler version.
- Build configuration.
- Controller connection state.
- Test duration.
- Measurement method.

### Latency Reporting

Distinguish:

1. Controller sampling interval.
2. Input processing duration.
3. Time until SendInput submission.
4. Actual target-application response latency.

Do not claim end-to-end latency measurements without appropriate instrumentation.

### Performance Optimizations

Only optimize when measurements reveal meaningful problems.

Prefer:

- Eliminating unnecessary polling work.
- Avoiding allocations in the input hot path.
- Reducing redundant GUI updates.
- Removing repeated file I/O.
- Avoiding excessive diagnostics.

Do not introduce busy waiting or aggressive timer changes merely to improve benchmark numbers.

## 11. Compiler Warnings and Static Analysis

Review compiler warnings.

Use reasonable MSVC warning settings.

Recommended:

- `/W4`
- `/permissive-`
- C++20 mode.

Do not automatically enable `/WX` for all builds if existing dependencies generate warnings that cannot reasonably be controlled.

Keep warnings in ZeroPad-owned source code clean.

### Static Analysis

Use available tools such as:

- MSVC static analysis.
- CMake-compatible analysis tooling where already available.
- Windows SDK diagnostics.

Prefer existing toolchain capabilities.

Do not introduce a large new analysis infrastructure.

### Review Focus

Check:

- Integer conversions.
- Signed/unsigned comparisons.
- Array bounds.
- Windows handle ownership.
- Memory lifetime.
- Thread synchronization.
- Return-value checking.
- UTF-8 and UTF-16 handling.
- File I/O errors.
- Configuration validation.
- Resource cleanup.

Do not suppress warnings without understanding them.

## 12. Dependency and Runtime Audit

Verify that ZeroPad can run on a clean, supported Windows 11 x64 installation.

### Requirements

- No Qt runtime.
- No SDL runtime.
- No .NET runtime.
- No browser runtime.
- No third-party installer.
- No required external configuration files on first launch.
- No requirement to install a package manager.

Inspect the final executable's imported DLLs.

Use appropriate tools such as:

- `dumpbin /dependents`
- Windows SDK utilities.
- Visual Studio dependency tools.

Distinguish Windows system DLLs from application-specific redistributables.

### MSVC Runtime

Evaluate whether a statically linked MSVC runtime is appropriate.

Using `/MT` for Release builds is acceptable if compatible with the project's dependencies and licenses.

Do not assume static runtime linking is automatically the best choice.

Document any remaining runtime requirements.

### Single Executable Goal

The intended user-facing distribution should contain one main executable.

A README and license files may accompany it in the ZIP archive.

Do not introduce auxiliary executables or DLLs without a concrete need.

## 13. CMake Release Configuration

Review and stabilize CMake.

Required:

- C++20.
- MSVC compatibility.
- Windows 11 x64.
- Debug and Release builds.
- CTest integration.
- Correct Windows resources.
- Correct application manifest.
- Correct subsystem configuration.
- Reproducible dependency resolution.

### Build Commands

The following commands must work on a suitable Windows build machine:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Do not introduce unnecessary build options.

Do not require manual source-file modifications before building.

### Version Information

Add or verify Windows executable version metadata.

Target:

- Product name: ZeroPad
- Product version: 1.0.0
- File version: 1.0.0.0
- Architecture: x64

Use consistent version information in:

- CMake.
- Windows version resources.
- README.
- Release filenames.
- Release notes.

Avoid maintaining multiple contradictory hardcoded version strings.

Do not invent a publisher identity or copyright holder.

Use repository-established metadata, or explicitly flag missing metadata for maintainer review.

## 14. GitHub Actions CI

Add a minimal GitHub Actions workflow.

Suggested path:

`.github/workflows/build.yml`

### Triggers

Run CI on:

- Pull requests.
- Pushes to the repository's primary development branch.
- Manual workflow dispatch where useful.

Do not assume the default branch is named `main`.

Inspect the repository before configuring branch filters.

### Windows Build Job

Use an appropriate GitHub-hosted Windows runner with MSVC and CMake.

Perform:

1. Checkout source.
2. Checkout required submodules.
3. Configure CMake.
4. Build Release.
5. Run CTest.
6. Collect test results.
7. Upload build artifacts when useful.

### CI Requirements

- Keep the workflow small.
- Use maintained official actions where practical.
- Pin third-party actions appropriately.
- Use least-privilege permissions.
- Do not expose secrets to untrusted pull requests.
- Do not require a physical controller.
- Do not run interactive GUI tests that require an unattended desktop session.
- Do not automatically publish GitHub Releases from normal CI builds.

### Failure Behavior

CI must fail when:

- CMake configuration fails.
- Compilation fails.
- Required automated tests fail.
- Packaging commands fail.

Do not suppress errors merely to obtain a green build.

### Artifact Retention

Use a reasonable artifact retention period.

Do not accumulate unnecessary large build artifacts.

## 15. Portable ZIP Distribution

Create a simple portable distribution.

Suggested filename:

`ZeroPad-v1.0.0-win-x64.zip`

Suggested archive contents:

```text
ZeroPad/
├── ZeroPad.exe
├── README.md
├── LICENSE
└── THIRD_PARTY_NOTICES.txt
```

Include files only when applicable.

### Requirements

- No installer.
- No required administrative privileges for ordinary use.
- No registry setup required for basic operation.
- No custom driver installation.
- No manual configuration file creation.
- No auxiliary setup application.

The user should be able to:

1. Download the ZIP.
2. Extract the ZIP.
3. Run ZeroPad.exe.
4. Configure keyboard mappings through the GUI.
5. Close Settings and continue using ZeroPad from the tray.

### Packaging

Use CMake install rules, CPack, or a small packaging script if appropriate.

Prefer the simplest reliable approach.

Do not introduce a full packaging framework.

### Verification

Extract the final archive into a clean temporary directory.

Run the extracted executable.

Verify:

- Startup.
- First-run configuration creation.
- Settings window.
- System tray.
- Keyboard mapping.
- Graceful exit.

The packaged binary must be the same tested Release build intended for distribution.

## 16. Release Integrity

Generate a SHA-256 checksum for the final ZIP archive.

Suggested filename:

`ZeroPad-v1.0.0-win-x64.zip.sha256`

Use a standard hashing tool.

Document how users can verify the archive.

### Requirements

- Hash the final release artifact.
- Do not modify the ZIP after generating its checksum.
- Clearly identify the exact artifact covered by the checksum.
- Do not present a checksum as a digital signature or proof of publisher identity.

If the release process is repeated, regenerate checksums from the final artifacts.

## 17. Windows Security and Code Signing

ZeroPad uses SendInput to generate keyboard events.

This functionality may attract additional security scrutiny.

### Requirements

- Do not request administrator privileges by default.
- Do not disable Windows security features.
- Do not attempt to bypass SmartScreen.
- Do not modify Defender settings.
- Do not add antivirus exclusions.
- Do not include privilege escalation.
- Do not inject code into other processes.
- Do not install keyboard hooks unnecessarily.
- Do not conceal the running process.

### Code Signing

Code signing is desirable but not mandatory for an initial open-source release.

If no trusted signing certificate is available:

- Produce an unsigned executable.
- Document the unsigned status.
- Explain that SmartScreen warnings may occur.
- Do not claim that Windows will always permit execution.
- Do not ask users to disable security protections.

If signing infrastructure already exists:

- Verify signatures.
- Ensure the signing process is secure.
- Do not commit private keys or signing secrets.
- Do not modify binaries after signing.

Do not purchase certificates or enroll in signing services without explicit maintainer approval.

### SmartScreen Limitations

Newly distributed Windows executables may have limited reputation.

Even signed executables can receive warnings until sufficient reputation has developed.

Do not promise warning-free execution.

## 18. OSS License Compliance

Review repository licensing before publication.

### Main Project License

Inspect whether ZeroPad already has a selected license.

If a valid license is already present:

- Preserve it.
- Verify its presence in the repository and release archive.
- Ensure documentation matches it.

If no license has been selected:

- Do not silently choose one.
- Recommend a suitable permissive OSS license, such as MIT or BSD-2-Clause, for maintainer consideration.
- Report that explicit license selection is required before public stable release.

Do not invent copyright ownership.

### Third-Party Dependencies

Review all bundled third-party code.

Pay particular attention to:

https://github.com/taqu/cpptoml

Verify:

- Actual upstream license.
- Copyright notices.
- Attribution requirements.
- Redistribution conditions.
- Any bundled third-party components.

Do not assume the dependency license based on its name or repository metadata alone.

### Notices

Create `THIRD_PARTY_NOTICES.txt` if needed.

Include required notices without removing upstream copyright statements.

Do not silently alter upstream license text.

### Compliance Report

Summarize:

- ZeroPad's own license.
- Each third-party dependency.
- Applicable license.
- Required notice.
- Whether the release archive satisfies the requirements.

Flag uncertainties for maintainer review rather than guessing.

## 19. README for General Users

Rewrite or improve README so nontechnical Windows users can understand ZeroPad.

Avoid excessive implementation details in the main introduction.

Recommended structure:

### What is ZeroPad?

Explain briefly that ZeroPad converts game controller inputs into keyboard inputs.

### Features

Describe only implemented features.

### Requirements

Specify:

- Windows 11 x64.
- XInput-compatible controller.

Explain that other controllers may not work.

### Download and Run

Provide simple portable ZIP instructions.

### Basic Usage

Explain:

1. Start ZeroPad.
2. Select a controller.
3. Choose keyboard bindings.
4. Save configuration.
5. Close Settings.
6. Use the tray icon to reopen Settings or exit.

### Settings

Explain:

- Controller selection.
- Key assignment.
- Clearing bindings.
- Analog thresholds.
- Save.
- Cancel.
- Defaults.

### Configuration Location

Document:

`%LOCALAPPDATA%\ZeroPad\config.toml`

### Troubleshooting

Cover:

- Controller not detected.
- Keyboard input not working in a particular application.
- Mapping disabled.
- Application running in the tray.
- Configuration file errors.
- SmartScreen warnings.
- How to exit the application.

### Known Limitations

Explain:

- XInput-only controller support.
- Windows input integrity restrictions.
- Certain games may reject simulated keyboard input.
- No virtual controller emulation.
- No mouse emulation.
- No macro support.

### Building from Source

Provide concise CMake and MSVC instructions.

### License

Document the selected license once confirmed.

Keep README practical and readable.

Do not include elaborate gamepad diagrams or promotional graphics.

## 20. Additional Documentation

Create only documentation that materially improves usability or maintainability.

Useful files may include:

```text
README.md
LICENSE
THIRD_PARTY_NOTICES.txt
CHANGELOG.md
docs/
    building.md
    troubleshooting.md
```

Do not create redundant documentation files.

Avoid maintaining the same instructions in multiple places.

### Developer Documentation

Document only non-obvious architectural decisions, such as:

- Keyboard state reconciliation.
- Configuration save/apply ordering.
- Engine shutdown.
- Thread synchronization.
- Tray lifecycle.
- Error recovery.

Do not create a large architecture manual for a small utility.

## 21. Release Notes

Prepare release notes for v1.0.0.

Suggested structure:

```text
ZeroPad v1.0.0

Initial stable release.

Features
- XInput-compatible controller mapping
- Configurable keyboard bindings
- Analog stick and trigger support
- Native Windows settings interface
- TOML configuration
- System tray background operation

Requirements
- Windows 11 x64
- XInput-compatible controller

Known Limitations
- No DirectInput or generic HID support
- No mouse emulation
- No virtual controller output
- Some applications may reject SendInput events

Distribution
- Portable ZIP
- No installer
```

Adapt these notes to the actual verified implementation.

Do not claim functionality that failed testing.

## 22. Release Workflow

Prepare a controlled release procedure.

Recommended process:

1. Complete code review.
2. Run all automated tests.
3. Complete available manual integration tests.
4. Confirm performance measurements.
5. Resolve release-blocking defects.
6. Verify license compliance.
7. Confirm version metadata.
8. Produce the Release executable.
9. Package the portable ZIP.
10. Verify the extracted archive.
11. Generate SHA-256 checksums.
12. Prepare release notes.
13. Prepare an optional draft GitHub Release.
14. Produce a final release-readiness report.
15. Obtain explicit maintainer authorization before publishing.

### Important Authorization Boundary

Do not automatically:

- Push commits.
- Push release tags.
- Publish a GitHub Release.
- Upload a public binary.
- Change repository visibility.
- Modify repository permissions.
- Purchase signing services.
- Publish packages to registries.

unless explicitly authorized by the maintainer.

Preparing local release artifacts is permitted.

If GitHub access and authorization are available, prepare a draft release only when that external operation is permitted.

Publishing the final release requires separate explicit approval.

## 23. Release Readiness Gates

Use the following release gates.

### Gate A — Build

Required:

- Clean Windows 11 x64 Release build.
- No unresolved build failures.
- Correct executable version.
- Correct application resources.
- Verified runtime dependencies.

### Gate B — Automated Tests

Required:

- All Phase 0 tests pass.
- All Phase 1 tests pass.
- All Phase 2 tests pass.
- All Phase 3 tests pass.
- All Phase 4 tests pass.
- All Phase 5 tests pass.

### Gate C — Input Safety

Required:

- No known reproducible stuck-key defects under ordinary recoverable scenarios.
- Correct duplicate mapping behavior.
- Safe configuration changes.
- Safe disable and shutdown.
- Reliable disconnection handling.

### Gate D — User Experience

Required:

- Settings GUI is usable.
- No TOML editing is necessary for ordinary operation.
- Background operation works.
- Tray Exit is discoverable.
- Error messages are understandable.
- No release-blocking GUI defects.

### Gate E — Distribution

Required:

- Portable ZIP created.
- Archive extraction verified.
- Runtime dependencies audited.
- SHA-256 checksum generated.
- First-run behavior checked.

### Gate F — Compliance

Required:

- Main project license confirmed.
- Third-party licenses reviewed.
- Notices included where required.
- No unauthorized code signing claims.

### Gate G — Documentation

Required:

- README matches actual functionality.
- Requirements are documented.
- Installation-free usage is documented.
- Troubleshooting instructions exist.
- Known limitations are disclosed.
- Release notes are prepared.

### Gate H — Maintainer Approval

Required before public release:

- Maintainer reviews release report.
- Remaining risks are understood.
- Release assets are approved.
- Explicit publication authorization is given.

Do not treat incomplete gates as passed.

## 24. Release Readiness Report

Produce a concise report.

Suggested format:

```text
ZeroPad v1.0.0
Release Readiness Report

Build:
PASS / FAIL / NOT VERIFIED

Automated Tests:
PASS / FAIL / NOT VERIFIED

Manual Controller Tests:
PASS / FAIL / NOT VERIFIED

GUI and Tray:
PASS / FAIL / NOT VERIFIED

Stability:
PASS / FAIL / NOT VERIFIED

Performance:
MEASURED / NOT VERIFIED

Dependency Audit:
PASS / FAIL / NOT VERIFIED

License Compliance:
PASS / BLOCKED / NOT VERIFIED

Portable ZIP:
PASS / FAIL / NOT VERIFIED

Documentation:
PASS / FAIL / NOT VERIFIED

Outstanding Critical Issues:
...

Outstanding High Issues:
...

Known Limitations:
...

Final Recommendation:
READY / NOT READY / CONDITIONAL
```

Use `READY` only when all mandatory gates have actually been satisfied.

Use `CONDITIONAL` when a clearly identified manual or external dependency remains.

Do not interpret missing evidence as success.

## 25. CI and Release Artifact Security

Treat CI workflows as part of the software supply chain.

Requirements:

- Use least-privilege GitHub token permissions.
- Avoid granting write permissions to ordinary PR builds.
- Do not execute untrusted PR code with access to release secrets.
- Review third-party GitHub Actions.
- Prefer maintained official actions.
- Pin sensitive third-party actions to immutable commit hashes where appropriate.
- Do not expose signing credentials.
- Do not upload private build environment data.
- Do not automatically trust downloaded binaries.

### Release Artifacts

Build release assets from a known source revision.

Record:

- Commit hash.
- Version.
- Build configuration.
- Compiler version.
- Artifact checksum.

Ensure the published artifact corresponds to the reviewed and tested revision.

Do not rebuild an unreviewed revision between testing and release.

## 26. Suggested Repository Structure

Preserve the existing structure where possible.

Suggested final organization:

```text
ZeroPad/
├── .github/
│   └── workflows/
│       └── build.yml
├── CMakeLists.txt
├── LICENSE
├── README.md
├── CHANGELOG.md
├── THIRD_PARTY_NOTICES.txt
├── include/
│   └── zeropad/
├── src/
├── resources/
├── third_party/
│   └── cpptoml/
├── tests/
└── docs/
    ├── building.md
    └── troubleshooting.md
```

Only create files that are needed.

Do not reorganize working code solely to match this layout.

## 27. Implementation Workflow

Follow this sequence.

### Step 1 — Inspect Repository

Review all source files, tests, CMake configuration, documentation, and dependencies.

Identify release blockers.

### Step 2 — Fix Critical Defects

Prioritize keyboard state safety, configuration integrity, application lifecycle, and thread correctness.

Avoid unrelated refactoring.

### Step 3 — Run Regression Tests

Run the complete CTest suite.

Fix failures.

Add missing release-critical tests.

### Step 4 — Review Windows Integration

Verify available controller, GUI, tray, and power recovery scenarios.

Record what was actually tested.

### Step 5 — Perform Stability Evaluation

Measure resource usage and test repeated lifecycle transitions.

Investigate reproducible leaks and failures.

### Step 6 — Verify Release Build

Clean and rebuild using MSVC and CMake.

Inspect binary dependencies.

Review compiler warnings.

### Step 7 — Implement CI

Create the GitHub Actions workflow.

Verify build and test execution if GitHub CI is available.

Otherwise, validate the workflow as far as the local environment permits and report the remaining uncertainty.

### Step 8 — Review Licensing

Inspect the main project license and cpptoml license.

Prepare required attribution files.

Flag decisions that require maintainer approval.

### Step 9 — Improve Documentation

Update README, build instructions, troubleshooting, and known limitations.

### Step 10 — Package ZeroPad

Create the Windows 11 x64 portable ZIP.

Test the extracted package.

Generate checksum files.

### Step 11 — Prepare Release Notes

Create accurate v1.0.0 release notes.

### Step 12 — Final Verification

Run the complete test suite against the final release candidate.

Ensure the binary and ZIP originate from the verified source revision.

### Step 13 — Produce Readiness Report

Summarize:

- Implemented changes.
- Test results.
- Manual verification.
- Performance.
- Dependencies.
- License status.
- Release artifacts.
- Remaining risks.

### Step 14 — Stop Before Publication

Do not publish the release without explicit maintainer authorization.

Do not proceed to post-v1.0 development automatically.

## 28. Definition of Done

Phase 5 is complete when:

- [ ] Existing source code has been reviewed.
- [ ] No unresolved Critical release blockers remain.
- [ ] No unresolved High release blockers remain.
- [ ] Keyboard state management has been reviewed.
- [ ] Automated regression tests pass.
- [ ] Available integration tests have been completed and reported.
- [ ] Long-duration stability testing has been performed or explicitly identified as pending.
- [ ] CPU and memory measurements have been recorded where possible.
- [ ] Executable size has been measured.
- [ ] Runtime dependencies have been audited.
- [ ] Release CMake build succeeds.
- [ ] GitHub Actions CI configuration is implemented.
- [ ] CI execution has been verified or explicitly marked pending.
- [ ] Executable version information is correct.
- [ ] Portable ZIP packaging works.
- [ ] Packaged executable matches the intended release candidate.
- [ ] SHA-256 checksum is generated.
- [ ] Main OSS license is confirmed.
- [ ] Third-party license compliance is reviewed.
- [ ] README is updated.
- [ ] Troubleshooting documentation is available.
- [ ] Known limitations are documented.
- [ ] v1.0.0 release notes are prepared.
- [ ] Final release-readiness report is produced.
- [ ] No unrelated new features have been introduced.

Public release publication is a separate maintainer-authorized action.

Do not mark missing hardware tests, missing legal decisions, or unexecuted CI jobs as completed.

## 29. Final Deliverables

Provide:

1. Stabilized C++ source code.
2. Updated CMake configuration.
3. Expanded automated tests.
4. GitHub Actions CI workflow.
5. Updated README.
6. Additional necessary documentation.
7. License and third-party notices.
8. Portable release ZIP, if the build environment permits.
9. SHA-256 checksum.
10. v1.0.0 release notes.
11. Performance results.
12. Dependency audit results.
13. Test results.
14. Release-readiness report.
15. List of unresolved issues.

Clearly identify unavailable deliverables and the reason they could not be completed.

Do not fabricate build artifacts or verification results.

## 30. Final Engineering Principles

**Stability over features.**

Phase 5 is about making existing functionality trustworthy.

**Correctness over micro-optimization.**

Never weaken input safety to reduce binary size or CPU usage.

**Minimal dependencies.**

Do not introduce frameworks merely to simplify packaging or tests.

**Native Windows behavior.**

Use standard Windows mechanisms wherever practical.

**Simple distribution.**

A portable ZIP should be sufficient for ordinary use.

**Transparent limitations.**

Do not overstate controller compatibility, input reliability, security, or test coverage.

**Reproducible releases.**

A release must be traceable to a known source revision and build configuration.

**No automatic public publication.**

Prepare release artifacts, report readiness, and obtain explicit maintainer approval before publication.

**No feature creep.**

Do not add functionality planned for later versions.

**Stop at Phase 5.**

Complete release stabilization and preparation, then stop.
