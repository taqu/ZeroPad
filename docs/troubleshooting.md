# Troubleshooting

Select the correct controller slot (1–4), check XInput support in Windows, and reconnect the controller. DirectInput and generic HID are unsupported. Check Enabled in the tray menu and the selected binding. If capture left mapping paused, release physical keyboard keys, reopen Settings, and choose Resume. Resume cannot override a power/session recovery pause.

Windows integrity boundaries, secure desktops, anti-cheat, and application filters may reject injected keyboard input. ZeroPad provides no bypass. Successful SendInput submission does not prove consumption. Virtual-key behavior can depend on layout and application support.

For a missing tray icon, check notification-area overflow. Launch ZeroPad again to reopen Settings. Closing Settings keeps mapping active; tray Exit stops it.

Back up `%LOCALAPPDATA%\ZeroPad\config.toml` before manual edits. See [configuration](configuration.md). Invalid files are preserved. Reload reads edits; Defaults changes only the editor until Save. Saves normalize formatting and remove comments. Persistence failures leave the previous active configuration usable.

The candidate is unsigned. Windows may display SmartScreen/publisher warnings. Verify SHA-256 against a trusted release source and decide whether to run it. Do not disable Windows security protections.

Graceful Exit attempts bounded key release. Crashes, forced termination, power loss, refused injection, or application interception can prevent cleanup. Recovery after termination is not guaranteed.

There is no installer, updater, startup registration, telemetry, or continuous disk log. Event diagnostics use OutputDebugString with an attached debugger. Report version, Windows build, controller model/slot, reproducible steps, and configuration with private data removed.
