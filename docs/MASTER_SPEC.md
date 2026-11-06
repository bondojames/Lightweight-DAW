# Lightweight Windows DAW — Master Development Specification

**Specification version:** 2.0 — FINAL IMPLEMENTATION BASELINE (Dear ImGui / DirectX 11)  
**Prepared:** 2026-11-04  
**Status:** Architecture-approved planning baseline; no implementation claimed  
**Product name:** Working title “Lightweight DAW” (rename before public release)  
**Target:** Windows 10 22H2 and Windows 11, x86-64, 4-core CPU, 8 GB RAM; earlier Windows 10 releases best effort  
**Language:** C++20  
**License:** GPL-3.0-or-later for application code, with an obligatory file-by-file dependency and redistribution audit  
**Distribution:** Runnable portable directory containing a Windows `.exe` plus required DLLs/assets; optional installer  
**Product modes:** Production (arrangement/timeline) and Live Performance (session grid), sharing one project model and one audio engine  
**Fixed GUI architecture:** Dear ImGui (docking branch or pinned docking release) + native Win32 + DirectX 11; Qt is expressly excluded

> **Primary directive for implementation AI:** Build a small, *actually functioning* digital audio workstation in successive, independently testable releases. Never substitute visual mockups for an operating audio feature. Every phase must end in a clean, runnable Windows x64 artifact, a reproducible build, automated tests, manual acceptance evidence, and an updated concise handoff. Never claim a test ran if it did not.

---

## 1. Product intent and scope

Develop a lightweight free/open-source DAW familiar to experienced users of conventional timeline DAWs and clip-based live workstations, without copying their source code, branding, artwork, presets, or proprietary UI assets. Prioritize dependable audio capture/playback, timing accuracy, efficient routing, project portability, predictable real-time behavior, and low CPU/GPU cost even with the editor open. The UI should reveal complexity progressively: approachable defaults, visible controls, efficient keyboard/mouse editing, and deeper routing/patching views for advanced users.

**Core workflow:** Launch `.exe` → choose audio/MIDI device → create/open project → import or record audio, add MIDI and instruments → edit clips and piano-roll notes → sequence drum parts → route tracks and mix → insert effects or VST3 plug-ins → switch to Live Mode, launch synchronized clips/scenes and perform with a MIDI controller → capture a performance into the arrangement → save/reopen → export an interoperable finished master or stems.

**Definition of “industry standard”:** There is no single formal feature certification for DAWs. Here it means implementing the baseline workflows, file interoperability, and engineering behaviors routinely expected in a modern music-production tool; *not* claiming feature parity with Ableton Live, Reason, Cubase, or a professional mastering suite.

### Mandatory v1.0 capabilities

- Multi-track mono/stereo audio and MIDI; playback, recording, scrubbing/seeking, punch/loop-recording basics, transport, tempo, time signature, metronome, count-in.
- Non-destructive arrangement editor: clip move, duplicate, split, trim, slip, fades/crossfades, snap, waveform display, undo/redo, keyboard shortcuts.
- MIDI editing: piano roll, notes/velocity/length, quantization, MIDI device input/output, sustain/CC support, import/export standard MIDI files.
- Drum machine: pads assigned to samples, step grid, per-step velocity, pattern length, MIDI-note mapping, pattern-to-clip workflow.
- Mixer: track/bus/master strips, faders, pan, mute/solo/arm, stereo meters, sends and returns, gain staging, basic processing and routing.
- VST3 host: scan, instantiate, audio/MIDI I/O, parameter/state persistence, plug-in UI when compatible, latency compensation, failure handling. CLAP comes in post-v1 extension.
- Built-in visual patch-cable synthesizer: oscillator, noise, filter, envelopes, LFO, mixer, amplifier, modulation, output, polyphonic MIDI playback, save/load patches.
- Live performance: quantized audio/MIDI clip launching, scenes, looping, controller mapping, scene/track stop, fail-safe behavior, capture to arrangement.
- Standard device selection: USB audio interfaces/microphones/instruments through their *audio drivers*; USB MIDI keyboards/controllers through MIDI drivers; WASAPI support and qualified ASIO host support by the v1.0 release, after SDK licensing review and real-interface tests.
- User library browser (local files), audio preview, drag-and-drop, searchable/favorited folders, missing-media relink, project asset collection.
- Project save/open/autosave/recovery; offline bounce; WAV 16/24-bit PCM and 32-bit float; FLAC and AIFF where supported; optional other codecs only after compliance review.
- Performance display: audio-thread load relative to callback deadline, peak load, underruns, process CPU%, peak/RMS meters, process RAM; actionable diagnostics.
- Portable folder distribution plus optional user-friendly installer, no registration, account, cloud service, telemetry, or paid runtime dependencies.

### Explicitly outside v1.0

- Video scoring, notation/score editor, surround/Atmos, ARA, VST2, AAX/AU hosting, proprietary device ecosystems, online collaboration/cloud sync, AI music generation, fully featured mastering, advanced comping, live pitch correction, convolution reverb, and complex real-time time-stretch/pitch-shift (v1.0 will provide *offline cached tempo matching* for loops instead).
- MIDI 2.0 and MPE are future enhancements, not promises of the initial release.
- Arbitrary zero-delay feedback in modular or track routing graphs; initially refuse cycles with a clear error. Any future feedback path must contain an explicitly specified delay and be engineered/tested separately.
- “Always glitch-free” claims with unknown external plug-ins or faulty drivers; the host instead measures, reports, and recovers where possible.

## 2. Fixed engineering decisions and compliance boundaries

| Topic | Decision / invariant |
|---|---|
| Language/toolchain | Native C++20, MSVC 2022, CMake + Ninja, x64 Debug and Release, CTest, reproducible dependency pins |
| GUI framework | **Dear ImGui docking build**, pinned to a verified tag/commit, MIT license. Use the official `imgui_impl_win32` and `imgui_impl_dx11` backends. **Do not use Qt, JUCE, Electron, WebView, WPF or another app UI framework.** |
| Graphics | Native Win32 application / window message pump; Direct3D 11 device, DXGI swap chains, Windows Graphics/DirectX SDK. UI rendering on UI thread only; never inside audio callback |
| Windows management | One ImGui context and a main dockspace; enable docking, multi-viewports, separate OS windows, movable panels and multi-monitor use. Persist named Production and Live workspace layouts |
| UI widgets | Purpose-built ImGui/ImDrawList waveform, piano-roll, timeline, mixer, drum grid, modulation controls. Evaluate **ImPlot** (diagnostics) and **imgui-node-editor** (rack) as optional pinned MIT-licensed extensions; they are not automatically approved until compatibility + audit tests pass |
| Native Windows integration | Shell Common Item Dialog (`IFileOpenDialog`/`IFileSaveDialog`) for Open/Save/Export; Windows shell drag/drop, clipboard, IME, notifications, file picker, Explorer and accessibility adaptations via Win32 APIs. Use OS windows for incompatible third-party plug-in editors |
| Audio output | WASAPI shared/event-driven first, exclusive if qualified; backend-neutral `IAudioBackend` and ASIO required before v1.0. RtAudio adapter is a candidate, not an unconditional requirement |
| ASIO | Steinberg ASIO SDK offers GPLv3 OR a separate proprietary route. Favor the GPLv3 option for this open-source project only after a file-by-file audit of source and embedded notices; do not bundle vendor drivers |
| MIDI I/O | RtMidi / Windows multimedia MIDI API baseline; hotplug and timestamp normalization provided by DAW, not assumed from RtMidi |
| Plug-ins | VST3 SDK 3.8+ under MIT, pinned. Format-neutral plugin host interface. CLAP (MIT) is Phase 13, not a v1.0 blocker |
| File audio | libsndfile, libsamplerate; Windows Media Foundation codecs when installed/available; optional Rubber Band GPL-compatible background time stretch |
| Project | Versioned, human-inspectable JSON manifest + media/plug-in state files; stable UUIDs, safe saves, migrations and auto-recovery |
| Code license | Application GPL-3.0-or-later; retained third-party copyright/license texts, no paid SDKs, no copied commercial content, no unlicensed audio samples |
| Packaging | Portable `LightweightDAW.exe` directory with all required non-system DLLs/assets + separate optional installer; no per-user sign-in, cloud, subscription or telemetry requirement |
| Compatibility | Windows 10 22H2 (already out of mainstream support as of 2025-10-14) and Windows 11, x64; test both. Windows 10 security updates are OS/vendor responsibility, not a guarantee from this product |

