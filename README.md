# ZeroPad

ZeroPad maps one XInput-compatible controller to keyboard keys on Windows 11 x64. Native settings configure buttons, triggers and stick directions with analog hysteresis; the notification-area icon keeps it accessible in the background.

**v1.0.0 is an unpublished release candidate.** No public download is advertised until acceptance is complete.

## Portable use

1. Extract the entire `ZeroPad-v1.0.0-win-x64.zip`. Verify SHA-256 against a trusted distribution source.
2. Run `ZeroPad/ZeroPad.exe`. No installer, administrator privileges or separate VC++ runtime installation is intended. The executable is unsigned; Windows may display publisher/SmartScreen warnings.
3. Select controller 1–4, edit bindings and Save. A maps to Space by default. Configuration lives at `%LOCALAPPDATA%\ZeroPad\config.toml`; installation-free distribution does not imply portable configuration.
4. Close Settings to keep mapping active. Double-click the tray icon to reopen it. Disable Mapping pauses and releases keys; Exit stops ZeroPad. A second launch reopens the existing editor.

Save persists edits; Cancel discards them; Defaults changes the editor until Save. Key capture temporarily suspends mapping. Tray enable/disable lasts for the session; Save/Reload applies the saved enabled preference. See [settings and configuration](docs/configuration.md) and [troubleshooting](docs/troubleshooting.md).

## Compatibility and limits

One XInput controller/profile at a time. No mouse, macros, chords, drivers, startup registration, installer or updater. Windows and protected applications may reject injection. Successful submission does not guarantee application consumption. Forced termination, crashes, power loss or refused input can prevent key cleanup.

Actual polling depends on Windows scheduling: requested 4 ms does not guarantee 4 ms latency. Physical-input acceptance and controller-to-application latency remain unverified. No telemetry or continuous disk logging is used.

## Build

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Requires Visual Studio C++ tools, Windows SDK, CMake and PowerShell. See source-tree [build/test/package instructions](docs/building.md) and [release readiness](docs/release/v1.0.0-readiness.md). Dependencies are vendored. Native GUI tests are opt-in with mock input/output.

## License

[MIT](LICENSE), Copyright (c) 2026 taqu. Unmodified taqu/cpptoml is pinned to `6f064ef6f030713d5ba88eb58f537a3d5aa125f8`. [Third-party notices](THIRD_PARTY_NOTICES.txt) include full terms and source-only spdlog/fmt notices.
