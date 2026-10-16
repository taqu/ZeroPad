# ZeroPad

ZeroPad is an early stage, lightweight Windows gamepad to keyboard adapter. Phase 0 reads the A button from XInput controller 0 and holds Space through `SendInput` while A is held.

## Platform and requirements

- Windows 11 x64
- Visual Studio 2022 or newer with the C++ desktop tools and Windows SDK
- CMake 3.20 or newer

## Build

From a Visual Studio developer PowerShell or a shell with CMake and MSVC available:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The mapper tests are portable and can also be configured with another C++20 compiler. The application target is created on Windows only.

## Run and check the mapping

Run `build\Release\zeropad.exe`, connect an XInput-compatible controller, and press and release A. Space should be pressed and released correspondingly. Press Ctrl+C to stop; ZeroPad attempts to release Space before exiting.

## Limitations

Phase 0 supports only controller index 0, the A button, and Space. It has no GUI or configuration. Windows integrity restrictions and some games may reject `SendInput`; administrator privileges are not normally required. A forced process termination or crash cannot run the normal key-release cleanup.

## Future phases

Later planned work includes additional input mappings, configuration, a native settings window, and optional tray operation. Those features are not part of this Phase 0 implementation.
