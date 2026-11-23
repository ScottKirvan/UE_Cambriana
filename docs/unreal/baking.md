---
outline: [2, 3]
---

# Baking

Bake turns a grammar into standard engine assets. Nothing Cambriana-specific is needed to use them: place them, assign materials and animate them like any imported mesh.

There are four ways to bake:

| From | Writes | Built from |
|---|---|---|
| A grammar asset's right-click **Bake to Static Mesh** | A static mesh | The grammar asset's saved state |
| A grammar asset's right-click **Bake to Skeletal Mesh** | Skeletal mesh, skeleton and physics asset | The grammar asset's saved state |
| The grammar editor's toolbar **Bake > Bake to Static Mesh** | A static mesh | The grammar asset's saved state |
| The grammar editor's toolbar **Bake > Bake to Skeletal Mesh** | Skeletal mesh, skeleton and physics asset | The grammar asset's saved state |
| A live actor's Details **Bake** button | A static mesh | That actor's own settings, overrides and seed |

## Baking a grammar asset

In the Content Browser, right-click one or more grammar assets and choose **Bake to Static Mesh** or **Bake to Skeletal Mesh**. Each choice writes only its own assets, so the two can be used one after the other on the same grammar. Each selected grammar is baked in turn, and a notification shows each result: `Baked <path>` on success, or `<grammar> was not baked; see the Output Log`.

Bake builds the grammar as the grammar editor's preview does: with the asset's text, seed, settings and stored parameter values, and no live actor overrides.

### Saved state only

Both choices write from what is saved on disk. When the grammar asset has unsaved changes, or has never been saved, the bake first shows the engine's save prompt. If the asset is not saved there, nothing is baked.

Edits still open in the grammar editor are not part of the asset until the editor saves them. Baking from the right-click menu leaves them out. Baking from the grammar editor's toolbar asks whether to save them first: **Yes** saves them and bakes the saved grammar, and **No** bakes nothing.

### Where the assets go

The assets go into a folder beside the grammar asset, named after it, and are named after the grammar with the usual Unreal prefixes. For a grammar `/Game/Trees/Fern`:

| Asset | Path | Written by |
|---|---|---|
| Static mesh | `/Game/Trees/Fern/SM_Fern` | Bake to Static Mesh |
| Skeletal mesh | `/Game/Trees/Fern/SK_Fern` | Bake to Skeletal Mesh |
| Skeleton | `/Game/Trees/Fern/SKEL_Fern` | Bake to Skeletal Mesh |
| Physics asset | `/Game/Trees/Fern/PHYS_Fern` | Bake to Skeletal Mesh |

The baked assets are saved as soon as they are written.

### Bake settings

The grammar asset's **Bake** category holds one setting, **Physics Stiffness**, 25000 by default: how stiffly the physics asset holds the mesh in its bind pose. See [Stiffness](#stiffness). It applies to Bake to Skeletal Mesh.

### Rebaking

When any of the assets the chosen bake would write already exists, it asks once. Assets the other choice writes are not considered: with `SM_Fern` present, a first Bake to Skeletal Mesh does not ask.

> Do you want to overwrite the existing asset?
>
> An asset already exists at the bake location: `<path>`

The path named is the first existing one of that choice's assets, in the order skeletal mesh, skeleton, physics asset for Bake to Skeletal Mesh.

- **Yes** rebuilds every existing asset of that choice in place, as the same object, so everything that references it, such as placed actors and Blueprints, keeps working. A rebuilt mesh keeps the material assigned to each slot whose name it already had. The skeleton's bone tree follows any change to the grammar's bones. The physics asset's bodies and constraints are cleared and generated again, so hand edits to them are replaced.
- **No** bakes nothing and changes nothing.

### When nothing is baked

Bake writes nothing, leaves any existing baked assets unchanged and logs the reason to the Output Log (category `LogCambrianaBake`) when:

- the grammar asset is not saved;
- the grammar has errors;
- the grammar builds no triangles, including empty text;
- the grammar's bone tree cannot make a skeleton;
- an asset of a different type already holds one of the chosen bake's names, for example a texture called `SK_Fern`. That asset is left alone;
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

**Bodies.** Every drawn bone (`F`, `Z`) gets its own body, holding one capsule centred on the bone and along it, with the bone's radius and length. `Root` and undrawn bones have no body. `sk.ls`, with 80 bones, bakes 80 bodies.

**Mass.** Each body's mass is the engine's default for a body, from its capsule's volume.

**Constraints.** There is a joint at every bone: each body is constrained to the body of its parent bone, with limited angular motion, and collision between the two bodies off. So the mesh can bend at every bone, like a tentacle. The bodies on the bones directly under `Root` have no body above them and are not constrained. `sk.ls` bakes 76 constraints: one at each bone except its 4 bones under `Root`.

### Stiffness

Each constraint's angular drive pulls its body back to the bind pose, so the mesh springs back after being pushed. The drive is a SLERP angular drive in force mode, with no force limit.

Its stiffness follows how a beam bends: a thick, short bone is stiffer than a thin, long one.

- A body's **bending stiffness** is its bone's radius⁴ ÷ length, with the length and radius in centimetres.
- The drive's **stiffness** is the grammar asset's **Physics Stiffness** times the body's bending stiffness.
- The drive's **damping** is its stiffness ÷ 50.

Raise **Physics Stiffness** to make the whole mesh stiffer, lower it to make it floppier. At 0 the joints hang loose. At the default of 25000, a level 10 cm bone of radius 1 cm sags about 0.1 radians under gravity. A bone of radius 0 has no stiffness. Rebake to apply a new value.

Long runs of stiff joints need more solver iterations to hold their shape. Raise **Position Solver Iteration Count** in the placed skeletal mesh component's Details panel, under **Physics**: about 32 holds a 5-bone run within 1 cm of its bind pose, where the default of 8 lets its tip settle about 4 cm low. Every body of a baked physics asset uses the component's count.

## The static mesh

The static mesh `SM_<name>`, written by Bake to Static Mesh, has the same triangles, material slots and data channels as the skeletal mesh.

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
