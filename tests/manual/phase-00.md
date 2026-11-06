# Phase 00 hands-on checks

These need a person and physical hardware. Record the result of each in `docs/TEST_RESULTS.md`. Use the program from the extracted ZIP, not the build folder.

| # | Do this | Pass if |
|---|---|---|
| 1 | Launch `LightweightDAW.exe` by double-clicking it | The window opens, and the top bar says `Audio: not initialized` |
| 2 | Drag the Mixer tab onto another panel's edge, then into the middle of a panel | It docks in both places, and the preview shows where it will land |
| 3 | Drag the Mixer tab outside the main window | It becomes its own OS window with a taskbar entry |
| 4 | With two monitors: move that window to the second monitor | It draws correctly and stays responsive |
| 5 | Close that detached window with its X | Only the panel closes, and the app keeps running. View menu brings it back |
| 6 | Detach a panel again, close the app, reopen it | The layout returns, including the detached panel |
| 7 | Workspace menu: switch to Live, then Production | Each keeps its own layout |
| 8 | Workspace menu: Reset Workspace | The default layout returns |
| 9 | File menu: test the Open dialog and Save dialog, including Cancel | Windows dialogs appear, and Diagnostics shows the chosen path or `(cancelled)` |
| 10 | Drag a file from Explorer onto the main window; include a name with accents, e.g. `café.txt` | Its path appears in Diagnostics, accents intact |
| 11 | Resize, maximize, minimize and restore the main window | No blank areas, flicker or crash |
| 12 | Windows display scale at 100%, then 150% (Settings > System > Display) | Text stays sharp and readable, and nothing is cut off |
| 13 | With two monitors at different scales: drag the main window and a detached panel between them | Text resizes and stays readable |
| 14 | With a detached panel on monitor 2: unplug or disable monitor 2 | The panel comes back onto monitor 1, and the app keeps running |
| 15 | Preferences: switch to High contrast and Light | Everything stays readable |
| 16 | Tab and arrow keys, with no mouse | Focus visibly moves through controls |
| 17 | Leave it idle for a minute, watching Diagnostics | Frame rate settles near 10 per second, and Task Manager shows very low CPU |