**UI implementation boundary:** Dear ImGui is an *immediate-mode drawing/UI library*, not a complete desktop framework. The product must own the lifetime of its Win32 windows, Direct3D device/swap chains, event loop, custom DAW widgets, file integration, DPI handling, and plug-in editor host windows. The audio engine must run and render offline without initializing ImGui or Direct3D.

**Licensing guardrails:** Dear ImGui, VST3 3.8+, CLAP, ImPlot, and imgui-node-editor are MIT-licensed upstream, but pin *actual* revisions and check every bundled source file. RtAudio has its own MIT-like permissive terms. libsndfile is LGPL and calls for correct dynamic-library/redistribution handling; inspect codec subdependencies. ASIO SDK's GPLv3 alternative does not grant rights to redistribute proprietary ASIO drivers. GPLv2-or-later Rubber Band may be compatible with GPLv3 application distribution; verify exact embedded license/subdependencies. License approval is a recorded gate, not a blanket assumption. Hosting proprietary plug-ins submitted by users does not convey rights to redistribute them. Document VST/ASIO trademark use separately from SDK copyright licensing.

**Windows 10 lifecycle:** Standard Windows 10 support ended on October 14, 2025. Preserve compatibility testing for the supported target edition but do not imply the underlying Windows installation remains supported by Microsoft.

## 3. Source repository and code ownership

```text
lightweight-daw/
  CMakeLists.txt
  CMakePresets.json
  vcpkg.json                       # or pinned dependency lockfile
  LICENSE
  README.md
  THIRD_PARTY_NOTICES.md
  .github/workflows/windows-ci.yml
  app/
    WinMain.cpp                    # DPI awareness; Win32 bootstrap; app lifecycle
    Application.cpp               # services, shutdown, error recovery
  platform/win32/
    WindowHost.*                  # primary HWND, WndProc, message/event loop
    NativeDialogs.*               # Open/Save/Export using IFileDialog
    ShellIntegration.*            # drag/drop, clipboard, Explorer, paths
    DpiIntegration.*              # per-monitor DPI and font atlas scale
  renderer/d3d11/
    D3D11Device.*                 # GPU device, context, resource ownership
    SwapChainManager.*            # primary viewport + secondary HWND handling
    TextureAtlas.*                # waveform/preview GPU texture caches
  ui/
    ImGuiHost.*                   # Dear ImGui context & backend initialization
    WorkspaceManager.*           # persistent, versioned docking layouts
    FrameScheduler.*              # low-CPU event-driven frame pacing
    UiCommands.*                  # UI gestures => validated model commands
    widgets/                      # shared knobs, faders, tooltip, accessibility
    arrangement/                  # waveform timeline, tools, clip lanes
    piano_roll/                   # MIDI notes and velocities
    session/                      # live launch grid, queued states
    mixer/                        # faders, meters, routing
    drum_machine/                 # 16-pad and step grid
    synth_rack/                   # nodes, pins, patch cables
    library/                      # OS filesystem browser and file preview
    diagnostics/                  # CPU/load charts, xruns, devices
    plugin_editor/                # native-host HWNDs + generic parameters
  core/
    model/                        # tracks, clips, instruments, project, tempo
    commands/                     # undoable edits / validated transactions
    persistence/                  # save/load, migration and recovery
    assets/                       # media IDs, relink, waveform peak hierarchy
  engine/
    device/                       # WASAPI, ASIO by v1.0, device enumeration
    graph/                        # immutable runtime processing graphs
    transport/                    # sample clock, tempo and launch scheduling
    mixer/                        # buses, pan, gain, routing, PDC
    dsp/                          # metronome, built-in FX and synth units
    midi/                         # MIDI 1.0 events, scheduling and mapping
    render/                       # deterministic offline bounce
  plugins/
    api/                          # format-neutral host interfaces
    vst3/                         # discovery, host wrapper and state
    clap/                         # phase 13
    scanner/                      # separate low-privilege scan helper
  tests/
    unit/ integration/ golden_audio/ project_fixtures/
    ui/                           # interaction, DPI/docking state tests
    manual/                       # physical audio, monitors and MIDI
  tools/benchmark/                # CPU/audio load/graphics timing suite
  packaging/portable/installer/
  docs/
    MASTER_SPEC.md                # this full specification
    PROJECT_STATE.md              # <=1200 words; first continuation read
    NEXT_TASK.md                  # <=500 words; exactly one task
    HANDOFF.md                    # <=750 words; session completion facts
    DECISIONS.md                  # numbered architecture decisions
    TEST_RESULTS.md
    KNOWN_ISSUES.md
    BUILD_MANIFEST.json
    PHASE_CHECKLIST.md
```

**Rules:** Do not store generated compiler outputs or large media in Git. Commit only genuinely redistributable fixtures. No `ui/` code includes audio driver callback internals; communicate via typed commands and read-only telemetry snapshots. The renderer and native Windows code may never be dependencies of `engine/`. No ImGui state is the authoritative persisted project data.

## 4. Runtime architecture

### 4.1 Process boundaries and threads

**Single primary application process**, with an **isolated plug-in scanner helper process by Phase 09**. Within the primary app:

1. **UI / Win32 message thread:** owns the primary and secondary HWNDs, Direct3D 11 device/context, Dear ImGui context and event dispatch, native dialogs, drawing, audio/MIDI configuration and validated user commands. No DSP computation or disk decoding here.
2. **Audio callback thread:** renders exactly the frames requested by the active driver at a fixed sample rate; bounded and allocation-free in first-party code. Computes transport-aligned audio/MIDI, graph, mixing, meters, and recording-buffer writes.
3. **Control / graph-build worker:** resolves user edits, validates graph topology, allocates new immutable processing snapshots, prepares samples and plug-in state, then publishes safely at a block boundary. Old snapshots are retired outside the audio callback after they are no longer referenced.
4. **Disk/media workers:** decode, prefetch, render waveform peak caches, write captured audio from bounded rings, save projects, manage file browser search/index.
5. **MIDI input path:** timestamp input, normalize message data and route to a bounded queue; convert to sample offsets against the audio stream clock.
6. **Plug-in scanner helper:** discovers plug-in metadata in a separate process with timeouts and a quarantine list; never enumerate/load arbitrary plug-in binaries on the audio callback thread.

**Safety:** Untrusted VST3 plug-ins can crash or block while running *in-process*. The isolated scanner protects discovery only. A true out-of-process DSP plug-in bridge is optional later, subject to IPC latency and stability design; do not promise crash isolation at v1.0.

### 4.1A Dear ImGui + Direct3D 11 application lifecycle (mandatory design)

**Initialization sequence:**

1. Start WinMain; establish process DPI-awareness (`PER_MONITOR_AWARE_V2` where OS supports it), crash/log policies, COM on the UI thread as needed, and application directories.
2. Create the main Win32 `HWND`, initialize Direct3D 11 (hardware driver first) and DXGI swap chain, and create the main render target. Check DXGI capabilities and report unavailable device/feature levels cleanly. Do not require a discrete GPU.
3. Create exactly **one Dear ImGui context**; apply a pinned docking-enabled build; initialize official Win32 and DX11 backends. Enable `ImGuiConfigFlags_DockingEnable` and `ImGuiConfigFlags_ViewportsEnable`; validate both on the pinned revision. Create a fullscreen root dockspace.
4. Initialize `WorkspaceManager` with recoverable named **Production** and **Live** layouts; persist layout geometry separately from the audio project and remember panel visibility/zoom. Clamp saved floating window positions to connected monitors at startup.
5. Start engine/control/worker services independently of renderer; creating/restarting a D3D11 device must not reset the transport, unload plug-ins, corrupt projects, or stop audio.
6. Run a **message/event-driven UI loop**, handling Win32 messages, queued user actions, scheduler wakeups and visual invalidation. Build and render an ImGui frame only when the active UI update cadence requires it; cap expensive redraw work when hidden/minimized.
7. Render the main draw data through `ImGui_ImplDX11_RenderDrawData()`. For detached panels, use official multi-viewport callbacks via `ImGui::UpdatePlatformWindows()` and `ImGui::RenderPlatformWindowsDefault()`, with verified swap-chain behavior. Present in a paced manner; never busy-spin on an idle app.
8. Handle `WM_SIZE`, `WM_DPICHANGED`, display-change, device removal, monitor unplug, sleeping/resume and window reparenting. Recreate per-viewport render targets safely on the UI thread and rebuild font atlas when scale changes. Destroy ImGui/backends, D3D11 resources and native windows on shutdown after stopping UI callbacks.

**Essential invariants:**

