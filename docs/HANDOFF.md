# Continuation Handoff
Commit: 11efa22 is the built code; later commits are docs only | Accepted tag: none | Working tree clean? Yes when this was committed

This session completed (verified): Phase 00 shell, tests, packaging, license audit of Dear ImGui. Details in `docs/PROJECT_STATE.md` and `docs/TEST_RESULTS.md`.

Build artifacts: `builds/phase-00/LightweightDAW.exe`, `builds/LightweightDAW-phase-00-win64.zip`
SHA256:
- EXE `90bc31c0c2bf87d6f29931dfedc569f0df5ec4bf85e7aa3b65e53c1198260dc6`
- ZIP `1e0f42f7bf1f3c6d0d3f33b110dc8d11a5bd72eed70b61026eed7da6b358fb6c`

Commands actually executed + results:
- `tools\build.cmd release`: pass; CTest 3 of 3 (28 unit cases, smoke, 11-step UI self-test).
- Same from a fresh clone at f632734: pass.
- `tools\package.cmd 00`: pass, including a run from the extracted ZIP with no developer PATH.
- `tools\profile.ps1`: results in TEST_RESULTS, one invalid run recorded there.

Runtime/manual verification actually done: launched on the RX 6600 XT, and inspected one screenshot of the factory layout. Non-ASCII data folder and crash dump tested by command line. No person has dragged a panel, opened a dialog, or dropped a file yet.

CPU/audio load/xruns/RAM baseline delta: first baseline. Idle 0.365% of one core, active 1.094%, minimized 0, detached 0.547%; about 43 MiB. Audio metrics do not apply yet.

Known failures, hardware missing, and precise repro: no known test failures. Second display not active (`Screen.AllScreens` reports one, though WMI listed two monitors). APC Mini not detected. No ASIO driver. Windows Sandbox not installed. See `docs/KNOWN_ISSUES.md`.

Changes to interfaces/schema/ADR: ADR-001 to ADR-013; OPEN-001, OPEN-002. The owner chose Windows 11 only (ADR-004), which overrides the spec.

Advice for next engineer:
- `Start-Process` makes Windows ignore the first `ShowWindow` argument, so a second call is needed to start minimized.
- `MsgWaitForMultipleObjectsEx` timeouts round up to 15.6 ms. Use the existing waitable timer.
- `tools/build.cmd` reads extra CMake arguments from `LDAW_CMAKE_ARGS`.
- Long Bash heredocs can exceed the Windows command-line limit. Write files individually.

Next action: read NEXT_TASK.md then execute; do not skip tests.
