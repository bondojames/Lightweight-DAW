# Next Engineering Task
Phase / issue: Phase 03 (browser, project save and load, undo, recovery), once the owner finishes `tests/manual/phase-02.md`.

User-visible outcome: create a multi-clip project, save it to a folder, close the app, reopen it and keep editing.

Scope (maximum 3 bounded deliverables):
1. Project schema v1 with a migration entry point: human-readable JSON, stable UUIDs, relative media paths, atomic save with a last-known-good backup. Audit and pin a JSON library first.
2. New, Open, Save, Save As; autosave timer with a recovery selector that never overwrites the user's file; missing media placeholders and relink.
3. Asynchronous in-app folder browser with search, favourites, preview and drag to the timeline.

Out of scope: recording, MIDI, plug-ins.

Files/subsystems likely affected: new `core/persistence`, `ui/library`; `core/model`, `core/commands`, `app/Application.cpp`; `engine/media` for cache cleanup (KI-016) and on-demand CJK fonts (KI-002).

Dependencies and migration needs: `model::Id` is a counter today and must become stable UUIDs before the first file is written. Commit a small redistributable MP3 fixture (KI-015). Replace main-window file drop with OLE so detached panels accept drops (KI-001).

Acceptance tests (automated + physical where needed): spec Phase 03 list. Deterministic project round trip of audio, mixer and clip timing; killing the app mid-save leaves the last good project intact; a moved project folder still finds relative media; unplugged media stays as a placeholder; recovery asks rather than replacing the original; an example session ZIP.

Required packaged output: `builds/phase-03/`, ZIP, SHA-256 and log.

Failure/rollback policy: do not replace the last accepted tag or build.
