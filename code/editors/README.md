# Editors (level editor SDK)

The IX-Ray editors, ported to run on Infernis's own libraries. Off by default; nothing here is linked by the game
or the compilers.

## Build

Needs CMake 3.24 or newer and an MSVC toolset that compiles C++23 (14.43 or newer). SDL3 3.2.22 is downloaded at
configure time from the official release archive (pinned by hash); pass `-DFETCHCONTENT_SOURCE_DIR_SDL3=<path>` to
use an extracted copy instead.

Use a separate build folder and output root, so the editor build never overwrites the game's binaries in `bin/`:

```
cmake -S . -B build-editors -G "Visual Studio 17 2022" -A x64 -DBUILD_EDITORS=ON -DOUTPUT_ROOT=<repo>/build-editors/out
cmake --build build-editors --config RelWithDebInfo --target xrEditorUITest
```

On a machine whose default v143 toolset is older than 14.43, set `VCToolsVersion` (for example `14.43.34808`) in
the environment for both commands.

## Contents

| Folder | What |
|---|---|
| `xrEUI` | Editor window shell: ImGui on SDL3 and Direct3D 9, theme, fonts, gizmo, plots, node editor, file dialogs (`EFS`). |
| `xrEProps` | Property grids, item lists, choosers, small forms. |
| `Public` | Headers shared by the editor libraries. |
| `compat` | IX-Ray `xrCore` types and helpers that Infernis's `xrCore` lacks (`xr_path`, `xr_delegate`, string helpers). Editor-only. |
| `3rd-party/imgui` | Dear ImGui 1.92.1, docking branch, with IX-Ray's additions. Separate from the game's `code/3rd-party/imgui`. |
| `test` | `xrEditorUITest`: opens the shell with a property grid. Run it from a folder with an `fsgame.ltx` whose `$game_fonts$` holds `editors\*.ttf`; `-frames <n> -shot <file.bmp>` closes after n frames and saves the last one. |

Not built yet: the bone views and bone nodes in `xrEProps` (need `CBone` from the editor core) and the dialog nodes
(use IX-Ray's tinyxml2-based XML API).

Changes to the IX-Ray sources are marked with an `Infernis` comment where they change behaviour (single-player game
types only, no lazy archive loading, DX9 texture handles instead of IX-Ray's RHI).

## Licence

The sources under `xrEUI`, `xrEProps`, `Public`, `compat` (ported parts) and `3rd-party/imgui` come from
[IX-Ray](https://github.com/ixray-team/ixray-1.6-stcop), commit `09c1a3de9`, under its licence: MIT-style
permission to use, copy, modify, merge, publish and distribute, **except selling copies**; the copyright notice
must be kept ("Copyright (c) 2019-2025 IX-Ray and contributors"). GSC Game World keeps the rights to the original
X-Ray code. Dear ImGui, ImGuizmo, ImPlot and imnodes are MIT licensed; Font Awesome fonts are under the SIL OFL.
