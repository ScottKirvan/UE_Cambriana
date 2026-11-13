# Roadmap

A milestone is complete when its acceptance tests ([ATS.md](ATS.md)) pass. Core milestones
(`C`) and their tests are in CambrianaCore `notes/dev/roadmap.md` and `notes/dev/ATS-core.md`.

Every milestone also keeps ATS-05 (no experimental dependencies) passing.

| Milestone | Content | Needs core | Tests |
|---|---|---|---|
| M0 | Build skeleton: plugin, base project, core loading and version check | C0 | ATS-01, ATS-05 |
| M1 | Live actor and seeds; interim grammar asset holding legacy text | C2 | ATS-30 to ATS-35 |
| M2 | Grammar assets, import with conversion, reimport, export, grammar editor | C3 | ATS-10 to ATS-15, ATS-20 to ATS-23 |
| M3 | Static and skinned mesh construction in the runtime module; bake on it, reimport, detach | C3 | ATS-40 to ATS-48 |
| M4 | Run-time generation and definitions | C3 | ATS-70 to ATS-73 |
| M5 | Pose parameters | C4 | ATS-50 to ATS-52 |
| M6 | Growth animation | C4 | ATS-53 to ATS-55 |
| M7 | Rig controls | C5 | ATS-56 to ATS-59 |
| M8 | Blobs | C6 | ATS-25 |
| M9 | PCG and scripting | C3 | ATS-60, ATS-61 |
| M10 | Release packaging and platforms | C6 | ATS-02, ATS-03, ATS-04, ATS-62 |

## M3 steps

Each step is one PR with its acceptance test.

| # | Step | Tests |
|---|---|---|
| 1 | Bake a static mesh from a grammar asset (right-click Bake); static mesh output as a bake setting | ATS-41 |
| 2 | Baked static mesh matches the live actor | ATS-46 |
| 3 | Data channels in the vertex slots | ATS-45 |
| 4 | Live actor Details Bake writes a static mesh | ATS-47 |
| 5 | Skeleton and skinned skeletal mesh under a synthetic Root | ATS-40 (mesh, skeleton, slots) |
| 6 | Physics asset with capsules merged along chains | ATS-40 (physics asset) |
| 7 | Bind pose matches the live actor | ATS-42 |
| 8 | Bake from the grammar editor, asking to save unsaved edits | ATS-48 |
| 9 | Reimport on a baked mesh's right-click menu | ATS-43 |
| 10 | Detach on a baked mesh's right-click menu | ATS-44 |

Rules text syntax colouring (FR-05) is deferred out of M3:
[UE_Cambriana #36](https://github.com/ScottKirvan/UE_Cambriana/issues/36).
