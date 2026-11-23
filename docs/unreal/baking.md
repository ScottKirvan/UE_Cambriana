---
outline: [2, 3]
---

# Baking

Bake turns a grammar into standard engine assets. Nothing Cambriana-specific is needed to use them: place them, assign materials and animate them like any imported mesh.

There are two ways to bake:

| From | Writes | Built from |
|---|---|---|
| A grammar asset's right-click **Bake** | Skeletal mesh, skeleton and physics asset, plus a static mesh when **Static Mesh** is ticked | The grammar asset's saved state |
| A live actor's Details **Bake** button | A static mesh | That actor's own settings, overrides and seed |

## Baking a grammar asset

In the Content Browser, right-click one or more grammar assets and choose **Bake**. Each selected grammar is baked in turn, and a notification shows each result: `Baked <path>` on success, or `<grammar> was not baked; see the Output Log`.

Bake builds the grammar as the grammar editor's preview does: with the asset's text, seed, settings and stored parameter values, and no live actor overrides.

### Saved state only

Bake writes from what is saved on disk. When the grammar asset has unsaved changes, or has never been saved, Bake first shows the engine's save prompt. If the asset is not saved there, nothing is baked.

Edits still open in the grammar editor are not part of the asset until the editor saves them. Save in the grammar editor before baking.

### Where the assets go

The assets go into a folder beside the grammar asset, named after it, and are named after the grammar with the usual Unreal prefixes. For a grammar `/Game/Trees/Fern`:

| Asset | Path |
|---|---|
| Skeletal mesh | `/Game/Trees/Fern/SK_Fern` |
| Skeleton | `/Game/Trees/Fern/SKEL_Fern` |
| Physics asset | `/Game/Trees/Fern/PHYS_Fern` |
| Static mesh | `/Game/Trees/Fern/SM_Fern` |

The baked assets are saved as soon as they are written.

### Bake settings

The grammar asset's **Bake** category holds two settings:

