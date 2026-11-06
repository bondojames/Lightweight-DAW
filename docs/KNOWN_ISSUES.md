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

## Blocked evidence

| Item | Needs |
|---|---|
| Dual-monitor checks (manual 4, 13, 14) | A second display. None is connected to the PC. The laptop plus a monitor or TV would work, and would also test mixed scaling |
| Clean-system run | Run the ZIP on the laptop, which has no developer tools, or use Windows Sandbox or a VM. The ZIP has run from a fresh extract with no developer PATH on this PC, which is weaker evidence |
| Windows CI result | Confirm the first run in the repo's Actions tab |

## Hardware for later phases

- **Microphone (Phase 04):** Thronmax MDrill Zero is present. A USB microphone covers the capture path, but not a multi-input interface.
- **MIDI (Phase 05, 10):** the Akai APC Mini was not detected when checked on 2026-11-06, so it needs plugging in. See ADR-013 for what it cannot cover.
- **ASIO (Phase 04, 12):** none installed, see OPEN-002. Candidates are ASIO4ALL (freeware, proprietary) and FlexASIO (open source). Both wrap Windows audio, so they prove the host code but do not meet the spec's "real ASIO interface" sign-off. Install only for local testing, never bundle.
