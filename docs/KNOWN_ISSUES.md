# Known Issues

## Phase 00

| ID | Issue | Impact | Plan |
|---|---|---|---|
| KI-001 | File drop works on the main window only, not on detached panels. The ImGui backend owns those windows' message handling. | Dropping on a detached panel does nothing | Use OLE drag and drop in Phase 02 or 03 |
| KI-002 | Chinese, Japanese and Korean characters show as `?` boxes. Segoe UI lacks them, and loading a CJK font at startup adds tens of MB. Paths are still handled correctly; only display is affected. | Display only | Load a fallback font on demand in Phase 03 (library) |
| KI-003 | At a DPI change, the ImGui version scales fonts in detached panels but not their padding, and padding follows the main window's scale. | Slightly tight or loose spacing on mixed-scale monitors | Accept for now; recheck in manual test 13 |
| KI-004 | The UI stops redrawing while a native file dialog is open. Audio will not be affected, because it has its own thread. | Meters will freeze during the dialog from Phase 01 on | Accepted; document in help |
| KI-005 | "Simulate GPU device reset" rebuilds the device but does not make a real driver fail. | Real device-removed behavior is not yet proven | Test with `dxcap -forcetdr` if available, or during Phase 12 qualification |
| KI-006 | Keyboard and high-contrast support exist, but ImGui does not expose controls to screen readers. | Screen readers cannot read the UI | Document in help; revisit before v1.0 |
| KI-007 | Build Tools has no debugger, so crash dumps cannot be read on this machine yet. | Slower crash diagnosis | Install WinDbg when needed |

## Phase 01

| ID | Issue | Impact | Plan |
|---|---|---|---|
| KI-008 | Most drivers here only offer 10 ms (441 or 480 frames) in WASAPI shared mode. Of 4 outputs on this PC, only the generic Windows HD driver gave 128 and 256. | Higher latency than requested; the UI says so | Exclusive mode or ASIO before v1.0 |
| KI-009 | A rate other than the device's uses Windows resampling and the 10 ms period. | Latency only; timing measured within 1 frame | Accept; add engine-side resampling only if needed |
| KI-010 | Device dropout detection is timing-based and cannot see glitches inside the driver (ADR-017). | A real glitch could show 0 dropouts | Listening tests; compare loopback captures in later phases |
| KI-011 | Changing device stops the transport and keeps the position. | Press Play again after a device change | Accepted for now |
| KI-012 | Only one tempo segment and fixed 4/4. | No tempo map editing yet | Phase 02 onward |
| KI-013 | The load percentile has 0.5% resolution. | Low loads all read "0.5%" | Finer bins near zero if useful |

## Phase 02

| ID | Issue | Impact | Plan |
|---|---|---|---|
| KI-014 | **There's no save.** Closing the app loses the arrangement. | Work is lost on exit | Phase 03 |
| KI-015 | MP3 decoding has no automated test, because Windows has no MP3 encoder to make a fixture. | Covered by hands-on check 2 only | Commit a small redistributable fixture in Phase 03 |
| KI-016 | The audio cache folder is never cleaned. A one-hour stereo file uses about 1.3 GB per sample rate. | Disk use grows | Size limit and cleanup in Phase 03 |
| KI-017 | The loop wrap isn't crossfaded, so a loop point on loud audio can click. | Audible on some material | Phase 11 crossfades |
| KI-018 | Fades are only the fixed 64-frame edge fades. Overlapping clips simply mix. | No user fades or crossfades yet | Phase 11 |
| KI-019 | No time-stretch: audio stays at its own tempo when the BPM changes. | By design for now | Phase 11 |
| KI-020 | Ogg Vorbis isn't supported, though the spec lists it where vetted. | Those files are refused with a message | Audit a decoder later |
| KI-021 | Multichannel files keep only their first 2 channels. | Surround content is reduced | Out of v1.0 scope |
| KI-022 | Windows N editions without Media Foundation can only import WAV and AIFF. | Others fail with a clear message | Accepted |
| KI-023 | The waveform's finest detail is 256 frames per bin, so extreme zoom shows blocks. | Display only | Finer data if needed |
| KI-024 | Scrubbing moves the playhead but doesn't play sound while dragging. | Not required yet | Later |

## Blocked evidence

| Item | Needs |
|---|---|
| Dual-monitor checks (manual 4, 13, 14) | A second display. None is connected to the PC. The laptop plus a monitor or TV would work, and would also test mixed scaling |
| Clean-system run | Run the ZIP on the laptop if it has no developer tools, or use Windows Sandbox or a VM. The ZIP has run from a fresh extract with no developer PATH on this PC, which is weaker evidence |
| Laptop results | The owner has a laptop; its model and results are not yet recorded |

## Hardware for later phases

- **Microphone (Phase 04):** Thronmax MDrill Zero is present. A USB microphone covers the capture path, but not a multi-input interface.
- **MIDI (Phase 05, 10):** the Akai APC Mini was not detected when checked on 2026-11-06, so it needs plugging in. See ADR-013 for what it cannot cover.
- **ASIO (Phase 04, 12):** none installed, see OPEN-002. Candidates are ASIO4ALL (freeware, proprietary) and FlexASIO (open source). Both wrap Windows audio, so they prove the host code but do not meet the spec's "real ASIO interface" sign-off. Install only for local testing, never bundle.
