# Third-Party Notices

LightweightDAW is licensed under GPL-3.0-or-later (see `LICENSE`) with an additional permission for loading non-GPL plug-ins and drivers (see `LICENSE-EXCEPTION.md`). It includes the components below, each under its own terms. This is an engineering audit record, not legal advice.

## Shipped in the Phase 00 program

### Dear ImGui v1.93.0-docking
- Source: https://github.com/ocornut/imgui, commit `b916186e66aa3c02b72c6ca17455516512ed8049`
- License: MIT. Copyright (c) 2014-2026 Omar Cornut. Full text: `licenses/imgui-LICENSE.txt`.
- Compiled from: `imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp`, `backends/imgui_impl_win32.cpp`, `backends/imgui_impl_dx11.cpp`. The demo window and the extra fonts in `misc/fonts/` are not compiled or shipped.

Material embedded in those files, audited at the pinned commit:

| Material | Terms | Notice |
|---|---|---|
| stb_truetype, stb_rect_pack, stb_textedit (`imstb_*.h`) | Public domain or MIT, recipient's choice | Sean Barrett |
| ProggyClean font data (`imgui_draw.cpp`) | MIT | Copyright (c) 2004, 2005 Tristan Grimmer |
| ProggyForever font data (`imgui_draw.cpp`) | MIT | Copyright (c) 2026 Disco Hello, Copyright (c) 2019, 2023 Tristan Grimmer |
| Japanese kanji code point table (`imgui_draw.cpp`) | CC BY 4.0 | Sourced by Dear ImGui from lists published by Japan's Agency for Cultural Affairs and Ministry of Justice |

## Used from Windows, not redistributed

Direct3D 11, DXGI, D3DCompiler_47, DWM, IMM32, DbgHelp, Shell and COM libraries, and the Segoe UI font are part of Windows and are not included in this distribution.

## Planned, not yet included or approved

Each must pass its own audit before it ships: RtMidi, RtAudio, libsndfile, libsamplerate, VST3 SDK, ASIO SDK, Rubber Band, ImPlot, imgui-node-editor, CLAP.

VST and ASIO are trademarks of Steinberg Media Technologies GmbH. This project is not affiliated with Steinberg.
