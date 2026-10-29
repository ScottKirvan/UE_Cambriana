# Cambriana developer specs

## Status markings

| Marking | Meaning |
|---|---|
| **Decided** | Requirement. |
| `[Proposed — unconfirmed]` | Working assumption until the author confirms or changes it. |
| **Open** | Needs a decision; listed in [TBD.md](TBD.md). |

Numbers in the specs are required values.

## Documents

| Document | Type | Contents |
|---|---|---|
| [SRS.md](SRS.md) | Software Requirements Specification | Definitions, user stories, functional and non-functional requirements, constraints, anticipated requirements |
| [SDD-architecture.md](SDD-architecture.md) | Software Design Description | Layers, core delivery, data flow, coordinates, size, engine modules, repo layout |
| [SDD-rigging.md](SDD-rigging.md) | Software Design Description | Bone tree, skinning, parameter tiers, posing, growth, physics, bone names, pruning |
| [SDD-surfacing.md](SDD-surfacing.md) | Software Design Description | Tubes, blocks, polygons, blobs, UVs, data channels, materials |
| [SDD-unreal.md](SDD-unreal.md) | Software Design Description | Assets, source files, editor, live actor, seed, run time, scripting, modules |
| [FMT-grammar.md](FMT-grammar.md) | Format Specification | New grammar format |
| [ATS.md](ATS.md) | Acceptance Test Specification | Plugin acceptance tests |
| [roadmap.md](roadmap.md) | Plan | Milestone order and the tests each must pass |
| [TBD.md](TBD.md) | Open Issues | Decisions still needed |
| [mistakes.md](mistakes.md) | Process log | See [CLAUDE.md](../../CLAUDE.md) |

In CambrianaCore, `notes/dev/`:

| Document | Type | Contents |
|---|---|---|
| SDD-core.md | Software Design Description | Core data model, behaviour, anticipated-requirement obligations, performance, build, tests |
| FMT-legacy-ls.md | Format Specification | Legacy `.ls` dialect, golden values |
| ATS-core.md | Acceptance Test Specification | Core acceptance tests |
| roadmap.md | Plan | Core milestones |

## Reference material

| Path | Contents |
|---|---|
| `E:\1\Brazil\lsys-core\` | Reference C++ implementation of the legacy dialect, bone tree, skinning and glTF export |
| `E:\1\Brazil\lsys-playground\` | Reference JavaScript implementation and viewer pages |
| `E:\1\Brazil\cvs\BlurBeta\src\BlurBeta\MaxPlugins\objects\LParserMax\` | Original 3ds Max plugin source; defines the legacy dialect |
| `E:\1\Brazil\cvs\BlurBeta\doc\archive_docs\LSystem\` | Sample `.ls` files |

No code from the reference material is copied into this repo or the core repo.
