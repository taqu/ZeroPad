# ZeroPad — Automated GitHub Release on Version Tag Push

## 1. Objective

Implement a GitHub Actions workflow that automatically builds, tests, packages, and publishes a ZeroPad release whenever a valid version tag is pushed to GitHub.

ZeroPad is an ultra-lightweight Windows game controller-to-keyboard mapper written in C++20.

The project uses:

- CMake
- MSVC
- Windows 11 x64
- Native Win32 APIs
- XInput
- SendInput
- taqu/cpptoml

The release process must remain simple, reliable, and easy to maintain.

**Pushing a valid release tag must trigger the complete release process without additional manual steps.**

The workflow must never publish a broken or untested build.

## 2. Expected Release Workflow

The intended process is:

```text
Developer
    |
    | git tag v1.0.0
    | git push origin v1.0.0
    v
GitHub Actions
    |
    +-- Validate tag and version
    |
    +-- Checkout tagged commit
    |
    +-- Initialize submodules
    |
    +-- Configure CMake
    |
    +-- Build Windows x64 Release
    |
    +-- Run CTest
    |
    +-- Package portable ZIP
    |
    +-- Verify package contents
    |
    +-- Generate SHA-256
    |
    +-- Publish GitHub Release
    |
    v
GitHub Releases
    |
    +-- ZeroPad-v1.0.0-win-x64.zip
    +-- ZeroPad-v1.0.0-win-x64.zip.sha256
    +-- Release notes
```

A failed validation, build, test, or packaging step must prevent publication.

## 3. Repository Inspection

Before implementing the workflow, inspect the existing repository.

Identify:

- Existing GitHub Actions workflows.
- CMake configuration.
- Build targets.
- CTest configuration.
- Project version definitions.
- Executable output locations.
- Resource files.
- Runtime dependencies.
- Packaging scripts.
- Third-party dependencies and submodules.
- Existing license and attribution files.

Reuse existing infrastructure wherever possible.

Do not introduce redundant build scripts or competing release workflows.

If a release workflow already exists, modify it rather than creating a conflicting second workflow.

## 4. Tag Naming Convention

Use Semantic Versioning.

Stable release tags:

```text
v1.0.0
v1.0.1
v1.1.0
v2.0.0
```

Prerelease tags:

```text
v1.1.0-alpha.1
v1.1.0-beta.1
v1.1.0-rc.1
```

The release workflow should accept stable and prerelease SemVer tags.

Do not trigger releases for unrelated tags.

### GitHub Actions Trigger

Use a tag push trigger:

```yaml
on:
  push:
    tags:
      - 'v*'
```

Because GitHub Actions tag patterns are glob patterns rather than complete SemVer validators, validate the complete tag name in a dedicated step.

Reject malformed tags such as:

```text
v1
v1.0
v1.0.0-test
release-1.0.0
vabc
```

Accept only the explicitly supported SemVer format.

Do not silently reinterpret malformed version tags.

## 5. Tag and Version Validation

The release tag must match the actual project version.

For example:

```text
Git tag:       v1.2.3
CMake version: 1.2.3
```

These must match.

### Requirements

1. Extract the version from the tag.
2. Validate its syntax.
3. Compare it against the project's authoritative version definition.
4. Reject mismatches.
5. Ensure Windows executable version information remains consistent.
6. Never silently modify source files during the release build to force a match.

For prereleases, distinguish the SemVer prerelease suffix from the Windows numeric file version.

Windows numeric file versions do not directly represent arbitrary SemVer prerelease suffixes.

Do not require invalid Windows resource version strings.

If the existing CMake project uses only numeric version components, support prerelease suffixes through a separate project metadata value or another minimal mechanism.

Avoid maintaining multiple unrelated hardcoded version definitions.

## 6. GitHub Actions Workflow

Create or update:

`.github/workflows/release.yml`

Use a GitHub-hosted Windows runner.

Recommended:

`windows-2022`

Use a fixed supported runner image rather than automatically changing the build environment whenever `windows-latest` changes.

If the existing project explicitly requires a newer Windows runner, preserve that requirement.

### Workflow Permissions

Use least-privilege permissions.

The release publishing job requires:

```yaml
permissions:
  contents: write
```

Do not grant unnecessary permissions.

No personal access token should be necessary for a standard same-repository release.

Use the automatically provided `GITHUB_TOKEN`.

### Concurrency

Prevent conflicting releases for the same tag.

Use a concurrency group derived from the tag reference.

Do not cancel an in-progress release merely because another identical workflow run starts.

Prefer:

```yaml
concurrency:
  group: release-${{ github.ref }}
  cancel-in-progress: false
```

Avoid concurrent publication attempts for the same version.

## 7. Checkout

Use the standard GitHub checkout action.

Requirements:

- Checkout the exact tagged commit.
- Initialize required submodules.
- Do not build the latest default branch instead of the tagged commit.
- Do not modify repository source during checkout.
- Preserve the tag identity.

Example:

```yaml
- name: Checkout
  uses: actions/checkout@v4
  with:
    submodules: recursive
    persist-credentials: false
```

Use an actively supported action version and inspect existing repository conventions before selecting or updating versions.

Pin third-party actions appropriately.

The checked-out source revision must be the source of the published executable.

## 8. CMake Configuration

Configure the project for Windows x64.

Expected commands:

```powershell
cmake -S . -B build -A x64
```

Do not install a package manager unless the repository already requires one.

The project should use standard Windows SDK and MSVC facilities.

### Requirements

- C++20.
- MSVC.
- x64 architecture.
- Release configuration.
- Correct Windows resource compilation.
- Correct application manifest.
- Existing cpptoml integration.
- No unnecessary runtime dependencies.

Do not alter compiler flags solely to hide build failures.

## 9. Release Build

Build the project using:

```powershell
cmake --build build --config Release --parallel
```

The workflow must fail if compilation or linking fails.

### Build Output

Identify the actual ZeroPad executable from the existing build configuration.

Do not assume a hardcoded output directory without checking the repository.

Prefer CMake install rules or the existing packaging mechanism over fragile recursive searches for executables.

Do not accidentally package:

- Debug binaries.
- Test executables.
- Intermediate build artifacts.
- Object files.
- Compiler caches.

Only the intended Release executable should be distributed.

## 10. Automated Testing

Run all automated tests before publishing.

Use:

```powershell
ctest --test-dir build -C Release --output-on-failure
```

### Requirements

- Do not ignore failed tests.
- Do not use `continue-on-error`.
- Do not suppress CTest failures.
- Do not publish if any required test fails.
- Preserve Phase 0–5 regression tests.

Tests must run without a physical game controller.

Do not run tests requiring an interactive desktop unless the existing CI environment supports them reliably.

### Test Availability

If no tests are discovered, treat that as a release failure unless the repository explicitly defines and justifies a different policy.

Use CTest's no-tests error behavior where appropriate.

The workflow must not produce a successful release simply because zero tests were executed.

## 11. Portable ZIP Packaging

Produce a portable Windows x64 ZIP archive.

Filename format:

`ZeroPad-v<VERSION>-win-x64.zip`

Example:

`ZeroPad-v1.0.0-win-x64.zip`

Suggested contents:

```text
ZeroPad/
├── ZeroPad.exe
├── README.md
├── LICENSE
└── THIRD_PARTY_NOTICES.txt
```

Include only files applicable to the project and its license requirements.

### Packaging Requirements

- Package