- Do **not** create one ImGui context per floating window. Secondary viewports are additional native HWND/swap chains under one ImGui context.
- Use official backends unmodified initially. If a compatibility patch becomes necessary, isolate it, test it, pin the source diff and create an ADR.
- A monitor removed while a panel is detached must not strand the panel off-screen. A corrupted `.ini` layout triggers an accessible reset-to-default action rather than application failure.
- On driver/GPU loss, stop rendering, reset D3D11 resources and restore a minimal UI where possible; the audio engine continues unaffected. Software WARP is **diagnostic fallback only** and must not silently replace accelerated rendering in production benchmarks.
- Each Win32 API object/resource must have deterministic ownership; use RAII for COM pointers (`ComPtr`), handles and D3D11 resources. All HWND parenting and GPU calls occur on the correct thread.

**Native third-party plug-in editors:** A VST3-provided editor may expect its own Win32 host `HWND` and lifecycle; it is **not** an ImGui widget. Offer a separate native plug-in editor window per plug-in instance (with correct focus, keyboard routing, sizing and DPI) plus an always-available ImGui-generated generic parameter panel. Docking a foreign `HWND` inside a Dear ImGui panel is *not* a v1.0 commitment unless explicitly proved safe; this avoids unstable reparenting. Closing the editor never unloads or stops the plug-in's DSP. Keep editor instances owned by the plug-in manager, not by transient GUI draw code.

**Frame pacing target:** Foreground animation/editing refresh at 60 Hz if warranted, configurable 30/60 Hz; a largely idle stable workspace targets ~10–15 UI frames/second or less with event-based wakeups; minimized/headless UI performs no routine rendering. Meter values may be sampled at e.g. 20–30 Hz but diagnostics CPU/load charts at 1–2 Hz. These are *engineering candidates*, not blanket guarantees; verify UI responsiveness and GPU scheduling on real devices. Use `MsgWaitForMultipleObjectsEx` (or carefully measured equivalent) to sleep/wake; do not call blocking waits on the audio callback. Multiple detached swap chains must not cause unbounded Present calls or synchronization stalls.

**DPI, localization and access:** Per-monitor DPI-v2 with font atlas rebuild and stable logical-coordinate widgets; target 100–200% scaling, mixed 100%/150% monitors, keyboard navigation, consistent tab focus, readable labels, Win32 IME for non-ASCII project names, clipboard, high-contrast styles, selection state beyond color, and a keyboard-operable path for fundamental editing. Do not promise full Windows UI Automation accessibility from Dear ImGui out of the box; test with assistive tech and document gaps.

### 4.2 Audio processing graph

```
Audio device / MIDI input
           |          \
     Audio input      MIDI scheduler
           |          |
     Input track   Synth / Drum / VST instrument
           \          /
        Track clip reader / instrument
                    |
           Inserts / channel DSP
                    |
               Fader + Pan
                    |
           Track sends / Buses
                    |
           Master + limiter (opt-in)
                    |
            Audio backend output
```

Graph nodes have stable IDs, typed audio/MIDI/control ports, reported latency, optional processing tails, a prepared state, a deterministic frame processor, and a non-real-time teardown. Build a directed acyclic graph (DAG) and topologically sort it. **No arbitrary cycles.** At first, support mono and stereo paths; channel conversion and multi-output instruments have explicit adapters and later scope. Route track input to monitored output and recording independently to avoid feedback and double monitoring.

The **project model** stores *intended state*, and the **runtime graph** stores a compiled, efficient rendering representation. Never let a UI widget own a DSP object or modify its memory without synchronization. Build new graph snapshots off-thread and atomically activate on safe boundaries. For stateful nodes, preserve persistent node instances or migrate state explicitly; an edit must not restart all oscillators or tails. Protect old graph lifetimes with a proven epoch/retire queue or other callback-safe reclamation strategy. Avoid `shared_ptr` destruction in the callback.

### 4.3 Timing and event semantics

- Use an authoritative **64-bit sample-frame transport clock** plus a piecewise tempo/time-signature map; beats/ticks are converted to samples without accumulating floating-point drift. Use e.g. 1,920 PPQ in the project format, with tested round trips and a future resolution migration rule.
- The driver callback may request variable frame counts. Each call must process precisely its requested frames; internal fixed-size DSP processing can split into chunks. At changes within a buffer, split processing at sample-accurate event/tempo/loop boundaries.
- MIDI events carry source timestamp, target sample frame and event offsets; order note-offs vs note-ons deterministically at equal timestamps; guarantee reliable note-off and transport-stop `all notes off` paths, including bounded queue overload.
- Clip launching is queued with a documented policy: Immediate / next 1/4, 1/2, 1 bar, 2 bars (optionally other subdivisions). Same per-track slot exclusivity in Session Mode; explicitly define whether a launched session clip overrides arrangement for that track and when arrangement playback resumes.
- Recording aligns captured audio to transport using known input latency and measured offsets; avoid double-compensating driver offsets. Persist latency calibration and allow manual correction.
- Handle device sample-rate changes, unplug/replug, exclusive-mode failures, 32-/64-bit Windows sample formats, and unavailable interfaces without corrupting projects.

### 4.4 Real-time invariants

**First-party audio callback must not:** allocate or free heap memory; use blocking locks/mutexes/condition variables; sleep; do synchronous disk, network or console I/O; parse projects; load samples or plug-ins; update widgets; or perform unbounded scans. Preallocate scratch buffers, queues and voice pools. DSP buffers should use aligned contiguous memory and float32 samples; evaluate double precision *only where audible/measurement warrants it*, e.g. accumulation or time math.

- No logger flush in audio callback; bounded lock-free diagnostics buffer with a consumer elsewhere.
- Prefer single-producer/single-consumer bounded queues for audio-related messages; explicitly serialize multiple producers through a control thread where appropriate.
- Stream long recordings and samples using prefetch / write rings; handle queue exhaustion with an immediate, visible error and a recoverable partial take. Do not silently lose data.
- Optimize **after measurement**: avoid waking inactive modules, propagate silent-buffer flags, idle inactive voices, process only audible clips, cache waveform peaks, throttle offscreen UI updates to ~20–30 Hz.
- External plug-ins are not subject to the host's allocation guarantees. Mark plug-in-specific performance faults separately from the host's own DSP faults.

### 4.5 Persistence and portability

Project directory example:

```
My Song.ldaw/
  project.json                     # formatVersion, UUID, tracks, clips,
                                   # tempo map, routing, automation, MIDI, patches,
                                   # plug-in descriptors and opaque serialized states
  media/                            # optional copied original recordings/samples
  patches/                          # optional user-authored synth patches
  recovery/                         # rolling autosave snapshots
```

- Save to a temporary manifest, fsync/flush as appropriate, then atomically replace on the same volume. Preserve last-known-good backup.
- Audio recording writes to a temporary take file with journaling, promotes it to media when closed; after crash, present recoverable takes.
- Every item gets stable UUID, independent from UI position. Store media paths relatively where possible, canonical hash and original source hints for relinking.
- Never embed the plug-in binary. Serialize IDs/vendor/version, MIDI mappings, automation IDs, and binary preset/state blobs in an encoded project subresource or dedicated file; restore after plug-in graph is prepared.
- Unknown plug-in or missing media should display placeholders and retain serialized state, not remove that data on save.
- Autoresave must not stall audio, and autosaves must never overwrite the user's deliberate Save As target.
- The portable app uses user app-data by default; a clearly documented `portable.flag` mode can put settings/cache beside the EXE *only if the directory is writable*. Projects remain user-selected; never assume Program Files is writable.

### 4.6 DSP and mixing correctness

- All track processing receives format-neutral audio and timestamped MIDI buffers; same nodes work for real-time and offline render.
- v1.0 mixer: gain (linear in DSP, dB in UI), constant-power stereo pan, mute/solo policies, mono/stereo metering, send/return buses, master clip indication, optional safety limiter, track/bus routing validation.
- Implement **plug-in delay compensation (PDC)**: nodes report latency, graph computes latency along every relevant signal path, inserts bounded delay buffers for coherent summed paths; recompute safely when plug-ins change latency. Document what is/not compensated for *live monitoring* to preserve low-latency mode.
- Tempo-synced delay uses the tempo map, supports parameter smoothing and proper feedback limits. Default basic EQ, filter, delay and dynamics must have unit tests (NaN/inf, denormals, discontinuities, channel consistency).
- Offline bounce must not drop pending plug-in tails or freeze effects early; rendered length/tail policy is explicit. For integer PCM exports, apply optional correct TPDF dither *only when quantizing/reducing word length*, with user control and true-peak warnings if calculated.

