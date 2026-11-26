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
| (shared) | Dear ImGui lives in `code/3rd-party/imgui` (1.92.1 docking, IX-Ray's copy): the game links it statically, the editors as `imgui_editor.dll`. |
| `xrECore` | Editor core: device, cameras, tools and scene base, editable objects, libraries, and Infernis's DX9 R1 renderer sources compiled in `_EDITOR` mode as the viewport renderer. Links `xrEngineCore.dll`, the engine built as a DLL for the editors. |
| `ShaderEditor` | Engine shaders (`shaders.xr`), compiler shaders (`shaders_xrlc.xr`), game materials and material pairs (`gamemtl.xr`), sound environments (`senvironment.xr`, GSC's EAX layout; IX-Ray's page edits its own EFX layout, so this one is rewritten). |
| `ParticleEditor` | Particle effects and groups (`particles.xr`, or the `.pe`/`.pg` sources under `$game_particles$` when the workspace has them). The actions use GSC's layout (Clear Sky SDK), as Infernis's particle engine is GSC's; IX-Ray's particle animation curves are left out (`EDITOR_PARTICLE_ANIM_CURVES`). |
| `ActorEditor` | Editable objects (`.object`) with skeletons and motions (`.skl`/`.skls`), exported to `.ogf`/`.omf` in Infernis's (GSC's) format: 16-bit rotation keys, 8- or 16-bit translations. IX-Ray's 32-bit keys and motion notify tracks are left out (`EDITOR_MOTION_NOTIFY`). Post-process effects (`.ppe`) through Infernis's own `PostprocessAnimator`. |
| `3rd-party/MagicSoftware` | The part of FreeMagic the skinned-mesh export uses for bone shapes (see its README). |
| `test` | `xrEditorUITest`: opens the shell with a property grid. Run it from a folder with an `fsgame.ltx` whose `$game_fonts$` holds `editors\*.ttf`; `-frames <n> -shot <file.bmp>` closes after n frames and saves the last one. |

## Running the editors

Run an editor from a workspace folder holding an IX-Ray-style `fs.ltx` (pass another with `-fsltx <file>`). The
workspace's game data must have the game's `shaders\r1` (the viewport renderer compiles them), IX-Ray's editor
textures (`textures\ed`) and editor fonts (`fonts\editors`). Settings and the shader cache go to
`$app_data_root$`.

Options for checks without anyone at the screen: `-frames <n>` closes after n frames (drawn every frame, dialogs
logged and answered OK, or Cancel/No for questions), `-shot <file.png|bmp>` saves the last one, `-save` runs Save
on the last frame without edits (open/save round trips). Shader Editor: `-ogf <model>` draws a game model
(`$game_meshes$` name, no extension) at the origin and fits the camera to it. Particle Editor: `-pe <effect>`
selects an effect and plays it. Actor Editor: an `.object` path as the first argument opens it; `-engine` shows it
through the engine's model and animation code (the Engine render style), `-motion <name>` plays a motion,
`-smooth legacy|edges|normals` picks how vertex normals are made (legacy is Shadow of Chornobyl's 3ds Max smoothing
groups, edges Call of Pripyat's, the default), `-export_ogf <file>` and `-export_omf <file>` export on the last
frame. The whole command line must stay under 512 characters (`Core.Params`).

On a first run (no saved layout) the Actor Editor docks its windows into a default layout; IX-Ray opens every window
small and undocked.

The editors are per-monitor DPI aware (their manifests): on a scaled display they draw at native resolution and
scale their UI by the display's DPI.

The bone view and bone nodes are `xrEProps` sources built by `xrECore`, which has `CBone`'s editor layout. Not built
yet: the dialog nodes (IX-Ray's tinyxml2-based XML API).

Changes to the IX-Ray sources are marked with an `Infernis` comment where they change behaviour (single-player game
types only, no lazy archive loading, DX9 texture handles instead of IX-Ray's RHI).

## Licence

The sources under `xrEUI`, `xrEProps`, `xrECore`, `ShaderEditor`, `ParticleEditor`, `ActorEditor`, `Public`, `compat` (ported parts) and the ImGui core in `code/3rd-party/imgui` come from
[IX-Ray](https://github.com/ixray-team/ixray-1.6-stcop), commit `09c1a3de9`, under its licence: MIT-style
permission to use, copy, modify, merge, publish and distribute, **except selling copies**; the copyright notice
must be kept ("Copyright (c) 2019-2025 IX-Ray and contributors"). GSC Game World keeps the rights to the original
X-Ray code. Dear ImGui, ImGuizmo, ImPlot and imnodes are MIT licensed; Font Awesome fonts are under the SIL OFL. FreeMagic is
under Magic Software's free source code licence.
