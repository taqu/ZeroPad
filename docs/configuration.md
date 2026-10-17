# Settings and configuration

## Settings window

Choose Controller 1–4 (XInput indices 0–3). The Connected/Disconnected text updates every 250 ms for the **selected** controller. Selecting another slot does not change the running controller until Save.

The two-column list contains every supported controller input and its canonical keyboard key. Use arrow keys to select a row, then press Enter, double-click it, or choose **Change Key**. **Clear Binding** sets that editable row to `None`.

In the assignment dialog, the focused **Press a key here** control captures one physical key. Release it, then confirm with **Assign**. Enter, Tab, Space, and standalone left/right Shift, Control, and Alt are supported. Escape cancels the dialog; assign Escape using **Choose Key** or the key dropdown. Unsupported keys display an explanation. The dropdown uses the same canonical keys as TOML configuration and includes `None`. No chords or sequences are supported.

Mapping is temporarily suspended for capture. ZeroPad first releases all its held keys; a failed release prevents the dialog from opening. Assignment and cancellation affect only the editable copy. On closing the dialog, mapping resumes from fresh input after keyboard keys are released. If keys remain held for two seconds, mapping stays paused: release them and select **Resume**. No global keyboard hook is installed. Tagged ZeroPad events are ignored by the capture control; injected input from unrelated applications cannot always be identified.

Edit the four analog thresholds and polling interval using the ordinary text fields. Use a decimal point for stick values. Trigger values and polling interval must be integers. Validation errors keep your input intact and focus the relevant field where practical. Invalid values are never clamped.

- Stick thresholds: finite values with `0 <= release < press <= 1`.
- Trigger thresholds: integers with `0 <= release < press <= 255`.
- Polling interval: integer 1–100 ms; default 4 ms. Disconnected polling is 250 ms.
- **Enable keyboard mapping** edits the enabled setting. Saving `false` suspends controller conversion; re-enable and Save to resume and retain that preference for future launches.

**Defaults** loads the centralized defaults into the editor. It does not save or change live mappings. **Cancel** and the window close button discard unsaved edits and close only Settings. **Save** validates the entire candidate, safely releases held keys, persists the file, then activates the candidate before closing. While activation is finishing, Cancel/close waits for the result. An unchanged Save does not rewrite the file or generate release/repress events unless applying the saved enabled preference changes a session override.

**Reload** discards edits after confirmation and reads the configuration file. Changed settings use the same safe release-first transition. If the file or active revision changed externally before Save, ZeroPad refuses to overwrite it and offers Reload or cancellation. There is no automatic file watcher. Other applications can still race a change between the final file comparison and replacement; this is not a cross-process transaction lock.

If saving fails, the old engine configuration remains active and resumes safely; your edits remain open. If persistence succeeds but activation fails, the UI reports that distinction and keeps mapping paused. Retry Save, Reload the saved file, or explicitly Resume the old active mapping. Pending key releases retain their state and retry at most once per key per 100 ms. Saving is never reported as fully successful until activation is acknowledged.

Tab/Shift+Tab navigate controls, list arrow keys select rows, Enter edits a selected row, and ordinary button shortcuts work. The resizable window uses system fonts, standard controls, and a PerMonitorV2 manifest with DPI-aware layout. These provide basic accessibility support; comprehensive screen-reader behavior has not been certified.

## Configuration file

Settings are stored at `%LOCALAPPDATA%\ZeroPad\config.toml`, resolved through the Windows Known Folder API. On first launch, ZeroPad attempts to create the directory and save defaults. Persistence failure is reported while safe defaults remain usable. Invalid existing files are preserved during startup and reload; an explicit GUI Save asks before replacing an invalid file.

Saving writes and flushes a unique same-directory temporary file, closes it, then replaces the destination using Windows file APIs. It never deletes the original before replacement. This reduces partial-write risk without guaranteeing power-loss durability. Saves normalize formatting and remove comments; startup and Reload do not rewrite files.

The full default schema remains:

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

All 24 controller input identifiers appear above. Missing optional fields use these defaults; `version = 1` is mandatory. Unknown fields, duplicate entries, wrong types, and unsupported versions are rejected.

Supported key names are case sensitive: `A`–`Z`, `0`–`9`, `F1`–`F12`, `Space`, `Enter`, `Escape`, `Tab`, `Backspace`, `Up`, `Down`, `Left`, `Right`, `Insert`, `Delete`, `Home`, `End`, `PageUp`, `PageDown`, `LShift`, `RShift`, `LCtrl`, `RCtrl`, `LAlt`, `RAlt`, and `None`. A numeric string such as `"7"` means that keyboard key; `"32"` is invalid. Multiple inputs may share one key without premature releases.

Press occurs at or above the press threshold; release occurs at or below the release threshold. Values between preserve the previous state. Stick axes normalize positive values by 32767 and negative values by 32768. Axis-aligned thresholds permit diagonals and exclude opposite directions on an axis.

The unchanged [taqu/cpptoml](https://github.com/taqu/cpptoml) reader is pinned to revision `6f064ef6f030713d5ba88eb58f537a3d5aa125f8`, with hashes verified by CMake. See [provenance and license](../third_party/cpptoml/PROVENANCE.md). Configuration is limited to 64 KiB, valid UTF-8 without NUL/control bytes, and one level of named tables below the root. Arrays, nested/dotted or quoted field names, multiline strings, and escaped strings are unsupported. Scalar values are limited to 64 bytes. UTF-8 comments are validated and stripped before parsing; an initial UTF-8 BOM is accepted. These restrictions mitigate upstream limitations without claiming complete protection against every malformed input or allocation failure.

