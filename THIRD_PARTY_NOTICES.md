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

### libsndfile 1.2.2 (`sndfile.dll`)
- Source: https://github.com/libsndfile/libsndfile, commit `72f6af15e8f85157bd622ed45b979025828b7001`
- License: GNU Lesser General Public License, version 2.1 or later. Copyright (C) Erik de Castro Lopo and contributors. Full text: `licenses/libsndfile-COPYING.txt`.
- Built unmodified, with external codecs off. The only addition is an export list, `cmake/sndfile.def`, and a build step that wraps the library in a DLL.
- It is dynamically linked. You may replace `sndfile.dll` with your own ABI-compatible build. The source for this exact version is at the link above, and how it's built is in `cmake/Dependencies.cmake` in this project's repository.
- Audit note: of the files at this commit, only `src/G72x/g72x_test.c` carries a GPL notice. It's a test program that isn't compiled here.

### libsamplerate 0.2.2
- Source: https://github.com/libsndfile/libsamplerate, commit `c96f5e3de9c4488f4e6c97f59f5245f22fda22f7`
- License: BSD-2-Clause. Copyright (c) 2012-2016 Erik de Castro Lopo. Full text: `licenses/libsamplerate-COPYING.txt`.
- Statically linked.

## Used from Windows, not redistributed

Direct3D 11, DXGI, D3DCompiler_47, DWM, IMM32, DbgHelp, AVRT, WASAPI, Media Foundation and its codecs, Shell and COM libraries, and the Segoe UI font are part of Windows and are not included in this distribution. FLAC, MP3 and M4A decoding is provided by Windows when those codecs are present.

## Planned, not yet included or approved

Each must pass its own audit before it ships: RtMidi, RtAudio, VST3 SDK, ASIO SDK, Rubber Band, ImPlot, imgui-node-editor, CLAP.

VST and ASIO are trademarks of Steinberg Media Technologies GmbH. This project is not affiliated with Steinberg.
