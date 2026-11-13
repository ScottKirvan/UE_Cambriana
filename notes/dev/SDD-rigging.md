# Software Design Description: Rigging and Animation

Requirements: [SRS.md](SRS.md) (FR-12 to FR-16, FR-20, NFR-06).

Output is rigged skeletal meshes; parameters are editable on the result.

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

- A baked mesh stores its definition: grammar, seed and parameter values.
- Pose evaluation is a core call: the turtle re-walks the stored production with the current
  pose values and returns each bone's local rotation. No rewriting or surfacing.
  Translations stay at bind values.
- One core model per baked mesh is shared by its instances; each instance holds only its pose
  values.
- Every bone's transform matches the core redrawn at the same parameter values
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

- `[Proposed — unconfirmed]` A chain is a maximal run of drawn-segment bones in which each bone
  after the first is the only drawn child of the bone before it and has no chain break before
  it (the core's `chain_break_before`: after `]`, `f`, `z`, `g`, as for tubes in
  [SDD-surfacing.md](SDD-surfacing.md)). A fork ends a chain: each branch, the continuing one
  included, starts its own. `sk.ls` has 4 chains of 20 bones.
- `[Proposed — unconfirmed]` The physics asset has one body per chain, on the chain's first bone,
  holding one capsule per bone of the chain: centred on the bone, along it, with the bone's
  radius and length. Bones in no chain (undrawn bones and `Root`) have no body. Mass comes from
  the capsules' volume, the engine's default for a body.
- `[Proposed — unconfirmed]` Each body is constrained to the body of the nearest bone above it
  that has one, with the parameters of the FBX skeletal mesh import's physics asset
  (`FPhysAssetCreateParams` defaults: limited angular motion, collision between bodies off),
  as `FPhysicsAssetUtils::CreateFromSkeletalMesh` joins a body to its parent's
  (`PhysicsAssetUtils.cpp`). A body with none above it, such as each of `sk.ls`'s four, is
  unconstrained.

## Rig controls

- Rig hints ([FMT-grammar.md](FMT-grammar.md)) are carried on bones by the core and stored in
  the mesh metadata.
- The plugin ships one shared Control Rig. Its construction event reads the mesh's hints and
  creates the controls: FK controls, IK goals with their chains, pole targets, aim controls.
  Baked and run-time generated meshes use the same rig; no Control Rig asset is generated per
  mesh.
- Controls act on the posed skeleton: pose evaluation sets bone rotations first.
- An editor action writes a standalone Control Rig asset from a mesh's hints, as a starting
  point for custom rigs. It does not change the shared rig.
- Controls whose bones are pruned are removed with them.

## Bone names

- Structural and short: derived only from the bone's place in the tree and its creating
  symbol. Long path names are not used. Format `[Proposed — unconfirmed]`: symbol, `_`,
  depth-first index (`F_12`; polygon attachment bones `P_7`).
- Unchanged structure keeps names across regeneration. The editor reports renamed bones
  after a structure change.

## Pruning

For AR-02 ([SRS.md](SRS.md)): a pruned branch removes its bones
and all descendants; bone indices of the remaining tree are compacted and names are kept.

## Bake skeleton

- One synthetic `Root` bone parents the generated bone tree.
- The bind pose is the grammar's own parameter values, until pose parameters
  (M5).
- `[Proposed — unconfirmed]` `Root` is bone 0, at the origin, unrotated; the core's bone `i`
  is bone `i + 1`, and the core's roots are `Root`'s children. Each bone's bind pose is at its
  start, with X along the bone (the frame's Forward) and Z the frame's Up, the axes Control
  Rig's aim uses by default (`RigUnit_AimBone.h`: primary axis X, secondary Z).
- `[Proposed — unconfirmed]` The influence limit is the engine's `MAX_TOTAL_INFLUENCES` (12),
  applied as the engine's skeletal mesh import applies it
  (`LODUtilities.cpp`, `FLODUtilities::ProcessImportMeshInfluences`): a vertex's weights are
  normalised, the strongest 12 at or above `BoneWeightThreshold` kept and renormalised. A
  vertex with no influence is bound to `Root` with weight 1, as that import binds one to bone 0.
