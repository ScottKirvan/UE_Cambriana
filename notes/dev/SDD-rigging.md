# Software Design Description: Rigging and Animation

Requirements: [SRS.md](SRS.md) (FR-12 to FR-16, FR-20, NFR-06).

**Decided**: output is rigged skeletal meshes; parameters are editable on the result.
Mechanisms: `[Proposed — unconfirmed]`.

## Bone tree

- Each drawn segment (`F`, `Z`) is a bone; parent is the previous bone in the chain. `[` and
  `]` push and pop the current bone with the turtle state.
- `f`, `g`, `z` do not change the current bone.
- Each emitted `{ }` polygon gets an undrawn, zero-length bone with the turtle frame at `{`,
  parented to the current bone. The polygon's vertices are bound to it.

| Sample | Bones | Drawn | Polygon |
|---|---|---|---|
| `sk.ls` | 80 (4 roots) | 80 | 0 |
| `BranchedAlga.ls` | 422 | 422 | 0 |
| `AIRHORSE.LS` | 676 | 586 | 90 |
| `TREE11.LS` | 7,952 | 3,248 | 4,704 |

## Skinning

- Tubes and blocks: each vertex bound with weight 1 to the bone that placed it. Posed to any
  pose-parameter value, the mesh matches the mesh regenerated at that value within 1e-6 cm
  at default size.
- Polygons: bound with weight 1 to their attachment bone. Polygons whose own turns use the
  default angle get one bone per vertex instead, each at the turtle frame where that vertex
  was recorded. To be validated against the user stories.
- Blob skins: multiple influences per vertex from field contributions ([SDD-surfacing.md](SDD-surfacing.md)).
- Baking applies Unreal's per-vertex influence limit: strongest influences kept, renormalised.

## Parameter tiers

| Tier | Examples | Change | Editable in |
|---|---|---|---|
| Pose | Basic angle, twist, droop | Bone rotations | Asset editor, instance details, Sequencer, Blueprint, game |
| Shape | Thickness, lengths, taper | Mesh rebuild, same skeleton | Editor |
| Structure | Generations, rules, seed, axiom | Mesh and skeleton rebuild | Editor |

A parameter is pose-tier only if used solely in rotation commands; the parser enforces this.

## Run-time posing

- Bake stores each bone's rotation record (core repo, `notes/dev/SDD-core.md`).
- At run time, rotations are re-evaluated with current parameter values and composed into
  the bone's local rotation. Translations stay at bind values.
- Requirement: every bone's transform matches the core redrawn at the same parameter values
  within 1e-4 cm.

## Growth

- Fractional recursion within the last generation maps to bone scale and translation.
- Full growth: bake at the full generation count; each bone carries its creating
  generation; young bones animate from zero length.
- Grammars that replace drawn segments with several segments (e.g. `F=F[+F]F`) cannot
  animate full growth on one skeleton; the parser warns.

## Bind pose

Bind at the midpoint of each pose parameter's range. Extra rings near joints with large
bends.

## Physics

Physics asset: one capsule per bone (length, radius), mass from volume, stiffness from
thickness. Consecutive short bones may merge into one capsule.

## Bone names

- Module path, e.g. `Spine_3/BranchA_1/Seg_2`. Converted legacy grammars use their
  single-character module names.
- Unchanged structure keeps names across regeneration. The editor reports renamed bones
  after a structure change.

## Pruning

`[Proposed — unconfirmed]`, for AR-02 ([SRS.md](SRS.md)): a pruned branch removes its bones
and all descendants; bone indices of the remaining tree are compacted and names are kept.