- **Static Mesh**, off by default: also write `SM_<grammar name>`.
- **Physics Stiffness**, 25000 by default: how stiffly the physics asset holds the plant in its bind pose. See [Stiffness](#stiffness).

The skeletal mesh, skeleton and physics asset are always written.

### Rebaking

When any of the assets Bake would write already exists, Bake asks once:

> Do you want to overwrite the existing asset?
>
> An asset already exists at the bake location: `<path>`

The path named is the first existing one, in the order skeletal mesh, skeleton, physics asset, static mesh.

- **Yes** rebuilds every existing asset in place, as the same object, so everything that references it, such as placed actors and Blueprints, keeps working. A rebuilt mesh keeps the material assigned to each slot whose name it already had. The skeleton's bone tree follows any change to the grammar's bones. The physics asset's bodies and constraints are cleared and generated again, so hand edits to them are replaced.
- **No** bakes nothing and changes nothing.

### When nothing is baked

Bake writes nothing, leaves any existing baked assets unchanged and logs the reason to the Output Log (category `LogCambrianaBake`) when:

- the grammar asset is not saved;
- the grammar has errors;
- the grammar builds no triangles, including empty text;
- the grammar's bone tree cannot make a skeleton;
- an asset of a different type already holds one of the names, for example a texture called `SK_Fern`. That asset is left alone;
- you answer **No** to the overwrite question.

If the assets are built but cannot be saved, the Output Log says so.

## The skeletal mesh and skeleton

The skeletal mesh `SK_<name>` uses the skeleton `SKEL_<name>` and the physics asset `PHYS_<name>`.

**Bones.** The skeleton's first bone is `Root`, at the origin and unrotated. Below it is the grammar's bone tree: one bone per drawn segment (`F`, `Z`), and an undrawn, zero-length bone for each polygon. The bone tree's own root bones, such as each separate trunk, are children of `Root`. Bones are named after the symbol that drew them and their place in the tree, for example `F_12`.

**Bind pose.** The skeleton's reference pose is the form exactly as the grammar builds it, with the asset's settings, seed and parameter values: placed in a level at its bind pose, the skeletal mesh matches a live actor built from the same grammar asset. Each bone's bind transform sits at the bone's start, with its X axis along the bone and its Z axis the turtle's up direction.

**Skinning.** Each vertex of a tube or block is bound with full weight to the bone that drew it. A vertex keeps at most 12 influences, the engine's limit: the strongest are kept and their weights renormalised. A vertex with no influence is bound to `Root`.

**Mesh.** The mesh has one LOD and one material slot per material ID (`c` in the grammar), in ascending ID order, named `Material_<ID>`. A new slot gets the engine's default surface material. The mesh keeps the grammar's normals and computes tangents. It carries the [data channels](./data-channels) in UV1, UV2 and the vertex colour.

## The physics asset

The physics asset `PHYS_<name>` is set as the skeletal mesh's physics asset, with the skeletal mesh as its preview mesh.

**Bodies.** There is one body per **chain**. A chain is a run of drawn bones in which each bone is the only drawn child of the one before it, with no break between them, such as after a `]`, `f`, `z` or `g`. A fork ends a chain: each branch, the continuing one included, starts its own. The body sits on the chain's first bone and holds one capsule per bone of the chain, centred on the bone and along it, with the bone's radius and length. `Root` and undrawn bones have no body.

**Mass.** Each body's mass is the engine's default for a body, from its capsules' volume.

**Constraints.** Each body is constrained to the body of the nearest bone above it that has one, with limited angular motion, and collision between the two bodies off. A body with no body above it, such as the trunk's, is not constrained.

### Stiffness

Each constraint's angular drive pulls its body back to the bind pose, so the plant springs back after being pushed. The drive is a SLERP angular drive in force mode, with no force limit.

Its stiffness follows how a beam bends: a thick, short body is stiffer than a thin, long one.

- A body's **bending stiffness** is 1 ÷ Σ(length ÷ radius⁴) over its bones, with lengths and radii in centimetres. For a one-bone body that is radius⁴ ÷ length.
- The drive's **stiffness** is the grammar asset's **Physics Stiffness** times the body's bending stiffness.
- The drive's **damping** is its stiffness ÷ 50.

Raise **Physics Stiffness** to make the whole plant stiffer, lower it to make it floppier. At the default of 25000, a level 10 cm bone of radius 1 cm sags about 0.1 radians under gravity. A body with a bone of radius 0 has no stiffness. Rebake to apply a new value.

## The static mesh

The static mesh `SM_<name>`, written when **Static Mesh** is ticked, has the same triangles, material slots and data channels as the skeletal mesh.

- One LOD, with the grammar's normals and computed tangents.
- Lightmap UVs are generated into UV3, after the data channels in UV1 and UV2.
- Very thin triangles are kept: degenerate triangle removal is off, so the mesh matches the live actor. The engine's build still drops a triangle with two corners at the same point, which has no area.
- UVs are stored at full precision.

## Baking from a live actor

Select a placed live actor and click **Bake** at the end of the **Cambriana** category in its Details panel. This writes one static mesh from the actor exactly as it shows in the level, with its own overrides, seed and grammar text. It does not change the actor, and the grammar asset's bake settings do not apply. With several actors selected, each is baked in turn.

The engine's **Bake Static Mesh** save dialog asks where to write it. It defaults to:

| Actor | Default |
|---|---|
| References a grammar asset `/Game/Trees/Fern`, labelled `Fern2` | `/Game/Trees/Fern/SM_Fern_Fern2` |
| Inline text, labelled `Fern2` | `/Game/SM_Fern2` |

- **Cancel** bakes nothing.
- Choosing an existing static mesh makes the dialog ask whether to replace it. Bake then rebuilds that static mesh in place, without asking again.
- When the actor's grammar asset has unsaved changes, the engine's save prompt shows first; if the asset is not saved, nothing is baked and the dialog does not show.

The same rules as a grammar bake apply for errors, empty results and names held by another asset type: nothing is baked and the Output Log says why. The static mesh is saved as soon as it is written.

The button is disabled in Blueprint class defaults, which have no placed actor to bake.
