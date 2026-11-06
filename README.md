# LightweightDAW (working title)

A small, free digital audio workstation for Windows 11, built in phases where every phase must run and be measured. Native C++20, Win32, Direct3D 11 and Dear ImGui.

**Status:** Phase 00, the application shell. It makes no sound yet. See [docs/PROJECT_STATE.md](docs/PROJECT_STATE.md).

## Build

You need Visual Studio 2022 Build Tools (C++ x64 and a Windows SDK), CMake 3.28+, Ninja and Git. From any shell:

```bash
tools/build.cmd release
```

This fetches the pinned dependencies, builds, and runs all tests. The program is `build/release/bin/LightweightDAW.exe`.

To create the portable folder and ZIP, and test the ZIP from a fresh extract:

```bash
tools/package.cmd 00
```

To measure CPU, memory and frame rate in each UI mode (about 8 minutes):

```bash
powershell -ExecutionPolicy Bypass -File tools/profile.ps1
```

## Useful options

| Option | Effect |
|---|---|
| `--data-dir PATH` | Keep settings, layouts and logs in PATH |
| `--warp` | Diagnostic software rendering, clearly labeled in the UI |
| `--ui-self-test` | Scripted docking, workspace and recovery checks; exit code 0 means pass |
| `--smoke-seconds N` | Run for N seconds, then exit |
| `--profile idle\|active\|minimized\|detached` | Measurement mode; see `tools/profile.ps1` |

## License

GPL-3.0-or-later, with an [additional permission](LICENSE-EXCEPTION.md) for loading non-GPL plug-ins and drivers. See [LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
