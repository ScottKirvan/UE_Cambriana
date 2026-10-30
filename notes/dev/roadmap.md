# Roadmap

A milestone is complete when its acceptance tests ([ATS.md](ATS.md)) pass. Core milestones
(`C`) and their tests are in CambrianaCore `notes/dev/roadmap.md` and `notes/dev/ATS-core.md`.

Every milestone also keeps ATS-05 (no experimental dependencies) passing.

| Milestone | Content | Needs core | Tests |
|---|---|---|---|
| M0 | Build skeleton: plugin, base project, core loading and version check | C0 | ATS-01, ATS-05 |
| M1 | Live actor and seeds | C2 | ATS-30 to ATS-35 |
| M2 | Grammar assets, import with conversion, reimport, export, grammar editor | C3 | ATS-10 to ATS-15, ATS-20 to ATS-23 |
| M3 | Skinned mesh construction in the runtime module; bake on it, regenerate, detach | C3 | ATS-40 to ATS-45 |
| M4 | Run-time generation and definitions | C3 | ATS-70 to ATS-73 |
| M5 | Pose parameters | C4 | ATS-50 to ATS-52 |
| M6 | Growth animation | C4 | ATS-53 to ATS-55 |
| M7 | Rig controls | C5 | ATS-56 to ATS-59 |
| M8 | Blobs | C6 | ATS-25 |
| M9 | PCG and scripting | C3 | ATS-60, ATS-61 |
| M10 | Release packaging and platforms | C6 | ATS-02, ATS-03, ATS-04, ATS-62 |