### 4.7 External hardware definitions

“USB support” means compatibility with Windows-class-compliant or vendor-driver devices, **not** a homegrown USB audio stack. Microphones, electric instruments and XLR microphones ordinarily connect via an audio interface (USB or other driver), while MIDI keyboards/controllers connect as MIDI devices. Expose device/channel selection, gain/meter guidance and monitoring; do not promise the DAW itself controls hardware preamp gain or supplies phantom power.

## 5. Editor design: custom Dear ImGui experience

### 5.1 Workspaces and panel system

**Root dockspace:** main menu + persistent top transport bar (play/stop/record, loop, tempo and time signature, metronome, count-in, device/audio status, *separate process CPU and audio deadline-load indicators*). Left library; center Production timeline *or* Live session grid; right inspector; bottom piano roll/sample/effects/synth; optional mixer and diagnostics panes. Users may dock, split, tab, close and detach panels onto a second monitor. A factory layout and **Reset Workspace** exist for each mode; changing modes does **not** rebuild the engine, restart audio devices or unload rack/plug-in nodes.

**Persistence ownership:** `.ini` / user settings store display/panel geometry, selection, zoom and view options; project manifests own musical content, routing, clips, automation and plugin state. Use stable panel IDs and migration hooks when UI labels change. Preserve a floating panel's logical workspace context but recover it if monitor layout changes. Support saving/loading named workspaces, including mixed-DPI multi-monitor setups.

### 5.2 Drawing and interaction widgets

| Editor region | Custom behavior and minimum tests |
|---|---|
| Timeline ruler | Musical bars/beats/PPQ, sample frames, snap and zoom; pixel-to-sample mapping is monotonic and tested at >2 hours/96 kHz |
| Waveforms | Worker-generated multiresolution min/max/RMS peak pyramids, clipped visible-range draw calls via `ImDrawList`; source audio untouched. Offscreen tracks are not tessellated. Seek, drag, trim, split and fade gestures emit undoable project commands |
| Piano roll | 2D zoom/pan, note resize/move, velocity, selection, snap/quantize, mouse audition, virtual keyboard and visible note range culling; notes stored in model, never only in ImGui widgets |
| Mixer | Smooth faders with dB scales, pan knobs, mute/solo/arm, peak/RMS/clip meters, effect slots, sends, routing popup and explicit drag types; peak/clip state derives from snapshot telemetry |
| Drum machine | 16 trigger pads, step grid, velocities, pattern length and swing; playback events scheduled by the engine, not `ImGui::GetTime()` |
| Modular rack | Nodes/pins/Bezier cables; typed audio/control/note inputs, validation preview, detachable patch links, pan/zoom; use a pinned compatible `imgui-node-editor` extension only if its requirements match our exact ImGui tag |
| Diagnostics | CPU%, audio deadline p50/p95/p99/peak, XRUN history, process memory, audio device and driver, UI FPS/CPU/GPU time; optional pinned ImPlot graphs, capped history buffer |
| File browser | In-app folder tree, search, favorites, preview and drag-drop, backed by asynchronous filesystem enumeration. The Windows native file dialog is used for open/save/export destinations |
| Plug-in windows | ImGui generic control panel plus dedicated native HWND host for third-party GUI; isolate keyboard focus and prevent double-handling transport shortcuts while typing inside plug-in |

**UI architecture contract:** Widgets are stateless views over small immutable model/telemetry snapshots and their own harmless view state. User gestures enqueue typed commands to `core/commands`; the control thread validates and applies them. The UI never owns, locks, starts or destroys a DSP graph while audio is processing. Long lists use visible-range clipping (`ImGuiListClipper` where appropriate), draw-list batching and incremental waveform data uploads. Avoid massive immediate-mode loop costs for hidden or scrolled-away items.

### 5.3 Production Mode

Horizontal arrangement with mono/stereo audio and MIDI lanes; ruler, loop markers, tempo regions, aligned waveforms and clips. Editing tools include select, drag, trim, split, duplicate, slip (audio source offset), fade, crossfade, snap, track resize and nondestructive undo/redo. Inline automation lanes and inspector are available by Phase 11. Marker and timeline positions follow the authoritative audio transport sample clock, *not UI frame time*. UI playback cursor can interpolate between timestamped snapshots but must resynchronize after seek/loop.

### 5.4 Live Performance Mode

Tracks as columns and scenes as rows. Clip slots show empty/queued/playing/recording/stopped; clip and scene launching honors the sample-accurate quantized scheduler. Pads/controllers map via MIDI Learn. Panic/Stop All always visible. Switching to/from Production Mode preserves track FX, instrument instances, graph state and live performance. Define session-vs-arrangement priority per track and capture launches with original timing to the arrangement. The UI is a controller, not the timekeeper.

### 5.5 Modular rack

User creates modules (sine/saw/square/triangle oscillator, noise, mixer, ADSR, LP/HP filter, LFO, VCA, MIDI note/gate, attenuator, output) and draws patch cables between compatible ports. Validate port direction, type and graph cycles in a **separate rack compiler**, outside the UI thread and callback. Provide a one-click playable factory patch, polyphony setting (8 voices initial), CPU/node complexity feedback, deterministic patch save/load, disconnect/remove undo, and clear error messages. Typed graph connections are the serialized truth; screen-space wire geometry is presentation only. Unsupported feedback connections are rejected.

### 5.6 Professional interaction standards

Keyboard shortcuts: Space Play/Stop, Ctrl+S Save, Ctrl+Shift+S Save As, Ctrl+Z/Ctrl+Y Undo/Redo, Delete, Ctrl+C/V, timeline zoom/scroll with expected modifiers, unambiguous focus behavior. Shortcut routing respects text fields, filenames, plug-in editor focus and native dialogs. File Explorer drag/drop works with non-ASCII Windows paths. Standard OS resize/maximize, multiple monitors and DPI changes must not leave the UI clipped or unreadable. Selection and armed/monitoring states must be obvious independent of color. ImGui source code/theme assets may not be mistaken for an accessibility or branding solution; supply original visual assets and document remaining limitations.

### 5.7 Renderer and redraw performance rules

- No loop that calls `Present` as fast as CPU allows. Favor bounded frame pacing and event-driven sleep; disable routine redraw when minimized or fully occluded if feasible.
- Do not redraw or GPU-upload entire hour-long waveforms on zoom, scroll, meter change or live clip launch. Maintain levels of detail, cache within bounded RAM, and rebuild previews asynchronously.
- Retain ImGui context and native windows across user workspace switches; only update visible panels and layout state.
- Watch **UI process CPU and GPU time separately** from audio deadline use. A UI CPU optimization cannot be declared successful if it introduces dropouts, command latency, visual stalls or GPU thrashing.
- Primary test layouts: single 1080p monitor; dual monitors (one 1080p + one 1440p); mixed 100%/150% scaling; detach, redock, unplug, reconnect, minimize and restore during active audio playback. When only one monitor is available, simulated tests are supplemental, not a substitute for physical qualification.

## 6. Audio and file compatibility matrix

| Item | v1.0 requirement | Later enhancement |
|---|---|---|
| Internal audio engine | float32, stereo/mono, 44.1/48/96 kHz, adaptable hardware buffers | 192 kHz qualification after benchmarks |
| Hardware buffer | Request device-supported 128/256/512/1024 frame sizes; default 256 or nearest supported | Adaptive low-latency mode |
| Driver | WASAPI shared first; exclusive as qualified; ASIO required before v1.0 sign-off after license + hardware tests | Specialized drivers / Windows MIDI Services adapter |
| MIDI | MIDI 1.0 note, velocity, CC, program change, pitch bend, sustain and channel handling | MIDI 2.0, MPE, SysEx routing enhancements |
| Audio import | WAV, AIFF, FLAC, Ogg Vorbis where decoder vetted; MP3 and M4A/AAC via Windows Media Foundation when the runtime codec is present; mono/stereo | Expanded codec testing, metadata and additional container support |
| Audio export | WAV PCM 16/24, WAV 32-float; FLAC/AIFF where supported; 44.1/48/96k; master/selection/stems; tails; normalization toggle; dither | MP3 and further formats |
| MIDI file | Import/export SMF type 0 or 1, with tempo and signature metadata where applicable | Advanced mappings/round-trip edge cases |
| Plug-ins | VST3 instrument/effect on x64, params, states, editor, automation, I/O, PDC | CLAP and optional bridge/sandbox |
| Session interoperability | Internal clip launching and capture; no proprietary DAW project formats | Standardized interchange investigation |

