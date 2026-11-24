---
outline: [2, 3]
---

# Baking

Bake turns a grammar into standard engine assets. Nothing Cambriana-specific is needed to use them: place them, assign materials and animate them like any imported mesh.

There are four ways to bake:

| From | Writes | Built from |
|---|---|---|
| A grammar asset's right-click **Bake to Static Mesh** | A static mesh | The grammar asset as it is now, saved or not |
| A grammar asset's right-click **Bake to Skeletal Mesh** | Skeletal mesh, skeleton and physics asset | The grammar asset as it is now, saved or not |
| The grammar editor's toolbar **Bake > Bake to Static Mesh** | A static mesh | The editor's current edits |
| The grammar editor's toolbar **Bake > Bake to Skeletal Mesh** | Skeletal mesh, skeleton and physics asset | The editor's current edits |
| A live actor's Details **Bake** button | A static mesh | That actor's own settings, overrides and seed |

## Baking a grammar asset

In the Content Browser, right-click one or more grammar assets and choose **Bake to Static Mesh** or **Bake to Skeletal Mesh**. Each choice writes only its own assets, so the two can be used one after the other on the same grammar. Each selected grammar is baked in turn, and a notification shows each result: `Baked <path>` on success, `<grammar> was baked but not saved; see the Output Log` when the baked assets are built but cannot be saved, or `<grammar> was not baked; see the Output Log`.

Bake builds the grammar as the grammar editor's preview does: with the asset's text, seed, settings and stored parameter values, and no live actor overrides.

### Current state

Both choices bake the grammar asset as it is now, including changes not yet saved and an asset that has never been saved. Bake asks nothing about saving and does not save the grammar asset; it saves only the assets it writes.

Edits still open in the grammar editor are not part of the asset until the editor saves them, so baking from the right-click menu leaves them out. Baking from the grammar editor's toolbar bakes those edits instead, without saving them; see [Bake](./grammar-editor#bake) in the grammar editor guide.

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

- **Yes** rebuilds every existing asset of that choice in place, as the same object, so everything that references it, such as placed actors and Blueprints, keeps working. A rebuilt mesh keeps the material assigned to each slot whose name it already had. The skeleton's bone tree follows any change to the grammar's bones. A rebuilt skeletal mesh has one LOD: LODs added to it since the last bake are removed. The physics asset's bodies and constraints are cleared and generated again, so hand edits to them are replaced, and placed skeletal meshes using it take the new bodies at once.
- **No** bakes nothing and changes nothing.

### Replacing an asset of another type

When an asset of a different type, or a redirector, holds one of the chosen bake's names, Bake asks the engine's import question, naming it, before it writes anything. A redirector is left where an asset was moved from; it is never followed, and the moved asset is never touched.

> Do you want to replace the existing asset?
>
> An asset already exists at the import location: `<path>`

- **Yes** deletes that asset, with no further confirmation, and bakes the output in its place.
- **No** bakes nothing and changes nothing.

The grammar asset's Bake to Static Mesh, Bake to Skeletal Mesh and a live actor's Details **Bake** all ask it. Reimport never replaces such an asset: it rebuilds nothing.

### When nothing is baked

Bake writes nothing, leaves any existing baked assets unchanged and logs the reason to the Output Log (category `LogCambrianaBake`) when:

- the grammar has errors;
- the grammar builds no triangles, including empty text;
- the grammar's bone tree cannot make a skeleton;
- you answer **No** to the overwrite question or to the replace question;
- an asset of a different type, or a redirector, holds one of the chosen bake's names and cannot be deleted after you answer **Yes** to the replace question. The Output Log gives the error and nothing else is deleted or written;

If the assets are built but cannot be saved, they are left in memory with unsaved changes and the Output Log says so.

## Reimport

A mesh baked from a grammar asset remembers that grammar, as an imported mesh remembers its source file. In the mesh's Details panel, **Import Settings > Source File** shows the grammar asset's package file, and **Grammar** names the grammar asset. `SM_<name>` and `SK_<name>` carry this link; the skeleton and physics asset do not.

To rebuild a baked mesh after changing its grammar, right-click the mesh in the Content Browser and choose **Reimport**. Reimport bakes the grammar again as it is now, unsaved changes included, as the matching Bake does, into the same assets:

