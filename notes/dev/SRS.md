# Software Requirements Specification

Cambriana: an Unreal Engine plugin that generates rigged skeletal meshes from L-system
grammars.

Status markings: [README.md](README.md). Open items: [TBD.md](TBD.md).

## 1. Definitions

| Term | Meaning |
|---|---|
| Grammar | Settings, parameters, axioms and rules, in the Cambriana grammar format |
| Legacy dialect | The Lparser `.ls` file format; converted to the Cambriana grammar format on import |
| Production | The result of rewriting the axiom for a number of generations |
| Turtle | The interpreter that turns a production into geometry |
| Bone tree | The bones the turtle emits, with parent links |
| Surfacer | The algorithm that builds a skinned surface around the bone tree: tubes, blocks, polygons, blobs |
| Pose parameter | A parameter that only rotates bones; animatable on a baked skeleton |
| Shape parameter | A parameter that changes geometry but not the bone tree |
| Structure parameter | A parameter that changes the bone tree |
| Grammar asset | The Unreal asset holding a grammar and its settings |
| Live actor | A level actor that regenerates a grammar's mesh on change; no skeleton |
| Bake | Writing standard Unreal assets (skeletal mesh, skeleton, physics asset, static mesh) from a grammar |
| Core | The engine-independent library, CambrianaCore |

## 2. User stories

| ID | As a | I want to | So that |
|---|---|---|---|
| US-01 | Artist | create a new grammar asset from the Content Browser and see a default form at once | I start from something that works |
| US-02 | Artist | import an existing Lparser `.ls` file or cpfg `.l` model and reimport it when the file changes on disk | existing L-system files are usable as they are |
| US-03 | Technical artist | edit grammar text with a live preview and errors shown at the right line | I see the effect of every edit |
| US-04 | Artist | adjust generations, angle, thickness, size, sides and seed with immediate feedback, and roll a new seed with one click | I can explore variations quickly |
| US-05 | Level designer | drag a grammar asset into a level and see it there without baking | I judge it in context before committing |
| US-06 | Artist | bake a grammar to a rigged skeletal mesh, or a static mesh | I use it like any other mesh asset |
| US-07 | Artist | change a grammar after baking and update every placed instance | iteration does not mean re-placing objects |
| US-08 | Animator | keyframe pose parameters such as curl and droop in Sequencer | I animate the form without hand-rigging |
| US-09 | Gameplay programmer | set pose parameters per instance from Blueprint or C++ | objects respond to wind, players and game state |
| US-10 | Animator | play a grown form growing from nothing, in Sequencer and in game | growth is an animation, not a mesh swap |
| US-11 | Level designer | scatter many varied instances (different seeds and poses) with PCG or foliage tools | a level has variety without hand work |
| US-12 | Gameplay programmer | generate new rigged forms while the game runs | players or procedural systems create unique objects |
| US-13 | Technical artist | keep grammars as text in version control and edit them outside Unreal | changes diff, merge and review like code |
| US-14 | Creature artist | place blob primitives by hand and combine them with grown parts into one skinned mesh | I build creatures, not only plants |
| US-15 | Technical artist | read generation data (position along a branch, age, thickness) in materials | shading and wind follow the structure |
| US-16 | Gameplay programmer | have baked objects collide and react physically | players can push through and disturb them |
| US-17 | Technical artist | generate and bake from Editor Utility Blueprints and Python | I batch-produce assets |
| US-18 | Game developer | generate any number of creatures at run time from definitions that players or game systems create or modify | creatures need no per-creature editor work |
| US-19 | Game developer | store or transmit a creature as a compact definition and regenerate it identically on any machine | creatures persist and can be shared |
| US-20 | Game developer | animate run-time generated creatures with the same tools as baked ones | generated creatures move like authored ones |
| US-21 | Rigger | get animation controls (IK goals, FK, pole and aim targets) from hints written in the grammar, on baked and run-time generated meshes | every creature arrives with a working rig, and I start custom rigs from a generated one |

## 3. Functional requirements

