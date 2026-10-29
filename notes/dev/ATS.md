# Acceptance Test Specification: Plugin

Requirements: [SRS.md](SRS.md). Core tests: CambrianaCore repo, `notes/dev/ATS-core.md`.
`[Proposed — unconfirmed]` unless marked.

## Build

| ID | Given / when / then | Verifies |
|---|---|---|
| ATS-01 | Given a clone of this repo without access to CambrianaCore, when `HostProject` is built with Unreal 5.8 and Visual Studio 2022, then it links the delivered core library with no warnings. | C-02, NFR-01 |
| ATS-02 | Given Unreal's plugin packaging tool, when run for 5.8, then the plugin packages without errors or warnings. | NFR-07 |
| ATS-03 | Given Fab's technical requirements, then the plugin meets each one. | NFR-07 |
| ATS-04 | Given each supported platform (Win64, Linux, Android), when `HostProject` is packaged for it, then it builds and runs `sk.ls` posing. | NFR-02 |
| ATS-05 | Given the plugin descriptor and module build files, then no dependency is an experimental engine plugin. | C-07 |

## Grammar assets

| ID | Given / when / then | Verifies |
|---|---|---|
| ATS-10 | Given the Content Browser, when a Cambriana grammar is created, then it opens in the grammar editor with the default grammar and a generated seed. | FR-02, FR-08 |
| ATS-11 | Given two newly created grammar assets, then their seeds differ. | FR-08 |
| ATS-12 | Given a `.ls` file, when dropped into the Content Browser or imported, then a grammar asset in the Cambriana grammar format is created and builds the same bone tree as the legacy file. | FR-03 |
| ATS-13 | Given an imported grammar, when its source file changes on disk, then reimport updates the asset. | FR-04 |
| ATS-14 | Given a grammar asset, when exported, then the written text parses to an equal grammar. | FR-04 |
| ATS-15 | Given a grammar asset with edited text, parameters, seed, size, surfacer, UV and bake settings and baked outputs, when saved and the editor restarted, then every value is restored. | FR-01 |

## Grammar editor

| ID | Given / when / then | Verifies |
|---|---|---|
| ATS-20 | Given a grammar asset, when opened, then the editor shows rules, viewport, details and outputs. | FR-05, FR-06, FR-07 |
| ATS-21 | Given a rules edit, then the preview updates and errors and warnings appear at the reported lines. | FR-05, FR-06 |
| ATS-22 | Given a parameter change in Details, then the preview updates. | FR-07 |
| ATS-23 | Given the new-seed button or a typed seed, then the stored seed changes and the preview rebuilds. | FR-08 |
| ATS-25 | Given blob primitives placed in the viewport, then they combine with grown parts into one skinned preview mesh. | FR-21 |

## Live actor

| ID | Given / when / then | Verifies |
|---|---|---|
| ATS-30 | Given a grammar asset dragged into a level, then a live actor referencing it is placed. | FR-10 |
| ATS-31 | Given a change to a grammar asset, then every live actor referencing it rebuilds. | FR-10 |
| ATS-32 | Given a live actor with inline `sk.ls` text, when Basic Angle changes, then the rebuild completes within one editor frame. | FR-11, NFR-03 |
| ATS-33 | Given a live actor with `TREE11.LS` at recursion 12, when any property changes, then the rebuild completes in under 1 s. | NFR-03 |
| ATS-34 | Given a live actor, then its vertex positions and triangle winding equal the core's output. | C-04 |
| ATS-35 | Given two newly placed live actors without a grammar asset, then each has a generated seed and the seeds differ. | FR-08 |

## Bake

| ID | Given / when / then | Verifies |
|---|---|---|
| ATS-40 | Given `sk.ls`, when baked, then a skeletal mesh with 80 bones plus any root, 4 material slots, a skeleton and a physics asset are created. | FR-12, FR-20 |
| ATS-41 | Given static mesh output selected, when baked, then a static mesh is created. | FR-12 |
| ATS-42 | Given the baked mesh at bind pose in a level, then its vertices match the live actor's within 0.001 cm. | FR-12 |
| ATS-43 | Given a baked asset placed in a level, when Regenerate runs after a shape change, then the asset updates in place and the placed instance shows the change. | FR-13 |
| ATS-44 | Given a detached baked asset, when its grammar changes, then the asset is unchanged. | FR-13 |
| ATS-45 | Given a baked mesh, then the data channels are present in the vertex slots specified in SDD-surfacing.md. | FR-19 |

## Pose and growth

| ID | Given / when / then | Verifies |
|---|---|---|
| ATS-50 | Given baked `sk.ls`, when Basic Angle is set to any value in [5, 40] degrees, then every bone's world transform matches the core redrawn at that angle within 1e-4 cm. | FR-14 |
| ATS-51 | Given a pose parameter on a placed instance, then it is editable in Details, keyframeable in Sequencer and settable from Blueprint and C++. | FR-14 |
| ATS-52 | Given two placed instances of one baked mesh, when one instance's pose value changes, then the other is unaffected. | FR-15 |
| ATS-53 | Given `sk.ls` baked at 20 generations, when the growth parameter is animated from 0 to 20, then at each key the bones match the core drawn at that recursion. | FR-16 |
| ATS-54 | Given `BranchedAlga.ls`, when baked for growth, then the editor warns that full growth cannot be animated on one skeleton. | FR-16 |
| ATS-55 | Given a grammar using `~`, when growth is scrubbed, then no bone jumps between adjacent keys. | FR-16 |

## Run-time generation

| ID | Given / when / then | Verifies |
|---|---|---|
| ATS-70 | Given a packaged game on each supported platform, when a creature definition is generated at run time, then a rigged, skinned mesh appears, built off the game thread, and matches the editor bake of the same definition within 0.001 cm. | FR-18 |
| ATS-71 | Given a run-time generated creature, when its definition is serialised and regenerated on each supported platform, then every result is identical. | FR-25 |
| ATS-72 | Given a run-time generated mesh, then pose parameters, growth and Animation Blueprints behave as on a baked mesh. | FR-26 |
| ATS-73 | Given a generation in progress, when the requesting actor is destroyed, then generation is cancelled and its memory released. | NFR-05 |

## Integrations and scripting

| ID | Given / when / then | Verifies |
|---|---|---|
| ATS-60 | Given the PCG node with a baked grammar, then scattered instances use varied seeds and per-instance pose values. | FR-17 |
| ATS-61 | Given an Editor Utility Blueprint and a Python script, then each can create, import, bake and regenerate. | FR-22 |
| ATS-62 | Given the plugin's description and About text, then each contains the Lparser credit. | FR-23 |
