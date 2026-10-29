# Roadmap

`[Proposed — unconfirmed]` order. A milestone is complete when its acceptance tests pass.
`ATC-` tests are in CambrianaCore `notes/dev/ATS-core.md`; `ATS-` tests are in [ATS.md](ATS.md).

Every milestone also keeps ATC-50 to ATC-55 (determinism, robustness, threading,
performance) and ATS-05 (no experimental dependencies) passing.

| Milestone | Content | Tests |
|---|---|---|
| M0 | Build skeleton: core CMake build, delivery script, plugin and host project | ATC-01, ATC-02, ATS-01, ATS-05 |
| M1 | Legacy dialect: parsing, rewriting, turtle, bone tree | ATC-10 to ATC-15, ATC-57, ATC-58 |
| M2 | Surfaces, skinning, Fit size, data channels, glTF dev tool | ATC-20 to ATC-25, ATC-60 |
| M3 | Cambriana grammar format, Lparser and cpfg conversion, round-trip, environment-query stepping | ATC-40 to ATC-49, ATC-56 |
| M4 | Live actor and seeds | ATS-30 to ATS-35 |
| M5 | Skinned mesh construction in the runtime module; run-time generation and definitions | ATS-70 to ATS-73 |
| M6 | Grammar assets, import with conversion, reimport, export, grammar editor | ATS-10 to ATS-15, ATS-20 to ATS-23 |
| M7 | Bake on the shared construction path, regenerate, detach | ATS-40 to ATS-45 |
| M8 | Pose parameters | ATS-50 to ATS-52 |
| M9 | Growth animation | ATS-53 to ATS-55 |
| M10 | Blobs | ATC-30, ATC-31, ATS-25 |
| M11 | PCG and scripting | ATS-60, ATS-61 |
| M12 | Release packaging and platforms | ATS-02, ATS-03, ATS-04, ATS-62 |
