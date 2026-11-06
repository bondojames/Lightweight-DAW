# Architecture Decisions

Newest decisions override the master spec where they conflict. "Owner" decisions came from the project owner.

**ADR-001: Repository outside OneDrive** (2026-11-06, owner). The repo lives at `C:\dev\Keris_DAW`. OneDrive sync can lock build outputs and corrupt `.git`.

**ADR-002: Working name** (2026-11-06, owner). Keep `LightweightDAW` and `.ldaw` until a final name is chosen. The folder name `Keris_DAW` is not a product decision.

**ADR-003: Dependency pinning** (2026-11-06, owner approved). CMake `FetchContent` with exact commit hashes, checked out in the ignored `.deps/` folder. Use `vcpkg` only if a later dependency cannot reasonably be built this way. Spec section 3 allows a pinned lockfile instead of `vcpkg.json`.

**ADR-004: Windows 11 only for now** (2026-11-06, owner). Overrides the spec's Windows 10 22H2 target and removes its Windows 10 acceptance checks. Nothing deliberately blocks Windows 10, but it is untested and unsupported.

**ADR-005: Performance targets** (2026-11-06, owner delegated). The goal is no stutter or quality loss from low-end to high-end machines, with modern laptops as the likely user hardware. The spec's 4-core, 8 GB reference is not a real machine here, so results are recorded at these tiers:
- **Tier 1, baseline:** the development PC (Ryzen 7 7700X, 16 logical cores, 31 GB, RX 6600 XT). Regression gates of spec section 7 apply here.
- **Tier 2, low-end proxy:** the same PC limited to 4 logical cores from Phase 01 on, reported as a proxy and never as real low-end hardware.
- **Tier 3, real laptop:** the owner has a laptop available. Record its model and run the benchmarks before v1.0 acceptance, and earlier when useful.

The numeric goals in spec section 7 stay. CPU is always reported against both one logical core and the whole system, because a whole-system figure flatters a 16-thread machine. A smooth result on every machine cannot be promised: the app measures and reports instead.

**ADR-006: Static C runtime** (2026-11-06). `/MT`, so the portable folder needs no VC++ redistributable. Future DLL dependencies must use C-style interfaces, with no memory or CRT objects crossing the boundary.

**ADR-007: System font, dynamic scaling** (2026-11-06). Use Segoe UI from Windows, so no font is redistributed. ImGui 1.92+ renders glyphs on demand, which replaces the spec's "rebuild the font atlas on DPI change" while meeting its intent. CJK display needs a fallback font; see KNOWN_ISSUES.

**ADR-008: VST3 calls stay on the UI thread** (2026-11-06). Clarifies spec section 4.1, which has a worker prepare plug-ins. VST3 expects controller and most setup calls on the UI thread. The worker prepares data and builds graphs, then marshals plug-in calls to the UI thread. Confirm against Steinberg's threading docs in Phase 09.

**ADR-009: Foundations built before the phases that need them** (2026-11-06). Spec phases add undo in 03, delay compensation in 09 and never assign graph snapshots. Build the command system, immutable graph snapshots and latency reporting in Phases 01 and 02 so they are not retrofitted. `IAudioProcessor::latencyFrames` already exists for this.

**ADR-010: GPU recovery restarts both ImGui backends** (2026-11-06). `ImGui_ImplDX11_Shutdown` calls `DestroyPlatformWindows`, which also clears the Win32 backend's data on the main viewport. Restarting only the renderer caused an access violation, found by the UI self-test. The official backends remain unmodified.

**ADR-011: Own test harness** (2026-11-06). A small harness in `tests/unit` rather than another dependency. Revisit if tests outgrow it.

**ADR-012: Measure over at least 8 seconds** (2026-11-06). Windows CPU counters tick about every 15.6 ms, so a light UI reads 0% over one second. Displayed CPU averages the last 8 samples. Profile runs use their own baseline for the whole window.

**ADR-013: Virtual MIDI and the APC Mini** (2026-11-06, owner hardware). No MIDI keyboard is available. Use the Akai APC Mini for physical MIDI input, launching and LED output tests. Pads do not exercise a full velocity range, sustain or pitch bend, so test those with synthetic events and record that those physical checks are not covered.

**ADR-014: Plug-in and driver licensing permission** (2026-11-06, owner). The project is GPL-3.0-or-later with a GPLv3 section 7 additional permission in `LICENSE-EXCEPTION.md`, allowing combination at runtime with non-GPL plug-ins and audio or MIDI drivers. `LICENSE` stays the unmodified GPL text. Contributions are accepted under the same terms. The permission cannot cover third-party GPL components, so check how each one (Rubber Band, the ASIO SDK) interacts with it during that dependency's audit. Not legal advice.

**OPEN-002: ASIO driver for testing.** No ASIO driver is installed. Needed by Phase 04 or 12. The owner is confirming whether an audio interface is available. Options are in KNOWN_ISSUES. The spec's sign-off requires a real ASIO interface, and a wrapper driver does not satisfy that without an owner decision.
