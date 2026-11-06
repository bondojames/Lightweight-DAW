# Test Results

## Phase 00, automated, run 2026-11-06

Machine: Ryzen 7 7700X (16 logical cores), 31 GB RAM, AMD Radeon RX 6600 XT, one 1920x1080 display, Windows 11 Pro build 26200. MSVC 14.44, CMake 4.4.4.

| Check | Result |
|---|---|
| Release build from a fresh `git clone` of f632734 | PASS |
| CTest `unit`: 28 headless cases, no ImGui or D3D linked | PASS |
| CTest `app_smoke_warp`: real window on software rendering | PASS |
| CTest `app_ui_self_test`: 11 scripted steps on real ImGui state | PASS |
| Extracted ZIP runs with no developer PATH, on the real GPU | PASS |
| EXE DLL dependencies are Windows system DLLs only; no Qt | PASS |
| Settings and layouts in a folder named `dönnées_日本語_тест` | PASS |
| Crash writes a minidump (`--test-crash`, 61 KB) | PASS |
| Saved panel visibility survives a relaunch (edited file, then relaunched) | PASS |

Self-test steps: factory docking; detach to a second OS window; switch to Live; switch back restores the detached panel; GPU reset keeps UI and detached panel; stranded panel comes back; closing a detached window closes only that panel; Reset Workspace; corrupt layout falls back and notifies; main window resize; minimize and restore.

The self-test drives ImGui directly. It does not prove real mouse dragging, physical monitors or real DPI changes.

## Phase 00, UI cost on the real GPU

60-second settle, then a 60-second window. Single 1080p monitor. Desktop in normal use, not an isolated system.

| Mode | UI frames/s | Process CPU, % of one core | % of whole system | Avg build / render / present ms | Draw calls | Working set |
|---|---|---|---|---|---|---|
| Idle | 9.97 | 0.365 | 0.023 | 0.099 / 0.029 / 0.139 | 11 | 43.3 MiB |
| Active (forced) | 58.77 | 1.094 | 0.068 | 0.077 / 0.027 / 0.127 | 11 | 43.2 MiB |
| Minimized | 0.00 | 0.000 | 0.000 | none | 0 | not captured |
| One detached window | 9.97 | 0.547 | 0.034 | 0.118 / 0.221 / 0.054 | 15 | 43.3 MiB |

Against spec targets: at most 3% of total-system CPU and 450 MiB when idle: both met by a wide margin, on this machine. Not low-end evidence (ADR-005).

Notes on these numbers:
- Idle, active and detached ran on commit f632734. Minimized ran on the next commit, which only adds logging and this validity check.
- GPU time itself is not measured yet, only CPU time spent on rendering calls.
- **One minimized run was discarded.** A first full run at f632734 drew 666 frames, at 11.1 frames/s, because the window was not minimized during it. That build had no logging to show what restored it. A click on the window is the likely cause, but this is unproven. The rerun logged a single minimize at 0.05 s and drew 0 frames over the full 120 seconds. The run now fails if the window was restored.
- Earlier numbers, before the timer fix, are also discarded: active reached only 49 frames/s, due to the 15.6 ms system tick.

## Phase 00, hands-on

Not yet done. Checklist: `tests/manual/phase-00.md`. Status of every row: PENDING. Rows 4, 13 and 14 are BLOCKED until a second display is active.

## Not run

| Check | Why |
|---|---|
| GitHub Actions workflow | No repository yet; the workflow has never executed |
| Clean-system run | Windows Sandbox not installed; no VM |
| Windows 10 | Out of scope (ADR-004) |
| Real driver failure | Only a simulated device reset (KI-005) |
