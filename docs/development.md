# Safety and ownership

The engine worker owns Runtime, logical hysteresis, desired keys and successfully submitted keys. A shared mapping is a union: releasing one controller input cannot release a keyboard key still requested by another. Failed submissions retain their previous state and retry at most once per key per 100 ms.

Configuration edits use a separate model. Save validates, suspends and releases old input, persists using a flushed same-directory temporary replacement, then waits for activation acknowledgement. Reload uses the same release-first transition. A file/revision comparison avoids silently overwriting observed external edits; it is not an interprocess transaction lock.

EngineService commands use a mailbox, command epochs and an output gate. Stop and environmental pauses inhibit presses before waiting for the worker; an old in-flight sample cannot produce a new press after that gate. Output and mailbox locks have a fixed order. Power, session, recovery and editor pauses remain independent. Shutdown makes five bounded release attempts and joins the worker before destroying windows, tray registration and session mutex ownership.

The tray host remains alive while modal settings close/reopen. Nested dialogs unwind through the centralized exit path. TaskbarCreated restores registration; failure presents a visible Settings/Exit fallback. Only a request to open Settings crosses the bounded single-instance IPC path.

Successful SendInput means submission only. Secure desktops and integrity restrictions are respected. Forced termination and refused releases cannot be made safe through process-local reconciliation alone.
