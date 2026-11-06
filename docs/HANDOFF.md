# Continuation Handoff
Commit: 1fb6681 is the built code; later commits are docs only | Accepted tag: phase-01-accepted | Working tree clean? Yes when this was committed

This session completed (verified): Phase 02 code, automated tests, playlist soak, profile and packaging. Details in `docs/TEST_RESULTS.md`. Phase 02 commits and the Phase 01 tag are local only until the owner approves a push.

Build artifacts: `builds/phase-02/`, `builds/LightweightDAW-phase-02-win64.zip`
SHA256:
- EXE `c29893cbc0cc6256dd1e202e0f8c464041b5b931c79e5e0af23ec0eaf69ffea8`
- ZIP `dbcbe1131e4587faf18f880f70f4b09fdd9792595965173a44a98c1c52923f82`

Commands actually executed + results:
- `tools\build.cmd release`: pass; CTest 3 of 3 (142 unit cases, smoke, 36-step UI self-test with simulated mouse input, in Release and Debug).
- Same from a fresh clone at 1fb6681: pass.
- `tools\package.cmd 02`: pass, including a run from the extracted ZIP.
- `LightweightDAW_audiotest playlist` (run at f0c7b2a; the engine is unchanged since): 10 minutes, and 5 minutes limited to 4 cores: 0 dropouts, 0 disk misses.
- `LightweightDAW_audiotest importbench --seconds 1800`: 0.47 s with the source in the OS cache.

Runtime/manual verification actually done: the owner tested f0c7b2a and the timeline failed; causes and fixes are in TEST_RESULTS. I inspected one screenshot of the fixed layout. The fixed build has not been used by a person yet, and MP3 import has never been run.

CPU/audio load/xruns/RAM baseline delta: playing with a clip: 1.64% of one core, up from 1.28% with only a metronome. Working set 52 MiB, up from 45.

Known failures, hardware missing, and precise repro: no known test failures. Second display, ASIO, audio interface and laptop data are outstanding. See `docs/KNOWN_ISSUES.md`, especially KI-014 (no save) and KI-015 (MP3).

Changes to interfaces/schema/ADR: `AudioService` now takes a cache folder and owns media and streaming. `IAudioBackend` unchanged. ADR-018 to ADR-023. No saved project schema yet.

Advice for next engineer:
- `sed` and `awk` edits of escaped C++ strings silently failed several times here. Use an edit tool.
- `ImDrawList::AddRect` swapped its `thickness` and `flags` arguments in ImGui 1.92.8.
- libsamplerate only flushes its tail if `data_in` is non-null at end of input.
- Self-test results can depend on the mouse: tooltips are OS windows.
- Passing tests said nothing about the UI until it had simulated input. Add a gesture step for every new interaction, and put a bug back once to prove its step fails.
- `ImGui::PopClipRect` restores the window clip from the draw list stack, so never mix it with `ImDrawList::PushClipRect` around it.
- Never end a toolbar with `SameLine` unless something follows it.
- `tools/capture-window.ps1` refuses to run unless the app is in front, because a capture otherwise shows whatever the owner has open.
- `Start-Process` makes Windows ignore the first `ShowWindow` argument.
- `MsgWaitForMultipleObjectsEx` timeouts round up to 15.6 ms. Use the existing waitable timer.

Next action: read NEXT_TASK.md then execute; do not skip tests.
