# Software Design Description: Architecture

Requirements: [SRS.md](SRS.md).

## Layers

```
 grammar text (Cambriana format; .ls converted)
        |
        v
 +---------------------------------------------------------------+
 | CambrianaCore  (private repo; C++17, no Unreal headers)       |
 |   parse -> rewrite -> turtle -> bone tree -> surfacer         |
 |   output: vertices, triangles, skin influences, UVs, data     |
 +---------------------------------------------------------------+
        |  headers + dynamic libraries        |
        v                                     v
 Cambriana (runtime module)            dev tools (private repo)
   asset types, live actor,              glTF exporter,
   pose/growth evaluation                golden-file runner
        |
        v
 CambrianaEditor (editor module)
   asset editor, importers, bake
```

| Layer | Depends on | Repo | Ships in a game |
|---|---|---|---|
| CambrianaCore | C++17 standard library | Private; delivered here as a dynamic library | Yes |
| Cambriana | CambrianaCore, Unreal runtime modules | This | Yes |
| CambrianaEditor | Cambriana, Unreal editor modules | This | No |
| Dev tools | CambrianaCore | Private | No |

Generation runs in packaged games (FR-18). Generation
(core call, skinned mesh construction, skeleton, physics bodies) is in the runtime module with
no editor-only dependencies, and editor bake uses the same path. Only asset saving, editors and
importers are in the editor module.

## Design documents

| Document | Repo | Contents |
|---|---|---|
| SDD-architecture.md | This | This document |
| [SDD-rigging.md](SDD-rigging.md) | This | Bone tree, skinning, parameter tiers, posing, growth, physics, naming |
| [SDD-surfacing.md](SDD-surfacing.md) | This | Tubes, blocks, polygons, blobs, UVs, data channels, materials |
| [SDD-unreal.md](SDD-unreal.md) | This | Assets, editor, live actor, run-time pieces, modules |
| [FMT-grammar.md](FMT-grammar.md) | This | New grammar format |
| `notes/dev/SDD-core.md` | CambrianaCore | Core data model, behaviour, build, tests |
| `notes/dev/FMT-legacy-ls.md` | CambrianaCore | Legacy `.ls` dialect |
| `notes/dev/roadmap.md` | CambrianaCore | Core milestones |
| `notes/dev/ATS-core.md` | CambrianaCore | Core acceptance tests |

## Core delivery

The core's source stays in `ScottKirvan/CambrianaCore`. The core is a dynamic library. This
repo contains only its public headers, import libraries and dynamic libraries.

- External module at `Source/ThirdParty/CambrianaCore/`, with `include/`, `lib/<Platform>/`
  (import libraries) and `bin/<Platform>/` (`CambrianaCore.dll` on Win64,
  `libCambrianaCore.so` on Linux and Android).
- The external module declares the dynamic library as a runtime dependency, so it is staged
  with editor builds and packaged games. On Android the `.so` is added to the APK by an Unreal
  Plugin Language file.
- The runtime module loads the library at startup and checks its API version; a mismatch fails
  module startup with a logged error.
- The public API is a C interface: opaque handles, plain structs and arrays, error codes. No
  C++ types or exceptions cross it. Memory the core allocates is released by the core.
- Only the C API is exported; all other symbols are hidden.
- A script in the private repo builds the libraries and copies headers and libraries here,
  stamped with the core version and commit. Updates arrive as a pull request.
- Each platform's library is built with the toolchain Unreal 5.8 uses for that platform.
- Symbol files stay in the private repo.
- Git LFS for the libraries if they grow large.
- This repo builds without access to the private repo.

Platform order Win64, Linux, Android; Mac and iOS later.

## Data flow

1. Parse grammar text; warnings carry line and column.
2. Rewrite the axiom for N generations; a fractional generation marks new growth.
3. Turtle walks the production and emits the bone tree.
4. Surfacer builds the surface; every vertex carries its bone influences.
5. Consumers: live dynamic mesh, baked assets, glTF dev tool, tests.

## Coordinates

The core uses Unreal's space: left-handed, +Z up, centimetres. Output is used in
Unreal unconverted.

## Size

Default overall object size about 2 m.

| Mode | Behaviour |
|---|---|
| Fit (default) | Step length is chosen so the largest bounding-box dimension of the full-generation bind result equals the target size. Target default 200 cm. |
| Step | Step length set directly, in cm. |

- In Fit mode the step length is computed once per structure or shape change. Pose and
  growth changes do not rescale.
- Min thickness and other absolute lengths are in cm and are not rescaled.

## Engine modules

| Module or plugin | Location in 5.8 | Use |
|---|---|---|
| GeometryCore | `Engine/Source/Runtime/GeometryCore` | Dynamic mesh data |
| GeometryFramework | `Engine/Source/Runtime/GeometryFramework` | Live actor and preview |
| Geometry Scripting | `Engine/Plugins/Runtime/GeometryScripting` | Blueprint access |
| PCG | `Engine/Plugins/PCG` | Scattering baked variants |
| Control Rig | `Engine/Plugins/Animation/ControlRig` | Shared rig built from rig hints |

## Repo layout

Plugin at the repo root: `Cambriana.uplugin`, `Source/`, `Content/`. Test host
project in `HostProject/`, not packaged. `UE_Cambriana.code-workspace` (local, gitignored) includes this repo
and `../CambrianaCore`.
