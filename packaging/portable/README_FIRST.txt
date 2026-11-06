LightweightDAW (working title) - Phase 00 build

WHAT THIS IS
The application shell only: windows, docking, workspaces and diagnostics.
It does not make sound yet. Audio status correctly reads "not initialized".
Audio arrives in Phase 01.

HOW TO RUN
1. Extract this whole ZIP to any folder.
2. Double-click LightweightDAW.exe.
This build is not code-signed, so Windows SmartScreen may warn you.

SUPPORTED
Windows 11, x64, with a Direct3D 11 graphics driver. No account, no telemetry.

WHAT TO TRY
- Drag a panel's tab to dock it elsewhere, or out of the window to detach it.
- Workspace menu: switch between Production and Live, or Reset Workspace.
- File menu: test the Open and Save dialogs (no project files yet).
- Drop a file on the main window; it appears in the Diagnostics panel.
- Close and reopen the app: your layout comes back.

WHERE YOUR SETTINGS GO
%LOCALAPPDATA%\LightweightDAW
To keep them beside the program instead, put an empty file named portable.flag
next to LightweightDAW.exe (the folder must be writable).

LICENSE
GPL-3.0-or-later; see LICENSE. Third-party notices: THIRD_PARTY_NOTICES.md and
the licenses folder. Source code: see the project repository.

KNOWN ISSUES
See docs/KNOWN_ISSUES.md in the source repository.