*Interoperable finished exports* are distinct from editable projects: do not promise that an `.ldaw` project opens in a different DAW. Deliver WAV/FLAC/stems and MIDI for exchange.

## 7. Performance budget and telemetry

The user's priority is **low CPU use**, so every phase must run a benchmark suite on a *recorded reference machine*, not a vague “optimized” claim.

**Reference target:** Windows 10/11 x64, an actual 4-core CPU, 8 GB RAM, an SSD, a stable audio interface/driver, 48 kHz, 256-frame period (approximately **5.33 ms**). Record processor model, power plan, audio device, driver version, Windows build and DLL/plug-in versions. Driver support for 256 frames cannot be assumed; use actual negotiated size and annotate results.

**Targets (engineering acceptance goals, not universal guarantees):**

1. Blank app, settled for 60 seconds: ≤3% *total-system CPU attribution to app process* on the recorded four-core reference host; ≤450 MiB working-set memory. These are app targets, not guarantees for every GPU/driver/plug-in combination.
2. Reference native session: eight streaming stereo tracks, one eight-voice synth, one 16-pad drum machine playing a modest pattern, basic EQ or filter on four tracks, 48k/256: audio processing **p99 ≤50%** of the measured per-buffer deadline, hard max below 80% excluding clearly tagged driver or external plug-in stalls, with **zero app-attributable xruns in 30 continuous minutes**.
3. UI interaction while playback continues: scrolling, zooming, opening panels and browser previews must not cause audio-thread allocation, blocking wait or app-attributable xruns.
4. Long-running live smoke: 60 minutes of repeated scene launches and controller input without event loss, stuck notes, memory growth that continues after warmup, crash, or audio-device restart.
5. Zero first-party audio-callback heap allocations and blocking system calls under instrumented tests; monitor actual audio-thread execution time rather than inferring real-time safety solely from CPU%.
6. Process CPU, audio load and RAM UI updated once or twice per second; peak callback load and xrun counter include a timestamp and `device / engine / external plug-in / unknown` classification when determinable.

**CPU metrics must not be conflated:** process CPU% is OS-level compute use over a sampling window; audio-thread load is callback compute time divided by **actual** callback deadline (which can vary). Display both. For CPU percentage specify whether denominator is one logical processor or the whole system. Sample callback timings sparingly if measurement itself raises load. Benchmarks establish a baseline at each accepted phase and fail regression gates when median/p99 worsens by >15% without a recorded, reviewed reason.

**Renderer-specific budgets and regression checks:** At Phase 00, measure idle, foreground-active, minimized and one-detached-window modes separately. Record ImGui frame time (build/draw), D3D11 present time, number of draw calls, waveform cache memory, and CPU spent on UI thread. In a static project with idle transport, measure whether the UI actually sleeps and avoids pegging a CPU core. Later phase tests include a 120-minute/100-track visually complex fixture for scrolling/zooming *UI responsiveness only* (not a promise of 100 concurrent audio tracks on minimal hardware). Mixed-DPI and detached window tests must not cause sustained redraw spikes. Any UI benchmark must list its GPU and number of monitors.

**Optimization hierarchy:** (a) remove unexpected work from callback, (b) fix render graph and event timing, (c) cache/stream media, (d) skip inactive processing and precompute coefficients, (e) SIMD where profiled, (f) optional multi-core graph processing only if deterministic scheduling and deadline overhead are justified. Avoid adding background indexing or GPU UI complexity before baseline metrics are known.

## 8. Phased development and build acceptance

Every phase ends with `builds/phase-NN/LightweightDAW.exe` packaged with runtime libraries in `LightweightDAW-phase-NN-win64.zip`, project fixtures, a test report, and a tag `phase-NN-accepted`. If not built *and executed* on a Windows machine or runner, state **UNVERIFIED**, not “working.” For phases with hardware dependencies, include both automated tests and an explicit physical-device manual verification record.

### Phase 00 — Build infrastructure and functioning native ImGui shell

**Implement:** Git repo, GPL license/third-party audit, locked dependencies, MSVC+CMake Windows CI, native `WinMain`, `HWND` / Win32 message pump, D3D11 device and DXGI swap chain, one Dear ImGui context (pinned **docking** version), official Win32/DX11 backends, root dockspace, menus, About, Preferences and Diagnostics windows, named Production/Live layouts, docking and multi-viewport enabled. Basic native Open/Save file-dialog test; initial DPI-awareness and multi-monitor placement handling. Add UI frame scheduler that sleeps when idle. Add headless test executable; no fake audio or misleading CPU numbers.

**Working build:** Launch portable `LightweightDAW.exe`; visibly drag/dock/detach panels and move to second monitor where available; toggle workspaces; reset layouts; see genuine CPU/RAM and UI frame timing; exit and relaunch with saved layout. Audio status says `not initialized`.

**Acceptance:** clean Release build + CTest pass; extracted ZIP launches on target Windows without developer PATH; no Qt DLLs, include runtime/third-party notices; native dialogs and drag/drop smoke test; no max-speed idle render loop; zoom/DPI 100%/150% with no unreadable UI; at least one physical dual-monitor manual case to claim fully verified multi-viewport behavior (otherwise BLOCKED); closing detached window does not terminate app; D3D11 resize and minimize/restore safe; measured startup CPU/RAM and idle frame profile. Tag accepted only when actual Windows execution evidence exists.

### Phase 01 — Real audio callback, test tone, metronome, diagnostics

**Implement:** Device enumeration, WASAPI shared output, format negotiation, audio callback, lifecycle, float32 stereo output, preallocated buffer pool, transport sample clock, 120 BPM 4/4 metronome (toggle, accent), simple sine test tone; callback/xrun and CPU meters. Distinguish device callback underruns from app render misses when known.

**Working build:** press Play and hear timing-accurate metronome or tone; change output device and restart transport safely.

**Acceptance:** audio plays at available supported 44.1/48k; 128/256/512 buffers where device permits; no first-party callback heap allocations; correct beat spacing in offline loopback test; survive ten stop/restarts; 30-minute xrun smoke and baseline CPU report. A missing physical output device must produce a clear message and offer offline test mode, not a crash.

### Phase 02 — Audio track arrangement, import and playback

**Implement:** Core `Project/Track/Clip/MediaSource` objects, a custom clipped ImGui/ImDrawList timeline waveform/clip widget, waveform peak cache, local WAV/AIFF/FLAC import (as vetted), and Windows Media Foundation MP3/M4A import where system codecs are available, drag-and-drop, zoom, selection, move/trim/split/duplicate, play/stop/seek/loop, basic gain and pan, snap to beats, clip-boundary micro-fades. Prefetch long samples from worker thread.

**Working build:** drop an audio file onto a track, move/trim it, play it from any timeline point, loop a region.

**Acceptance:** tested mono/stereo audio imports at native and resampled rates; no UI stall for large sample; no click at a properly faded split; measured intertrack alignment with identical files; 10-minute playlist no callback disk I/O. Non-destructive edits leave source media unchanged.

### Phase 03 — Browser, project save/load, undo and recovery foundation

**Implement:** project schema v1 + migration entrypoint, native Win32 open/save dialogs, new/open/save/save-as, path relinking, asynchronous in-app folder browser with preview and thumbnails, stable asset IDs, undo/redo command journal for track/clip operations, autosave timer and recovery selector, missing file placeholders.

**Working build:** create a multi-clip project, save to folder, close the application, reopen and continue editing.

**Acceptance:** project round-trip diff (audio/mixer/clip timing) deterministic; kill app mid-save without corrupting last good project; moving project directory works with relative media; unplugged file media preserved as placeholder; recovery presents rather than silently replacing original; ZIP example session supplied.

### Phase 04 — Audio capture and physical input devices

**Implement:** input device/channel selection, mic/instrument through Windows audio interface driver, track arm, input meters, mono/stereo record-to-disk, safe monitoring, latency metadata, overdub/new takes, count-in, loop record (simple separate takes), device loss recovery. Develop the ASIO adapter only after license review; if not ready in this phase, track it explicitly as a v1.0 release blocker, not an optional feature.

**Working build:** record a real microphone or interface instrument as an audio clip, play it back, overdub onto second track.

**Acceptance:** record 15 minutes and recover file correctly; verify no mismatched sample rate; input signal does not accidentally double-monitor; disconnection handled without project corruption; measured playback/record alignment via loopback and manual offset correction; recover interrupted take where possible. Manual USB microphone and USB interface matrix recorded.

### Phase 05 — MIDI engine, piano roll, basic MIDI instrument

