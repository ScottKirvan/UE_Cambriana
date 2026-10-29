# Software Design Description: Unreal Integration

Requirements: [SRS.md](SRS.md). `[Proposed — unconfirmed]` unless marked.

A grammar asset bakes standard engine assets. No custom renderer or component is needed to
display baked results.

## Assets

| Asset | Contents |
|---|---|
| Cambriana grammar | Grammar text (Cambriana grammar format), source file path, parameter values, seed, size mode, surfacer and UV settings, bake settings, list of baked outputs |
| Skeletal mesh | Standard; plus metadata: source grammar, parameter snapshot, per-bone rotation records, bone kinds, generations |
| Skeleton | Standard |
| Physics asset | Standard |
| Static mesh | Standard |
| Growth animation | Standard animation sequence; optional |

- Baked assets are written to a folder beside the grammar asset.
- **Regenerate** rebuilds a baked asset in place from its stored grammar and parameter
  snapshot; references to it remain valid.
- **Detach** removes a baked asset's link to its grammar.
- Each variant (seed, axiom) gets its own skeleton.
- The pose component drives any variant from its per-bone metadata, independent of bone names.

## Source files

- Import: `.lsys` (Cambriana grammar), `.ls` (Lparser) and `.l` with `.v` (cpfg) files, by drag
  and drop and by the import dialog.
  `.ls` files are converted on import (**Decided**).
- The grammar asset stores the source file path and uses Unreal's reimport mechanism when
  the file changes. Reimporting a `.ls` file replaces the grammar text with a new conversion.
- Export writes the grammar asset's text to a file.

## Grammar asset editor

Standalone asset editor, opened from the grammar asset.

| Panel | Contents |
|---|---|
| Rules | Text editor; syntax colouring; warnings and errors at line and column |
| Viewport | Transient live result; skeleton overlay; pose and growth scrubbing; blob primitive placement and editing |
| Details | Settings and parameters grouped by tier; seed field and new-seed button; size mode and target |
| Outputs | Baked assets; Bake, Regenerate, Detach |

- The editor layout reserves space for a variant grid (AR-01).

## Live actor

- Dynamic-mesh actor. Rebuilds through the core on property change.
- Properties: recursion, basic angle, thickness, min thickness, seed, closed form, sides,
  size mode and value; a grammar asset reference or inline grammar text. Inline `.ls` text
  is converted.
- Dragging a grammar asset into a level spawns a live actor referencing it. A change to the
  grammar asset rebuilds every live actor referencing it.
- No skeleton.

## Seed

**Decided**:

- A new grammar asset or live actor gets a newly generated seed, stored on the object.
- Seed UI: an editable number field and a button that generates a new seed.

`[Proposed — unconfirmed]`:

- New seeds come from a non-deterministic source (time or the OS random source).
- A grammar file's `seed` setting, if present, is used on import. Converted `.ls` files get
  a generated seed.

## Run time

- Pose component on a skeletal mesh actor: holds per-instance pose and growth values and
  applies them from the mesh's per-bone rotation records. Values are properties,
  keyframeable in Sequencer and settable from Blueprint and C++.
## Run-time generation

**Decided**: FR-18 ([SRS.md](SRS.md)).

`[Proposed — unconfirmed]`:

- Input: a creature definition: grammar text, seed and parameter values. The same definition
  serialises to a compact, versioned form (FR-25).
- The core runs on a worker thread. Skinned mesh render data, skeleton and reference pose are
  built in the runtime module without editor-only calls; `USkeletalMesh::Build` is
  editor-only in 5.8.
- Reference implementation for constructing skeletal mesh render data at run time: Epic's
  Mutable plugin (`Engine/Plugins/Mutable`, runtime module `CustomizableObject`). Reference
  only; not a dependency.
- Editor bake and run-time generation share this construction path. Bake additionally saves
  the result as assets.
- Run-time generated meshes use the same pose component, growth parameter and animation
  integration as baked meshes (FR-26).
- Physics bodies for run-time meshes are built from the bone tree on the same worker
  thread.
- Generation cost and memory are bounded by the caller-set budgets of the core.

## Scripting

- An editor subsystem exposes create, import, bake, regenerate and detach to Blueprint and
  Python.
- A runtime function library exposes pose and growth control.

## Integrations

| Integration | Function |
|---|---|
| PCG | Node scattering baked variants with per-instance pose values |
| Modeling Mode | Tool placing a static result without a grammar asset |

## Modules

| Module | Type | Contents |
|---|---|---|
| CambrianaCore | External | Core headers and prebuilt libraries (**Decided**: prebuilt) |
| Cambriana | Runtime | Asset types, live actor, generation path, pose component, runtime function library |
| CambrianaEditor | Editor | Asset editor, importers, factories, bake, editor subsystem, integrations |

Engine dependencies: stable modules only; no experimental engine plugins.
