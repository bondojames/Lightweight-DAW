# Next Engineering Task
Phase / issue: Phase 02 (audio track arrangement, import and playback), once the owner finishes `tests/manual/phase-01.md`.

User-visible outcome: drop an audio file onto a track, move and trim it, play it from any point, and loop a region.

Scope (maximum 3 bounded deliverables):
1. `core/model` Project, Track, Clip and MediaSource; the undoable command layer (ADR-009); immutable runtime graph snapshots published to the callback with safe retirement.
2. Media: libsndfile and libsamplerate after license audit, Media Foundation for MP3 and M4A, worker-thread prefetch, multiresolution waveform peaks.
3. Custom ImGui timeline: clipped waveforms, zoom, selection, move, trim, split, duplicate, snap, seek, loop, gain, pan, clip-edge fades.

Out of scope: save and load, browser, recording, MIDI.

Files/subsystems likely affected: new `core/model`, `core/commands`, `core/assets`; `engine/graph`; `ui/arrangement`; pins in `cmake/Dependencies.cmake`.

Dependencies and migration needs: audit libsndfile (LGPL; must be a DLL) and libsamplerate before pinning. `package.ps1` must allow only reviewed DLLs. ADR-006 applies: no CRT objects across DLL boundaries.

Acceptance tests (automated + physical where needed): spec Phase 02 list. Mono and stereo imports at native and resampled rates; no UI stall on a large file; no click at a faded split; measured alignment of identical files across tracks; 10-minute playlist with no callback disk I/O; source media unchanged; allocation counter still 0 in the callback.

Required packaged output: `builds/phase-02/`, ZIP, SHA-256 and log.

Failure/rollback policy: do not replace the last accepted tag or build.
