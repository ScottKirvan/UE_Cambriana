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
        |  headers + static libraries         |
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
| CambrianaCore | C++17 standard library | Private; delivered here prebuilt | Yes |
| Cambriana | CambrianaCore, Unreal runtime modules | This | Yes |
| CambrianaEditor | Cambriana, Unreal editor modules | This | No |
| Dev tools | CambrianaCore | Private | No |

Module split and names: `[Proposed — unconfirmed]`.

**Decided**: generation runs in packaged games (FR-18). `[Proposed — unconfirmed]`: generation
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
| `notes/dev/ATS-core.md` | CambrianaCore | Core acceptance tests |

## Core delivery

**Decided**: the core's source stays in `ScottKirvan/CambrianaCore`. This repo contains only
its public headers and static libraries.

`[Proposed — unconfirmed]`:

- External module at `Source/ThirdParty/CambrianaCore/`, with `include/` and `lib/<Platform>/`.
- A script in the private repo builds the libraries and copies headers and libraries here,
  stamped with the core version and commit. Updates arrive as a pull request.
- Library build settings: `/MD`, `_ITERATOR_DEBUG_LEVEL=0`, no `/GL`, MSVC toolset no newer
  than Unreal 5.8's.
- Headers expose only the public API. No exceptions cross it.
- Each platform's library is built with the toolchain Unreal 5.8 uses for that platform.
- Symbol files stay in the private repo.
- Git LFS for the libraries if they grow large.
- This repo builds without access to the private repo.

**Decided**: platform order Win64, Linux, Android; Mac and iOS later.

## Data flow

1. Parse grammar text; warnings carry line and column.
2. Rewrite the axiom for N generations; a fractional generation marks new growth.
3. Turtle walks the production and emits the bone tree.
4. Surfacer builds the surface; every vertex carries its bone influences.
5. Consumers: live dynamic mesh, baked assets, glTF dev tool, tests.

## Coordinates

**Decided**: the core uses Unreal's space: left-handed, +Z up, centimetres. Output is used in
Unreal unconverted.

## Size

**Decided**: default overall object size about 2 m.

`[Proposed — unconfirmed]`:

| Mode | Behaviour |
|---|---|
| Fit (default) | Step length is chosen so the largest bounding-box dimension of the full-generation bind result equals the target size. Target default 200 cm. |
| Step | Step length set directly, in cm. |

- In Fit mode the step length is computed once per structure or shape change. Pose and
  growth changes do not rescale.
- Min thickness and other absolute lengths are in cm and are not rescaled.

## Engine modules

`[Proposed — unconfirmed]`

| Module or plugin | Location in 5.8 | Use |
|---|---|---|
| GeometryCore | `Engine/Source/Runtime/GeometryCore` | Dynamic mesh data |
| GeometryFramework | `Engine/Source/Runtime/GeometryFramework` | Live actor and preview |
| Geometry Scripting | `Engine/Plugins/Runtime/GeometryScripting` | Blueprint access |
| PCG | `Engine/Plugins/PCG` | Scattering baked variants |

## Repo layout

**Decided**: plugin at the repo root: `Cambriana.uplugin`, `Source/`, `Content/`. Test host
project in `HostProject/`, not packaged. `UE_Cambriana.code-workspace` (local, gitignored) includes this repo
and `../CambrianaCore`.