**Implement:** MIDI device enumeration via RtMidi, input time stamping, stable note scheduler, external MIDI clock/output deferred, custom ImGui piano-roll grid with notes, velocity, move/resize, quantize, transpose, MIDI clip loop; simple built-in sine/saw playable instrument with ADSR; note on/off, CC sustain and pitch bend; MIDI import/export SMF 0/1.

**Working build:** connect USB MIDI keyboard (or mouse piano roll), record MIDI notes, edit, hear synth playback, export and re-import MIDI.

**Acceptance:** sample-offset MIDI timing under fixed synthetic input; all-notes-off on Stop/device loss; 60-minute play/stop no stuck notes; round-trip 3-track MIDI fixture preserves timing within defined PPQ quantization; note editing undo. User controls MIDI latency offset, not an arbitrary hardcoded fudge factor.

### Phase 06 — Drum machine and pattern sequencing

**Implement:** custom ImGui 16-pad/step-grid widget and sampler with sample import, pad assignments, channel output, velocity, ADSR amplitude, mute/solo, 16/32/64-step pattern grid, swing and per-step velocity, sequencer resolution and pattern to MIDI/audio engine; clips on drum track; pad input mapping from controller.

**Working build:** create a basic percussion arrangement, audition pads, edit steps and pattern lengths, play alongside existing audio tracks, save/reload.

**Acceptance:** deterministic trigger times across loop edges and tempo changes, no clicks from voice steal (short declick), no missing steps in swing tests, CPU profile with 16 samples and 64 steps. All factory demo sounds must be original, public-domain verified or clearly licensed for redistribution.

### Phase 07 — Full mixer, routing, internal effects, monitoring

**Implement:** custom ImGui mixer panel, buses, sends/returns, pre/post-fader choice, track input/output selectors, mute/solo semantics, faders/pan/peak/RMS meters, EQ, filter, delay, compressor and optional limiter; routing cycle detection; low-latency monitor bypass for latency-heavy nodes later.

**Working build:** build and route a mix with parallel sends and master effects; solo/bypass and mixer changes operate during playback.

**Acceptance:** impulse and gain-unit DSP tests, no NaN/Inf from automation extremes, solo-safe behavior documented, routing changes glitch-safe, clipping and output peaks reported, 8-track benchmark stays within agreed budget.

### Phase 08 — Modular synthesizer rack

**Implement:** graph compiler for modular nodes separate from presentation, typed graph model with custom ImGui node/cable UI (pinned `imgui-node-editor` only after license/compatibility audit), typed audio/control/note connections, oscillator/noise, 2-input mixers, filter, ADSR, LFO, VCA, control attenuator, output; voice manager 8 voices default, per-voice vs global modules; patch save/open, presets with original or compatible content, virtual keyboard. Guard CPU limits with maximal nodes/voices.

**Working build:** draw cables from oscillator to filter/VCA/output, modulate cutoff with LFO/envelope, play polyphonically with MIDI keyboard or piano roll, save patch and reopen.

**Acceptance:** invalid port connection and cycle rejection tested; test silence on disconnected output; no divide-by-zero/denormal storms; patch round trip reproduces deterministic offline output for deterministic modules; voice stealing cleanly releases; 8-voice benchmark recorded.

### Phase 09 — VST3 host and plug-in state

**Implement:** VST3 SDK integration, separate discovery executable, scan cache/quarantine, instantiate effect and instrument plug-ins, audio bus/parameter/event exchange, **dedicated native Win32 plug-in editor host window outside ImGui plus ImGui generic parameter editor**, vendor/version identity, plug-in state round-trip, automation plumbing, bypass, transport/context data, PDC, latency-change handling, clear missing plug-in placeholders. If sample-accurate parameter automation is incomplete, document the gap and gate v1.0 on adding it.

**Working build:** load at least one compatible freely redistributable test VST3 effect and one instrument, play MIDI through the latter, modify parameters, save/reopen project, render output.

**Acceptance:** validate host against Steinberg minimum-host requirements; test plug-in discovery timeout, malformed/failed scanner, missing plug-in, binary architecture mismatch, native HWND editor close/reopen and DPI/focus/keyboard routing, generic fallback, state restore, bypass, latency PDC impulse alignment, UI thread responsiveness and detached editor window positioning. An in-process plug-in crash may terminate host: record limitation; the scanner must not crash the editor. Never ship proprietary VST plug-ins in tests.

### Phase 10 — Live performance mode and MIDI mapping

**Implement:** custom ImGui session clip/scene grid on shared project, launch queue and quantization, stop/stop-all, scene status, one session clip per track at a time, playback/arrangement priority policy, MIDI Learn for launch/mute/fader, high-contrast indicators, no audio device restart between modes, capture session changes into arrangement (including clip launch times).

**Working build:** perform a song by launching audio and MIDI clips/scenes from keyboard/mouse or MIDI pads while the mixer and VST3 devices continue playing; record the performance back to timeline.

**Acceptance:** 1-hour repeated scene launch test, launch occurs at defined boundary within sample/beat tolerance, no stuck notes on scene switch/Stop All, no missing mapping after reopening, switch views under load without app-induced xrun, captured arrangement reproduces performance launch chronology and song timing.

### Phase 11 — Automation, polished editing, resampling and freeze

**Implement:** breakpoint automation lanes for volume/pan/send/plug-in params/synth controls, read/touch/write modes (read/write are mandatory; touch may be deferred only with a documented v1.0 scope decision), MIDI CC mapping, edit/time-selection refinements, clip fades/crossfades, sample-rate conversion and **offline cached tempo-stretch of audio loops** (evaluate GPL-compatible Rubber Band Library; run processing on workers, with original preserved), offline freeze/bounce-to-clip for expensive instruments and plug-ins, undo/redo of automation and freeze.

**Working build:** automate filter/fader changes, record controller changes, freeze a CPU-intensive track, unfreeze and edit, crossfade adjacent clips.

**Acceptance:** parameter timestamps correct through loop boundaries and punch, no zipper noise for smoothed first-party parameters, frozen vs original audio null-test within deterministic DSP tolerance (except non-deterministic plug-ins), unfreeze restores original state, freeze reduces callback CPU in benchmark; tempo-fit a 100-BPM source loop to a 120-BPM session while retaining its original pitch within measured tolerance and without real-time reprocessing on each launch.

### Phase 12 — Export, delivery polish, reliability and v1.0 release

**Implement:** qualified ASIO interface support if not already accepted; robust offline rendering (master/selection/stems, tail inclusion, fast/realtime fallback for plug-ins), format/bit depth/sample rate selector, normalization (optional), dither, optional loudness/true-peak readout if measurable; help and keyboard cheat sheet, first-run device wizard, accessibility pass, error/report bundling, packaging, installers (e.g. NSIS subject to license review), release docs and source distribution.

**Working build:** create a complete original track in production or live mode, save it, exit/reopen, export WAV and FLAC (plus AIFF if approved), use generated stems and MIDI in other audio software.

**Acceptance:** reference end-to-end demo recipe passes on Windows 10 and Windows 11, including a tested real ASIO interface and native Win32 multi-viewport monitor/scale matrix; tests from phases 00–11 rerun, 30-minute benchmark and 60-minute live soak, project recovery scenarios, bit-accurate or tolerance-bound export test, license/notice audit, fully extracted portable ZIP runs without development installation, optional installer install/uninstall leaves user projects intact. Sign release only if a legitimate signing certificate is available; unsigned releases must be clearly disclosed.

### Phase 13 — Post-v1: CLAP and modular extensions

**Implement:** CLAP host adapter and tests, sample-accurate parameter events, transport and port metadata, state/GUI, host extensions subset, PDC; existing v1.0 VST3 project compatibility preserved. After this, consider advanced modular modules, MPE/MIDI 2.0, extended codecs and optional plug-in process isolation.

**Working build:** run a verified compatible CLAP instrument and effect alongside existing VST3 projects; reopen/export correctly.

**Acceptance:** run CLAP validation/test plug-ins, parameter and state tests, multi-format benchmark, migration test, project compatibility retained, xrun/p99 regressions within approved limits.

## 9. Dependencies and release gates

