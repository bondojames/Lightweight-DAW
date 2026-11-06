# Test Results

## Phase 00, automated, run 2026-11-06

Machine: Ryzen 7 7700X (16 logical cores), 31 GB RAM, AMD Radeon RX 6600 XT, one 1920x1080 display, Windows 11 Pro build 26200. MSVC 14.44, CMake 4.4.4.

| Check | Result |
|---|---|
| Release build from a fresh `git clone` of 4e1c89d | PASS |
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
- Idle, active and detached ran on commit 4e1c89d. Minimized ran on the next commit, which only adds logging and this validity check.
- GPU time itself is not measured yet, only CPU time spent on rendering calls.
- **One minimized run was discarded.** A first full run at 4e1c89d drew 666 frames, at 11.1 frames/s, because the window was not minimized during it. That build had no logging to show what restored it. A click on the window is the likely cause, but this is unproven. The rerun logged a single minimize at 0.05 s and drew 0 frames over the full 120 seconds. The run now fails if the window was restored.
- Earlier numbers, before the timer fix, are also discarded: active reached only 49 frames/s, due to the 15.6 ms system tick.

## Phase 00, hands-on

Performed by the owner on 2026-11-06 on the single-monitor PC, using the previous package. That build has the same code, before the license wording was added to About and the ZIP. Results are as the owner reported them.

| Checks in `tests/manual/phase-00.md` | Result |
|---|---|
| 1-3, 5, 7-12, 15-17 | PASS |
| 6: detach a panel, close and reopen the app | PASS. The owner reported the panel was still detached after restart, which is the intended behavior |
| 4, 13, 14: need a second display | BLOCKED, none connected |

The current package differs from the tested one only as above. It passed the automated tests and the extracted-ZIP run, and was built from a fresh clone at 4de4dce.

## Phase 00, GitHub Actions

Run 42243642981 on commit c2f9e1b: success, with software rendering and no GPU.

## Phase 01, automated, run 2026-11-06

| Check | Result |
|---|---|
| Release build from a fresh clone of 174b237 | PASS |
| CTest `unit`: 75 cases | PASS |
| CTest `app_ui_self_test`: 16 steps; the audio clock keeps moving through a GPU reset, workspace switches and window changes | PASS |
| Whole CTest suite repeated 5 more times | PASS each time |
| Extracted ZIP runs with no developer PATH; DLLs are Windows system DLLs only | PASS |
| Offline beat spacing at 44.1, 48 and 96 kHz | PASS, mean within 0.1 frame |
| Tempo math after 2 hours at 96 kHz | PASS, exact |
| Output identical for any block size | PASS |
| No heap allocation in engine processing, plus a test that the counter works | PASS |
| No device, device lost and returned, missing saved device, failed open, default change (scripted backend) | PASS |

### Real devices (silent, gain 0, 3 seconds per setting)

| Output | Rate | Asked 128 / 256 / 512, got | Dropouts | Largest peak load |
|---|---|---|---|---|
| Headphones (Hesh Evo) | 44100 | 441 / 441 / 441 | 0 | 0.21% |
| Speakers (Thronmax MDrill Zero) | 48000 | 480 / 480 / 480 | 0 | 0.18% |
| MSI G27C4 monitor | 48000 | 480 / 480 / 480 | 0 | 0.44% |
| Speakers (High Definition Audio Device) | 48000 | 128 / 256 / 480 | 0 | 0.36% |

| Check | Result |
|---|---|
| 10 stop and restart cycles, each producing callbacks | PASS |
| 5 injected 40 ms stalls, one per second | 5 of 5 detected, all blamed on the engine |
| Rate requests 44100 and 96000 on the default device | Ran with 0 dropouts; 96000 used Windows resampling |

### Timing through Windows loopback (faint output, gain 0.1, 10 s, High Definition Audio Device)

| Stream | Clicks | Measured spacing, expected 24000 | Largest error | Capture gaps |
|---|---|---|---|---|
| 48 kHz, 128 frames | 20 | 24000.000 | 0.00 frames | 0 |
| 48 kHz, 256 frames | 20 | 24000.000 | 0.00 frames | 0 |
| 44.1 kHz via Windows resampling, measured at 48 kHz | 20 | 24000.053 | 1.00 frame | 0 |

### Soak (commit 7867e8f, metronome running, silent output, desktop in normal use)

| Run | Length | Stream | Callbacks | p50 / p95 / p99 load | Peak | Dropouts | Callback allocations |
|---|---|---|---|---|---|---|---|
| Tier 1 | 30 min | 48 kHz, 256 frames | 337,480 | at most 0.5% each | 2.71% | 0 | 0 |
| Tier 2 proxy, 4 logical cores (affinity 0x55, confirmed separately) | 10 min | 48 kHz, 256 frames | 112,500 | at most 0.5% each | 0.89% | 0 | 0 |

The later commit 174b237 changes only profile output and the self-test.

Limits of this evidence:
- The soak has no window open and only a metronome to process. It proves the device path and a lower bound on engine cost, not a real session.
- The 4-core limit is only a proxy: the same fast cores and full RAM.
- Zero dropouts is what the detector reports; ADR-017 covers what it cannot see.
- No automated test covers blocking calls in the callback. That rests on code review.

### UI cost with audio open (commit 7867e8f, 60-second settle, 60-second window, default device Hesh Evo at 44.1 kHz)

| Mode | UI frames/s | Process CPU, % of one core | % of whole system | Max working set |
|---|---|---|---|---|
| Idle | 9.98 | 0.729 | 0.046 | 45.4 MiB |
| Active (forced) | 58.85 | 1.849 | 0.116 | 45.7 MiB |
| Minimized | 0.00 | 0.104 | 0.007 | 38.7 MiB |
| One detached window | 13.22 | 1.120 | 0.070 | 45.6 MiB |
| Playing metronome (silent) | 29.70 | 1.276 | 0.080 | 45.4 MiB |

The app logs every dropout, and these runs logged none. That build did not write audio numbers to its JSON. A 13-second recheck after the fix reports running, p99 at most 0.5%, and 0 dropouts.

## Phase 01, hands-on

Checklist: `tests/manual/phase-01.md`. The owner confirmed by ear that the metronome and test tone work (checks 1 and 5, in part). All other rows: PENDING.

## Not run

| Check | Why |
|---|---|
| Clean-system run | Not yet run on the laptop |
| Windows 10 | Out of scope (ADR-004) |
| Real driver failure | Only a simulated device reset (KI-005) |
| Real device unplug | Scripted backend only; listening check 10 covers it |