| Reimport on | Rebuilds | As |
|---|---|---|
| `SM_<name>` | The static mesh | Bake to Static Mesh |
| `SK_<name>` | The skeletal mesh, the skeleton it uses and its physics asset, as below | Bake to Skeletal Mesh |

- The mesh is rebuilt in place, as the same object, so everything that references it, such as placed actors and Blueprints, keeps working and shows the change.
- A skeletal mesh keeps the skeleton it uses, even one shared with other skeletal meshes, and the grammar's bones are merged into it, as the engine's FBX reimport merges them: new bones are added and every bone the skeleton has is kept, so the other meshes on it keep working. A mesh without a skeleton gets `SKEL_<name>` beside the grammar.
- When the grammar's bones cannot be merged, for example because a bone the skeleton has now sits below a different parent, Reimport asks the engine's question, **FAILED TO MERGE BONES**, with its warning that animation data may be invalidated. **Yes** recreates the skeleton's bones from the mesh, which drops any bone only other meshes use; **No** and **Cancel** rebuild nothing and leave the mesh, its skeleton and its physics asset as they were.
- The physics asset is rebuilt only when it is the grammar's own `PHYS_<name>` in the folder beside the grammar. A physics asset you assigned to the mesh yourself is left as it is, as the FBX reimport keeps the mesh's physics asset, and a mesh without one is given none.
- Bake to Skeletal Mesh, by contrast, always recreates `SKEL_<name>`'s bones from the grammar.
- Apart from that question, Reimport does not ask before rebuilding, as the engine's Reimport of an imported mesh does not.
- The rebuilt assets are left with unsaved changes, as after reimporting an FBX file; save them as usual. Bake, by contrast, saves the assets it writes.
- The link follows the grammar asset when it is renamed or moved, and Reimport updates the Source File to the grammar's new package file.
- When Reimport bakes nothing, for the reasons under [When nothing is baked](#when-nothing-is-baked), the mesh is unchanged and the engine's notification says the reimport failed.

### Reimport With New File

**Reimport With New File** links the mesh to another grammar asset and rebuilds it from that grammar. In the file dialog, choose that grammar asset's package file: its `.uasset` file under the project's `Content` folder, for example `Content/Trees/Fern2.uasset` for `/Game/Trees/Fern2`. Any other file, including another kind of asset's package file, fails and leaves the mesh linked to its grammar and unchanged; the Output Log says why (category `LogCambrianaReimport`).

Meshes not baked from a grammar asset, such as meshes imported from FBX files, reimport from their own source files as usual.

### Detach from Grammar

To keep a baked mesh as it is and stop it from being rebuilt from its grammar, right-click it in the Content Browser and choose **Detach from Grammar**, in the menu's asset actions. The entry is offered on `SM_<name>` and `SK_<name>` meshes that are linked to a grammar, and detaches every selected mesh that is.

- The mesh loses its link: its Source File is cleared, as the Source File row's clear button clears it, and its **Grammar** row goes.
- The mesh itself is unchanged. Its Reimport is disabled, as for any asset without a source file, and Reimport of the grammar's other meshes leaves it alone.
- Detach is one change in the editor's undo history; **Edit > Undo** links the mesh to its grammar again. The mesh is left with unsaved changes; save it as usual.
- To link a detached mesh again, bake its grammar again: Bake rebuilds the mesh at its bake location and links it.

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
- Choosing an existing static mesh makes the dialog ask whether to replace it. Bake then rebuilds that static mesh in place, without asking again. If that static mesh was baked from a grammar asset, it loses its link to the grammar: its Source File is cleared and its Reimport is disabled, since Reimport cannot rebuild the actor's overrides.
- The actor is baked as it shows in the level, whether or not its grammar asset is saved. The grammar asset is not saved.

The same rules as a grammar bake apply for errors and empty results: nothing is baked and the Output Log says why. An asset of another type, or a redirector, at the chosen location is replaced on Yes to the engine's question as under [Replacing an asset of another type](#replacing-an-asset-of-another-type). The static mesh is saved as soon as it is written.

The button is disabled in Blueprint class defaults, which have no placed actor to bake.