- Dependency order is deliberate: output device/clock → audio clips and project state → input capture → MIDI → drums → mixer → rack → VST3 → live launches → automation/freeze → final export/release → CLAP.
- Features may be simplified to pass a phase, but **do not mark a phase accepted with broken/placeholder behavior**. Defer optional scope with explicit ADR; retain all mandatory v1.0 features.
- Test binaries must be built in **Release** configuration for user acceptance; Debug binaries may accompany for investigation only.
- Each accepted build includes a `README_FIRST.txt` with launch instructions, supported hardware, test steps, exact build tag, known issues and licensing notices.
- Any fixed-phase bug discovered later becomes a regression test. Previous test projects must stay loadable via versioned migration.
- Do not skip test execution by reading source code and asserting it “should work.” If hardware is unavailable, mark device tests BLOCKED and the phase **not fully accepted**.

### Cross-phase acceptance scenarios

**A. First launch:** extract ZIP and run .exe → audio output chosen, no crash, clean UI with 8 GB system RAM.  
**B. Studio playback:** import 3 WAVs at different original sample rates, arrange, mix, seek, loop, apply fades, save/reopen.  
**C. Recording:** input mic into armed track, record for 10 minutes, save, reopen, recover from simulated interrupted take.  
**D. MIDI:** record keyboard riff, edit notes, play via built-in rack and VST3, export SMF and reimport.  
**E. Performance:** launch 4 scenes with audio + MIDI, switch modes without dropouts, capture performance and export.  
**F. Plugin resilience:** VST3 scan stalls/crashes in helper, editor survives and quarantines plugin; missing plugin state preserved.  
**G. Audio integrity:** test alternating sample rates, buffer sizes, unaligned clip edges, tempo changes, plugin delay, fades and tails against golden captures/impulses.  
**H. Performance:** repeated 30-minute reference mix with telemetry and one-hour live soak, exact system settings and results attached.  
**I. Recovery:** crash mid-save and mid-recording; last saved project and any recoverable recording still accessible.  
**J. Export:** 24-bit stereo WAV at 48 kHz, 16-bit with TPDF dither, 32-bit float, optional FLAC, stems and MIDI verified by an independent reader.

## 10. Formal definition of done (applies to every phase)

A phase is **ACCEPTED** only if:

1. All mandatory phase deliverables are implemented and mapped to tests.
2. Source builds cleanly from a fresh checkout using documented, pinned dependencies on Windows.
3. Release `.exe` exists and was executed. Its complete portable directory was zipped; actual SHA-256 checksums recorded.
4. Automated unit, integration and offline audio tests pass; manual device tests are performed when required.
5. CPU, audio load, memory, xruns and timing are recorded on named hardware/configuration; regressions are analyzed.
6. No critical data-loss, hard crashes in core workflows, or stuck MIDI note regressions remain unaddressed.
7. GPL notices, Dear ImGui/extension notices, Windows runtime deployment, dependencies, asset licensing and SDK obligations are verified.
8. Project files from every prior accepted phase remain loadable or are deterministically migrated.
9. `docs/PROJECT_STATE.md`, `NEXT_TASK.md`, `HANDOFF.md`, `TEST_RESULTS.md`, `KNOWN_ISSUES.md`, `BUILD_MANIFEST.json` and `DECISIONS.md` are current.
10. The user can perform the phase's named use case from the packaged build without developer tooling.

**Status labels:** `PLANNED`, `IN_PROGRESS`, `IMPLEMENTED_UNVERIFIED`, `BLOCKED`, `ACCEPTED`, `REGRESSED`. Never use `ACCEPTED` without measured evidence.

## 11. Developer/AI operating rules

- Do not write thousands of lines in one pass without compiling. Make small vertical slices; compile and test every meaningful subsystem before integrating the next.
- Never rewrite working engine/editor modules to add a minor feature unless justified by a failing test or ADR. Minimal patches first.
- Treat thread boundaries, real-time invariants, project schema, stable IDs and plug-in ownership as architectural contracts.
- Plan before coding, but produce code + tests + a runnable artifact in the same engineering phase. UI-only prototypes are not acceptable stand-ins for audio behavior.
- Never silently downgrade acceptance criteria, change licensing, switch frameworks, import proprietary assets or add commercial SDKs.
- If dependent hardware or Windows execution is unavailable, continue unit/offline work, identify the blocked manual test, and report the build as **UNVERIFIED**, without inventing a pass.
- Do not promise audio correctness based on user-interface animation. Proof must include recording/playback or test samples.
- Security: treat external plug-ins and audio files as untrusted. Limit parser payload sizes, validate file paths and sample count computations, fuzz media/project parsers when practical, and keep scan crashes outside main process.
- Use original/licensed code and sample assets only; document SPDX IDs and redistribution rights.
- End each phase with a compact facts-only handoff rather than requiring another AI to reread conversation history.

## 12. Low-token cross-chat handoff protocol

The next AI session reads **only three short files initially**:

1. `docs/PROJECT_STATE.md` — ≤1,200 words; current accepted phase, state of code, major fixed decisions, exact build instructions, verified current behavior, known blockers.
2. `docs/NEXT_TASK.md` — ≤500 words; exactly one next phase/subtask, prerequisites, files to modify, technical acceptance tests and required output artifact.
3. `docs/HANDOFF.md` — ≤750 words; current commit/tag, last verified Windows build path and SHA256, tests passed/blocked, performance baseline, new issues, short advice for next engineer.

Read `MASTER_SPEC.md` *only when referenced by section and necessary for a disputed requirement*, then read specific source files/tests. Avoid re-reading full chat history. Keep other context in the repo for targeted lookup. A new AI must inspect repository reality rather than blindly trusting a handoff.

### `PROJECT_STATE.md` mandatory template

```markdown
# Project State
Spec: v1.0 | Current branch: ... | HEAD: ...
Accepted phase: NN (tag phase-NN-accepted)
Active phase/task: ... | Status: ...
Toolchain/deps: MSVC ..., CMake ..., ImGui docking commit/tag ..., DX11/Windows SDK ..., audio SDK versions ...
Verified builds: Windows x64 artifact path and SHA-256 ...
Implemented today (verified): ...
Key invariants: realtime callback; graph snapshots; schema version; licenses ...
Build command: ...
Test command: ...
Benchmark: machine/device/48k/actual buffer/CPU p99/xruns/RAM ...
Blocking issues: ...
Decisions changed since last handoff: ADR-...
Next file: docs/NEXT_TASK.md
```

### `NEXT_TASK.md` mandatory template

```markdown
# Next Engineering Task
Phase / issue: ...
User-visible outcome: ...
Scope (maximum 3 bounded deliverables): ...
Out of scope: ...
Files/subsystems likely affected: ...
Dependencies and migration needs: ...
Acceptance tests (automated + physical where needed): ...
Required packaged output: builds/phase-NN/..., ZIP, SHA256, log
Failure/rollback policy: do not replace last accepted tag/build
```

### `HANDOFF.md` mandatory template

```markdown
# Continuation Handoff
Commit: ... | Accepted tag: ... | Working tree clean? ...
This session completed (verified): ...
Build artifacts: ... | SHA256: ...
Commands actually executed + results: ...
Runtime/manual verification actually done: ...
CPU/audio load/xruns/RAM baseline delta: ...
Known failures, hardware missing, and precise repro: ...
Changes to interfaces/schema/ADR: ...
Next action: read NEXT_TASK.md then execute; do not skip tests.
```

### `BUILD_MANIFEST.json` minimal machine-readable schema

```json
{
  "phase": "00",
  "status": "IMPLEMENTED_UNVERIFIED",
  "commit": "<git-sha>",
  "toolchain": {"compiler": "MSVC", "compilerVersion": "<actual>", "imguiDockingVersion": "<actual>", "graphics": "Win32+D3D11"},
  "artifact": {"path": "builds/phase-00/LightweightDAW.exe", "sha256": "<actual>"},
  "testRun": {"hostOS": "<actual>", "dateUTC": "<actual>", "passed": 0, "failed": 0, "blocked": []},
  "audioBenchmark": {"device": "<actual>", "sampleRate": 48000, "frames": 256, "p99CallbackLoadPct": null, "xruns": null, "workingSetMiB": null}
}
```

**Never leave placeholder values in an accepted manifest.** `null` is permitted for an inapplicable metric, with a written explanation. An unavailable test does not equal a passed test.

### Standard prompt for a new AI development chat

> You are the lead C++20 Windows DAW engineer continuing an existing free/open-source project. Read `docs/PROJECT_STATE.md`, `docs/NEXT_TASK.md`, and `docs/HANDOFF.md` first, then inspect the relevant code and tests. The master specification is `docs/MASTER_SPEC.md`; use it by section only when needed. Implement the next bounded task without rewriting verified features. Honor real-time callback safety, approved GPL-compatible dependencies, reproducible Windows x64 CMake builds and mandatory phase acceptance. Compile and test actual changed code. Produce a portable `.exe` distribution, report actual tests and performance evidence, update the three handoff files and build manifest, and never call an untested build accepted. If the environment cannot execute Windows binaries or test a physical audio device, mark those checks blocked rather than inventing them. Do not ask me to re-explain prior discussions; use the repo files.

