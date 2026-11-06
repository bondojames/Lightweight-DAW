# Next Engineering Task
Phase / issue: finish Phase 00 acceptance, then Phase 01 (real audio callback, test tone, metronome, diagnostics).

User-visible outcome: press Play and hear a timing-accurate metronome or test tone through a chosen Windows audio device.

Prerequisite: Phase 00 checks 4, 13 and 14 need a second display. Tag `phase-00-accepted` only after those pass; never tag it without them. Whether to start Phase 01 first is an owner decision.

Scope (maximum 3 bounded deliverables):
1. `WasapiBackend` implementing `IAudioBackend`: device enumeration, shared-mode event-driven float32 stereo output, format negotiation, safe stop, restart and device change.
2. Transport sample clock, 120 BPM 4/4 metronome with accent and sine test tone behind `IAudioProcessor`, with a preallocated buffer pool and an offline render path that needs no device.
3. Real diagnostics: callback load against the actual deadline, xrun counts by cause, negotiated rate and buffer size. Telemetry reaches the UI through a lock-free snapshot. Enable the transport buttons.

Out of scope: audio input, ASIO, clips, projects, MIDI.

Files/subsystems likely affected: new `engine/device/`, `engine/transport/`, `engine/dsp/`; `ui/panels/Panels.cpp`; `app/Application.cpp`; tests.

Dependencies and migration needs: start the command queue and snapshot structure described in ADR-009. Do not assume RtAudio; evaluate direct WASAPI against it and record the decision.

Acceptance tests (automated + physical where needed): spec Phase 01 list. Beat spacing correct in an offline test; no first-party callback heap allocations under an instrumented test; ten stop and restart cycles; 30-minute xrun run with baseline CPU report at Tier 1 and Tier 2 (ADR-005). A missing output device gives a clear message and offers offline test mode. The owner confirms the sound by ear. Also test that an open native file dialog causes no xrun.

Required packaged output: `builds/phase-01/`, ZIP, SHA-256 and log.

Failure/rollback policy: do not replace the last accepted tag or build.
