# Phase 4 performance observations

Measured on 2026-10-17, Windows 11 kernel 10.0.26200.0, 20 logical processors,
MSVC 19.44 x64 Release, Windows SDK 10.0.26100.0. No system timer-resolution
changes. The desktop was in ordinary use; results are short local observations,
not a latency guarantee or a controlled benchmark.

## Method and results

`performance_probe 5` sampled each scenario for about five seconds. It uses the
real EngineService/Runtime with preallocated QPC interval samples and a mock
output callback. GetProcessMemoryInfo supplies working set/private committed
memory; GetProcessTimes supplies process CPU time. CPU percentages below are
relative to **one logical processor**, not the whole 20-processor machine.

Release ZeroPad.exe size: **286,208 bytes** at the probe measurement build;
**287,232 bytes** in the final verified build after lifecycle safety refinements.
Both are about 280 KiB. The interval/CPU observations below describe the earlier
measurement run; use the reproduction commands to measure another build.

| Probe scenario | Samples | Poll p50 / p95 / max (ms) | Working set | Private bytes | CPU, one core |
|---|---:|---|---:|---:|---:|
| Synthetic connected | 323 | 15.513 / 16.209 / 16.856 | 5,296,128 | 1,159,168 | 0 ticks measured |
| Synthetic disconnected | 20 | 263.261 / 264.981 / 265.013 | 5,365,760 | 1,159,168 | 0 ticks measured |
| Synthetic disabled, connected | 323 | 15.537 / 16.325 / 16.647 | 5,365,760 | 1,159,168 | 0 ticks measured |
| Real XInput, injection suppressed | 324 connected | 15.482 / 16.188 / 16.746 | 10,317,824 | 2,281,472 | 0.312% |

The real XInput pass found a connected controller at slot 0. It did not inject
keys, exercise physical buttons, or measure a target application. Its zero
output samples mean no mapped transitions were observed in that pass.

Synthetic sampled-state-to-mock-output p50/p95 was **0.0020/0.0086 ms**. This
includes engine coordination and the output gate after the poll callback; it
excludes waiting for the next poll, physical controller latency, SendInput
execution, and target consumption. It is not end-to-end input latency.

The configured 4 ms wait produced roughly 15.5 ms intervals here. The configured
250 ms disconnected wait produced roughly 263 ms intervals. Windows scheduling
and timer granularity matter; neither setting promises exact 250 Hz/4 Hz.

The native tray smoke harness also measured the full Win32 UI/tray code with
Settings closed and mock controller/output. Each interval was about three
seconds, including the existing worker and the UI's 500 ms status timer:

| Tray harness scenario | Working set | Private bytes | CPU, one core |
|---|---:|---:|---:|
| Synthetic connected, Settings closed | 25,894,912 | 3,825,664 | 0 ticks measured |
| Synthetic disconnected, Settings closed | 26,144,768 | 3,850,240 | 0 ticks measured |

The harness includes test code and an automation thread; it is not the production
process. The observed ~25 MiB working set exceeds the preliminary ~10 MiB goal;
private committed memory was about 3.7 MiB. Shared Win32/common-control mappings
contribute to working set. No correctness checks or native controls were removed
to pursue the preliminary memory target.

A reported zero means GetProcessTimes did not advance its CPU-time counter in
that short sample, **not zero CPU consumption**. Counter granularity and short
sample durations limit accuracy. Production desktop-safety checks, actual input
submission, and a real user's UI interactions can add work absent from these
probes. Production memory/CPU with real keyboard injection and true physical
controller-to-application delay remain unverified.

## Reproduce

```powershell
cmake -S . -B build -A x64 -DZEROPAD_PERFORMANCE_PROBE=ON
cmake --build build --config Release
.\build\Release\performance_probe.exe 5
```

The probe never calls SendInput and never reads/writes the user's configuration.
The final phase calls XInputGetState on slot 0 with output suppressed. Up to
60 seconds per scenario can be requested for a longer observation.

To measure an **already running production process**, use the read-only script:

```powershell
.\tools\measure-process.ps1 -ProcessId <ZeroPad-process-id> -Seconds 30
```

Run this while connected, disconnected, and disabled as separate observations.
The script does not launch ZeroPad, change timer settings, collect key content,
or write telemetry. The interactive CTest tray harness prints its memory/CPU
observations with `ctest --test-dir build -C Release -R tray_smoke_tests -V` after
configuring `ZEROPAD_GUI_SMOKE_TESTS=ON`.