## 13. Initial implementation instructions for Phase 00

1. Establish `GPL-3.0-or-later` application license, original repo name and `THIRD_PARTY_NOTICES.md`; verify exact tags/commits and licenses of Dear ImGui docking build, optional ImPlot/imgui-node-editor, RtAudio, RtMidi, libsndfile, libsamplerate, VST3 and ASIO before pinning. GUI code must not depend on Qt. Do not fetch random binary DLLs from unverified mirrors.
2. Create CMake x64 Release/Debug presets for MSVC 2022; build Win32 + D3D11 + Dear ImGui docking shell, two preset layouts, a floating dockable panel, diagnostics pane, Win32 file dialog, basic DPI handling, idle frame pacing, settings/logging, CTest, portable packaging and Windows CI.
3. Add `IAudioBackend`, `IAudioProcessor`, `IMidiBackend`, `IPluginHostAdapter` interface declarations with no simulated functioning audio; provide stub display of “not initialized.”
4. Build on Windows; package runtime DLLs, fonts/assets and license notices; extract and run ZIP on a clean system/VM. Measure idle/foreground/minimized CPU/RAM, DPI and detached-window behavior. If no Windows executor is present, stop at `IMPLEMENTED_UNVERIFIED` and list precise missing evidence.
5. Commit/tag only after definition of done; generate `docs/PROJECT_STATE.md`, `NEXT_TASK.md` (Phase 01), `HANDOFF.md`, `TEST_RESULTS.md`, and `BUILD_MANIFEST.json`.

## 14. Researched reference materials (reviewed 2026-11-04)

Primary vendor/upstream sources supporting the architecture. Pin exact SDK revisions and independently recheck licenses before distribution.

1. Dear ImGui upstream repository (MIT), official `imgui_impl_win32` and `imgui_impl_dx11`: https://github.com/ocornut/imgui ; https://github.com/ocornut/imgui/blob/master/docs/BACKENDS.md
2. Official Dear ImGui Docking guide: https://github.com/ocornut/imgui/wiki/Docking ; Multi-Viewports guide: https://github.com/ocornut/imgui/wiki/Multi-Viewports
3. MIT node editing UI candidate: https://github.com/thedmd/imgui-node-editor ; MIT diagnostics plotting candidate: https://github.com/epezent/implot
4. Windows native Common Item Dialog (preferred to old `GetOpenFileName`): https://learn.microsoft.com/en-us/windows/win32/shell/common-file-dialog
5. Microsoft WASAPI / IAudioClient3 low-latency Windows guidance: https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/low-latency-audio
6. Steinberg VST3 3.8+ SDK and MIT licensing: https://github.com/steinbergmedia/vst3sdk ; https://steinbergmedia.github.io/vst3_dev_portal/pages/VST%2B3%2BLicensing/VST3%2BLicense.html
7. Steinberg VST3 host requirements: https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/Host%2BRequirements/Index.html
8. Steinberg ASIO SDK, GPLv3 OR proprietary alternative (file-level review): https://github.com/audiosdk/asio/blob/main/LICENSE.txt
9. RtAudio Windows WASAPI/ASIO wrapper and license: https://github.com/thestk/rtaudio ; RtMidi Windows MIDI API wrapper: https://github.com/thestk/rtmidi
10. libsndfile formats/license: https://libsndfile.github.io/libsndfile/formats.html ; https://libsndfile.github.io/libsndfile/FAQ.html ; libsamplerate BSD-2-Clause: https://github.com/libsndfile/libsamplerate
11. CLAP plug-in SDK (MIT): https://github.com/free-audio/clap/blob/main/LICENSE
12. Rubber Band GPL-2.0-or-later for offline time stretch: https://github.com/breakfastquay/rubberband
13. Windows 10 end of standard support (2025-10-14): https://support.microsoft.com/en-us/windows/deployment/updates-lifecycle/windows-10-support-has-ended-on-october-14-2025
14. Microsoft Media Foundation file decoding: https://learn.microsoft.com/en-us/windows/win32/medfound/supported-media-formats-in-media-foundation
15. Workflow background only (do not copy commercial assets): Ableton Arrangement https://www.ableton.com/en/manual/arrangement-view/ and Session https://www.ableton.com/en/manual/session-view/

---

**Final directive:** Produce a stable, lean and measurable DAW rather than a plausible-looking interface. Start with Phase 00 **Win32/Direct3D11/Dear ImGui shell**, then Phase 01 **audible audio**. Every phase must end with real runnable build/test evidence and a compact handoff. No implementation is claimed by this specification alone.

## 15. Explicit architecture conflict resolutions

These override any ambiguous implementation suggestion elsewhere:

1. **ImGui is the only GUI framework.** Do not reintroduce Qt for dialogs, panels, waveform drawing or VST3 editors. Native Win32 facilities are allowed and expected; thin platform wrappers are not an alternative GUI toolkit.
2. **D3D11 is purely UI rendering.** It does not run the audio graph. No audio correctness depends on GPU timing, FPS or monitor refresh rate.
3. **Multi-viewport ≠ multiple projects or multiple ImGui contexts.** Separate physical HWNDs for panels, one context and one active audio project initially. Additional project tabs are out of v1.0 scope.
4. **Graph and UI are distinct.** An ImGui node position has no influence on DSP order. Connection edits are validated and compiled off the callback; valid graph snapshots are switched safely at block boundaries.
5. **VST3 GUIs may be native HWNDs.** The host does not falsely promise arbitrary vendor GUI embedded inside an ImGui dock. Vendor GUI failure falls back to generic parameter view while DSP may continue.
6. **Production/Live share routing, tempo and transport.** Their views and layout states differ, not the fundamental audio engine or project format.
7. **Low CPU is measured.** An empty idle editor does not need to present continuously. UI render pacing must not cause late transport commands, missed MIDI or audio xruns.
8. **Windows 10 is a compatibility target, not a supported Microsoft OS claim.** Verify on 22H2 x64; record unresolved differences. No guarantee for unsupported legacy Windows releases.
9. **Portable executable means a portable *folder*, not necessarily a lone EXE.** System D3D11 DLLs come from Windows; approved runtime DLLs and assets are alongside the app. Installers are optional and cannot delete user projects on uninstall.
10. **License decisions are gates.** Any SDK, extension, audio asset, vendor plug-in or driver requiring a commercial license must not be shipped; GPL compatibility and redistribution notices are documented.

## 16. Extended acceptance matrix for the ImGui migration

| Gate | Earliest qualifying phase | Passing evidence |
|---|---|---|
| EXE creates Win32 HWND and D3D11 swap chain | 00 | Windows x64 screenshot/video plus actual run log and stable exit |
| Dock, detach and return window | 00 | One context, saved layout, native secondary viewport test, monitor transfer |
| Idle UI sleeps and avoids busy-spinning | 00 | Repeatable 60-second CPU/GPU/frame measurements while settled and minimized |
| OS save/open dialogs do not stall audio | 01/03 | Native file dialog test during metronome/playback, no first-party xrun |
| Mixed DPI and monitor reconnection | 00 foundational, 12 qualification | 100/150% scale tests; detached window restored in visible bounds after unplug |
| Waveform cache culls unseen audio | 02 | Large-file scroll test, bounded CPU/time and draw primitive counts |
| Piano-roll grid UI separates model from view | 05 | Offline notes and undo persist identically with UI hidden |
| Drum pad trigger independent of FPS | 06 | Same scheduled output at 30, 60, capped idle frame modes |
| Mixer snapshot meters not callback GUI access | 07 | Stress test: 8 tracks, fast mouse faders, no callback stalls |
| Rack nodes compile separately from cables | 08 | Move visual nodes during playback without changing offline audio; invalid cable rejection |
| VST3 native editor focus and lifetime | 09 | Editor open/close and second monitor; typing does not trigger transport shortcuts; state survives |
| Live clips insensitive to ImGui FPS | 10 | Quantized launch positions identical under 30/60 Hz and minimized (MIDI initiated) |
| UI refresh regression gate | 12 | Compare reference CPU/GPU time across idle, audio play, large UI, dual displays |

**Final acceptance status for this planning deliverable:** `SPECIFICATION_COMPLETE`, `IMPLEMENTATION_NOT_STARTED`. No source build, Windows run or hardware test has been carried out by preparing this document.