| ID | Requirement | Stories |
|---|---|---|
| FR-01 | A grammar asset stores grammar text, settings, parameter values, seed, surfacer and UV settings, bake settings and its baked outputs. | US-01 |
| FR-02 | A grammar asset is created from the Content Browser with a default grammar and a generated seed. | US-01 |
| FR-03 | `.ls` files are converted to the Cambriana grammar format on import (drag and drop and the import dialog). The converted grammar builds the same bone tree as the legacy file. | US-02 |
| FR-04 | A grammar asset records its source file and reimports when that file changes; grammars export to text. | US-02, US-13 |
| FR-05 | The grammar editor has a rules text editor with syntax colouring and errors and warnings at line and column. | US-03 |
| FR-06 | The grammar editor viewport shows the result live, with a skeleton overlay and pose and growth scrubbing. | US-03, US-04 |
| FR-07 | Settings and parameters are edited in a details panel grouped by tier; every change updates the preview. | US-04 |
| FR-08 | Every new grammar asset or live actor gets a generated seed, stored on it; the seed is shown in an editable field with a new-seed button. | US-01, US-04 |
| FR-09 | Default overall object size about 2 m. Size modes Fit and Step ([SDD-architecture.md](SDD-architecture.md)). | US-04 |
| FR-10 | Dragging a grammar asset into a level places a live actor that references it and updates when the grammar changes. | US-05 |
| FR-11 | A live actor accepts inline grammar text without a grammar asset. | US-05 |
| FR-12 | Bake produces a skeletal mesh, skeleton and physics asset; static mesh output is selectable. | US-06 |
| FR-13 | Regenerate rebuilds baked assets in place with references intact; Detach unlinks a baked asset from its grammar. | US-07 |
| FR-14 | Pose parameters are editable on placed baked meshes. As Details properties, keyframeable in Sequencer, settable from Blueprint and C++. | US-08, US-09 |
| FR-15 | Pose parameter values are per instance. | US-09, US-11 |
| FR-16 | A growth parameter animates a baked mesh from nothing to full growth, in Sequencer and at run time. | US-10 |
| FR-17 | A PCG node scatters baked variants with per-instance pose values. | US-11 |
| FR-18 | Packaged games generate rigged, skinned, animatable meshes from a grammar, seed and parameter values on every supported platform. Generation runs off the game thread; the result matches an editor bake of the same inputs within 0.001 cm. | US-12, US-18 |
| FR-19 | Generated meshes carry data channels for materials ([SDD-surfacing.md](SDD-surfacing.md)). | US-15 |
| FR-20 | Baking produces a physics asset with one body per bone or merged chain. | US-16 |
| FR-21 | Blob skins and a blob API with a variety of distance-field primitives. Blob primitives are placed and edited in the grammar editor viewport and combine with grown parts into one skinned mesh. | US-14 |
| FR-22 | Generation, baking and regeneration are callable from Blueprint, Python and C++. | US-17 |
| FR-23 | The plugin credits Lparser by Laurens Lapré. Text in description and About: "Reads Lparser `.ls` files. Lparser by Laurens Lapré." | |
| FR-24 | Grammars are in the Cambriana grammar format, file extension `.lsys` ([FMT-grammar.md](FMT-grammar.md)); the legacy dialect (core repo, `notes/dev/FMT-legacy-ls.md`) is read only for conversion. | US-02, US-03 |
| FR-25 | A creature definition (grammar, seed, parameter values) serialises to a compact, versioned form; regenerating from it on any supported platform reproduces the creature exactly. | US-19 |
| FR-26 | Run-time generated meshes support pose parameters, growth and the engine's animation systems in the same way as baked meshes. | US-20 |
| FR-27 | cpfg models are converted to the Cambriana grammar format on import. The `.l` file is read with its `.v` view file; constructs that cannot be converted are reported with line and column. | US-02 |
| FR-28 | Rig hints in the grammar (FK, IK, pole, aim) produce Control Rig controls on baked and run-time generated meshes. An editor action writes a standalone Control Rig asset from a mesh's hints. | US-21 |

## 4. Non-functional requirements

| ID | Requirement |
|---|---|
| NFR-01 | Unreal Engine 5.8 only. |
| NFR-02 | Platforms Win64, Linux, Android; Mac and iOS later. |
| NFR-03 | Performance: `sk.ls` full rebuild 1 ms; `TREE11.LS` at recursion 12 full rebuild 100 ms single-threaded; pose evaluation of 1,000 bones 0.1 ms; live preview of `sk.ls` within one editor frame. |
| NFR-04 | Determinism: identical inputs and seed give identical output on every compiler and platform. |
| NFR-05 | Robustness: no grammar input crashes or hangs the editor; budgets bound memory and time; generation is cancellable. |
| NFR-06 | Bone names are stable across regeneration when the bone tree is unchanged. |
| NFR-07 | Packaged for Fab. The plugin packages for 5.8 without errors or warnings. |

## 5. Constraints

| ID | Constraint |
|---|---|
| C-01 | All grammar, turtle and geometry logic is in CambrianaCore: C++17, no Unreal dependency, built and tested outside Unreal. |
| C-02 | CambrianaCore's source is in the private repo `ScottKirvan/CambrianaCore`; this repo receives headers and prebuilt dynamic libraries. |
| C-03 | Full rewrite; no code from the reference material ([README.md](README.md)). |
| C-04 | The core uses Unreal's space: left-handed, +Z up, centimetres. |
| C-05 | The core has no format limits; exporters apply format limits. |
| C-06 | glTF is for inspection only. |
| C-07 | Engine dependencies are stable modules only; no experimental engine plugins. |

## 6. Anticipated requirements

Not yet required. The design must allow each to be added without restructuring.

| ID | Requirement | Design obligations |
|---|---|---|
| AR-01 | Mutation and breeding of grammars. | Grammar is an editable model with text round-trip; seeds are explicit inputs; the editing API is available at run time; the grammar editor can host a grid of variants. |
| AR-02 | Pruning and environment-responsive growth (obstacles, light, gravity, collision with level geometry). | Rewriting can run generation by generation with an environment query between generations; modules can carry parameters set by that query; the core defines an environment interface with no Unreal dependency; the bone tree supports removal of pruned branches. |
