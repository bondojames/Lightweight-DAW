LightweightDAW (working title) - Phase 02 build

WHAT THIS IS
Real audio playback with an editable timeline: import audio files, then arrange,
trim, split and loop them, with gain, pan, mute and solo on each track. Your files
are never changed.

THERE IS NO SAVE YET
Projects arrive in the next phase. Anything you arrange is lost when you close
the app. Recording and MIDI come later too.

HOW TO RUN
1. Extract this whole ZIP to any folder.
2. Double-click LightweightDAW.exe.
This build is not code-signed, so Windows SmartScreen may warn you.

SUPPORTED
Windows 11, x64, with a Direct3D 11 graphics driver. No account, no telemetry.

WHAT TO TRY
Turn your volume down first.
- Drag a WAV, AIFF, FLAC, MP3 or M4A onto the Timeline panel, then press Space.
- Drag clips to move them, and drag their edges to trim. S splits at the playhead,
  Ctrl+D duplicates, Delete removes, Ctrl+Z undoes. Hold Alt to ignore snapping.
- Click the ruler to move the playhead. Shift-drag the ruler to set a loop region.
- Ctrl+wheel zooms, the wheel scrolls, and Shift+wheel scrolls tracks.
- Tick Metronome and press Play or the Space bar.
- Tick Test tone for a steady, quiet 440 Hz note.
- Audio menu: choose the output device, sample rate and buffer size.
- Diagnostics panel: audio load, dropouts by cause, and what the driver really gave.
- Audio menu: run the offline metronome test, which needs no audio device.
- Drag a panel's tab to dock it elsewhere, or out of the window to detach it.
- Workspace menu: switch between Production and Live, or Reset Workspace.

BUFFER SIZES
Many Windows drivers only offer 10 ms (about 480 frames) in shared mode. The app
uses the closest size the driver allows and shows you what it got.

TESTING WITHOUT THE WINDOW
LightweightDAW_audiotest.exe is a console tool for measuring a machine's audio
path. It is silent unless you pass --gain. Run it with no arguments for its options.

WHERE YOUR SETTINGS GO
%LOCALAPPDATA%\LightweightDAW
To keep them beside the program instead, put an empty file named portable.flag
next to LightweightDAW.exe (the folder must be writable).

LICENSE
GPL-3.0-or-later with a permission for loading non-GPL plug-ins and drivers; see
LICENSE and LICENSE-EXCEPTION.md. Third-party notices: THIRD_PARTY_NOTICES.md and
the licenses folder. Source code: https://github.com/bondojames/Lightweight-DAW

KNOWN ISSUES
See docs/KNOWN_ISSUES.md in the source repository.
