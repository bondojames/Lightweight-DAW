# Project State
Spec: v2.0, with changes in `docs/DECISIONS.md` | Current branch: main | HEAD: see `git log`
Accepted phase: none (no tag yet)
Active phase/task: Phase 00 | Status: IMPLEMENTED_UNVERIFIED, awaiting hands-on checks
Toolchain/deps: MSVC 14.44 (VS 2022 Build Tools), CMake 4.4.4, Ninja, Windows SDK 10.0.26100; Dear ImGui v1.93.0-docking at b916186e; no audio SDKs yet
Verified builds: `builds/phase-00/LightweightDAW.exe`, built from 11efa22; hashes in `docs/HANDOFF.md`. Builds are not stored in Git.

Implemented today (verified by automated tests or a real run):
- Win32 window, message loop, per-monitor DPI v2 manifest, D3D11 hardware device, flip-model swap chain, resize and minimize.
- One ImGui context, docking and multi-viewports, Production and Live workspaces, Reset Workspace, saved layouts, corrupt-layout fallback, stranded-panel rescue.
- Frame scheduler using a high-resolution waitable timer: about 10 Hz idle, 60 Hz when active, no drawing when minimized.
- Real CPU, memory and frame diagnostics. Audio status reads `not initialized`, and transport buttons are disabled.
- Native Open and Save dialogs, main-window file drop, settings, logging, local crash dumps, simulated GPU reset recovery.
- Interfaces only, with no implementation: `IAudioBackend`, `IAudioProcessor`, `IMidiBackend`, `IPluginHostAdapter`.

Written but not yet proven by a person or on target hardware: real mouse docking, second monitor, real DPI changes, dialogs and drops by hand. Checklist: `tests/manual/phase-00.md`.

Key invariants:
- `core`, `engine`, `plugins` link no ImGui, D3D or window code; CMake targets enforce this, and the test binary proves it.
- Future audio callback: no allocation, locks or I/O. `core/logging` must never be called from it.
- UI and layouts are never authoritative project data. No schema exists yet.
- Official ImGui backends stay unmodified. GPU recovery restarts both backends (ADR-010).
- Software rendering only with `--warp`, labeled in the UI; never automatic.

Build command: `tools\build.cmd release` (finds MSVC itself, fetches deps, builds, runs tests)
Test command: `ctest --preset release` after a build, or `build\release\bin\LightweightDAW.exe --ui-self-test`
Package command: `tools\package.cmd 00`
Benchmark: `tools\profile.ps1`. Ryzen 7 7700X, RX 6600 XT, 1 display: idle 0.37% of one core, 10 UI frames/s, 43 MiB. No audio yet, so callback load and xruns do not apply.

Blocking issues:
- Hands-on checks pending; a second display must be active for rows 4, 13, 14.
- GitHub repo does not exist, so CI has never run.
- OPEN-001 plug-in license exception and OPEN-002 ASIO test driver await owner decisions.

Decisions changed since last handoff: ADR-001 to ADR-013 are new (first handoff).
Next file: docs/NEXT_TASK.md
