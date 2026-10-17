# Build, test, and package

Requires Windows 11 x64, Visual Studio 2022 C++ desktop tools and Windows SDK, CMake 3.20+, and PowerShell. Dependencies are vendored; builds require no downloads. MSVC uses static CRT and baseline x64 instructions. CMake version generates manifest/resource versions.

```powershell
cmake -S . -B build -A x64 -DZEROPAD_GUI_SMOKE_TESTS=ON -DZEROPAD_PERFORMANCE_PROBE=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
pwsh -NoProfile -File tools/package.ps1 -BuildDirectory build -OutputDirectory dist
```

GUI tests open temporary windows/icons on an interactive desktop with mock controller/output callbacks and temporary configurations. They never call SendInput. Leave GUI tests OFF in unattended CI. Native instance tests spawn a helper process. Performance probe output is suppressed; its processing measurements exclude SendInput and target consumption.

Owned targets use `/W4 /permissive-`. For MSVC analysis configure a separate directory with `"-DCMAKE_CXX_FLAGS=/EHsc /analyze"` and build ZeroPad Release. Preserve exception unwinding when replacing the default flags. Vendored cpptoml is unmodified and hash checked.

Packaging requires matching post-link source/executable hashes and passing Release noninteractive tests. It installs into a fresh staging directory, audits imports/version, creates the ZIP, verifies extraction, and emits SHA-256. BUILD_INFO records compiler, base Git commit, dirty state, and binary source hashes. Dirty snapshots are local candidates: commit reviewed changes, rebuild and repeat verification before public release. Ordered ZIP entries with fixed timestamps do not guarantee identical MSVC binaries across toolchains.

Branch/PR CI runs Release/Debug noninteractive tests with read-only repository permissions and pinned official actions, retaining candidate artifacts. The [tag release workflow](releasing.md) automatically publishes a matching stable/prerelease tag after validation, testing and packaging. Pushing a release tag authorizes publication; finish release acceptance first. Hosted execution needs separate verification.
