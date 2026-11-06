# Project State
Spec: v2.0, with changes in `docs/DECISIONS.md` | Current branch: main | HEAD: see `git log`
Accepted phases: 01 and 02 (tags phase-01-accepted, phase-02-accepted). Phase 00 is BLOCKED only on its three dual-monitor checks.
Active phase/task: none. Phase 03 is next, and the owner has asked not to start it yet.
Toolchain/deps: MSVC 14.44 (VS 2022 Build Tools), CMake 4.4.4, Ninja, Windows SDK 10.0.26100; Dear ImGui v1.93.0-docking at b916186e; libsndfile 1.2.2 at 72f6af15 (DLL); libsamplerate 0.2.2 at c96f5e3d; WASAPI and Media Foundation from Windows
Verified builds: `builds/phase-02/`, built from 1fb6681; hashes in `docs/HANDOFF.md`. Builds are not stored in Git.
Repository: https://github.com/bondojames/Lightweight-DAW (public)

Implemented today (verified by automated tests or a real run):
- All of Phase 00 (see `docs/TEST_RESULTS.md`).
- `AudioEngine`: 64-bit sample clock, tempo map with no accumulated drift, sample-accurate metronome with accent, 440 Hz test tone, declicked stop. Same code for live and offline.
- `WasapiBackend`: event-driven shared mode, period negotiation via IAudioClient3, MMCSS Pro Audio priority, any device format, Windows resampling for other rates.
- `AudioService`: device work on a control thread; no-device message, reconnect, follows the Windows default, falls back from a missing saved device.
- Lock-free commands (UI to audio) and telemetry (audio to UI): load percentiles, dropouts by cause.
- UI: transport, Space bar, metronome, tone, BPM, device, rate and buffer settings, audio diagnostics, offline metronome test.
- `LightweightDAW_audiotest.exe`: headless device tests, including loopback timing, an 8-track playlist and a long-file import benchmark.
- Phase 02: immutable project versions with undo; import via libsndfile and Media Foundation with resampling and a float cache; waveform pyramid; arrangement snapshots; block disk streaming with a preload gate; timeline and inspector UI. There is no save yet.

Owner confirmed all 18 Phase 02 hands-on checks on the fixed build, including MP3: `tests/manual/phase-02.md`.

Owner confirmed all 12 Phase 01 listening checks: `tests/manual/phase-01.md`.

Key invariants:
- `core`, `engine`, `plugins` link no ImGui, D3D or window code; CMake targets enforce this.
- Audio callback: no allocation, locks or I/O. `RealtimeSection` plus counting `operator new` in the test programs proves the allocation part. `core/logging` must never be called from it.
- Audio survives GPU resets and workspace changes; the self-test checks this.
- UI commands go through `AudioService::post`, from the UI thread only (single producer).
- Layouts are never authoritative project data. No project schema exists yet.
- Official ImGui backends stay unmodified. Software rendering only with `--warp`.

Build command: `tools\build.cmd release`
Test command: `ctest --preset release` after a build, from an MSVC x64 environment
Package command: `toolspackage.cmd 02`
Benchmark: `toolsprofile.ps1` and `LightweightDAW_audiotest`. 7700X, 48 kHz, 256 frames, 8 tracks: p99 load at most 0.5%, peak 1.33%, 0 dropouts and 0 disk misses in 10 minutes. UI with a clip: idle 0.44%, playing 1.64% of one core, 52 MiB.

Blocking issues:
- Phase 00 checks 4, 13, 14 need a second display.
- Laptop results not yet recorded (ADR-005 Tier 3).
- OPEN-002: ASIO driver and audio interface.
- Phase 01 tag and Phase 02 are not on GitHub yet, so CI has not run on Phase 02.

Decisions changed since last handoff: ADR-018 to ADR-023.
Next file: docs/NEXT_TASK.md
