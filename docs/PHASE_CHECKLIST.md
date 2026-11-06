# Phase Checklist

| Phase | Name | Status |
|---|---|---|
| 00 | Build infrastructure and native ImGui shell | IMPLEMENTED_UNVERIFIED |
| 01 | Audio callback, test tone, metronome, diagnostics | PLANNED |
| 02 | Audio track arrangement, import, playback | PLANNED |
| 03 | Browser, project save/load, undo, recovery | PLANNED |
| 04 | Audio capture and physical input | PLANNED |
| 05 | MIDI engine, piano roll, basic instrument | PLANNED |
| 06 | Drum machine and pattern sequencing | PLANNED |
| 07 | Mixer, routing, internal effects, monitoring | PLANNED |
| 08 | Modular synthesizer rack | PLANNED |
| 09 | VST3 host and plug-in state | PLANNED |
| 10 | Live performance mode and MIDI mapping | PLANNED |
| 11 | Automation, editing polish, resampling, freeze | PLANNED |
| 12 | Export, polish, reliability, v1.0 release | PLANNED |
| 13 | Post-v1: CLAP and modular extensions | PLANNED |

## Phase 00 against the definition of done (spec section 10)

| # | Requirement | State |
|---|---|---|
| 1 | Deliverables implemented and mapped to tests | Done, except hands-on rows |
| 2 | Clean build from a fresh checkout with pinned deps | Done |
| 3 | Release EXE executed, zipped, SHA-256 recorded | Done |
| 4 | Automated tests pass; manual tests performed | Automated done; manual PENDING |
| 5 | Performance recorded on named hardware | Done on Tier 1 (ADR-005) |
| 6 | No critical data loss, crashes or regressions | None known; pending hands-on |
| 7 | Notices and licensing verified | Dear ImGui done; OPEN-001 awaits owner |
| 8 | Prior project files load | Not applicable, no project format yet |
| 9 | Docs current | Done |
| 10 | User performs the named use case from the package | PENDING |
