# Continuation Handoff
Commit: 4de4dce is the built code; later commits are docs only | Accepted tag: none | Working tree clean? Yes when this was committed

This session completed (verified): Phase 00 shell, tests, packaging, license audit of Dear ImGui. Details in `docs/PROJECT_STATE.md` and `docs/TEST_RESULTS.md`.

Build artifacts: `builds/phase-00/LightweightDAW.exe`, `builds/LightweightDAW-phase-00-win64.zip`
SHA256:
- EXE `8b436c3e86308fc719a66397af44b0b50dcb283c7f1a0ea7344a1e81ea10a61f`
- ZIP `0ae94d7b3beda7afe9849c20e2d69121aa7ea66fa6f0a7b63135437831ee88f0`

Commands actually executed + results:
- `tools\build.cmd release`: pass; CTest 3 of 3 (28 unit cases, smoke, 11-step UI self-test).
- Same from a fresh clone at 4e1c89d: pass.
- `tools\package.cmd 00`: pass, including a run from the extracted ZIP with no developer PATH.
- `tools\profile.ps1`: results in TEST_RESULTS, one invalid run recorded there.

Runtime/manual verification actually done: the owner ran checks 1-3, 5-12 and 15-17 by hand on the single-monitor PC and reported them passing. They used the previous package (same code, before the license text was added to About and the ZIP). I inspected one screenshot. Non-ASCII data folder and crash dump tested by command line.

CPU/audio load/xruns/RAM baseline delta: first baseline. Idle 0.365% of one core, active 1.094%, minimized 0, detached 0.547%; about 43 MiB. Audio metrics do not apply yet.

Known failures, hardware missing, and precise repro: no known test failures. Owner confirmed no second monitor is connected. Audio interface availability unconfirmed. APC Mini not detected. No ASIO driver. Windows Sandbox not installed. See `docs/KNOWN_ISSUES.md`.

Changes to interfaces/schema/ADR: ADR-001 to ADR-014; OPEN-002. The owner chose Windows 11 only (ADR-004), which overrides the spec.

Advice for next engineer:
- `Start-Process` makes Windows ignore the first `ShowWindow` argument, so a second call is needed to start minimized.
- `MsgWaitForMultipleObjectsEx` timeouts round up to 15.6 ms. Use the existing waitable timer.
- `tools/build.cmd` reads extra CMake arguments from `LDAW_CMAKE_ARGS`.
- Long Bash heredocs can exceed the Windows command-line limit. Write files individually.

Next action: read NEXT_TASK.md then execute; do not skip tests.
