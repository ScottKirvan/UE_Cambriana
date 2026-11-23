# Getting Started in Unreal

Cambriana turns a grammar, a short text that describes how a form grows, into meshes in Unreal Engine 5.8. You write or import the grammar as a **grammar asset**, see the result live in a level, and **bake** it into standard engine assets: a skeletal mesh with its skeleton and physics asset, and optionally a static mesh.

This page walks through the whole loop once. The other pages cover each part in full:

- [Grammar Assets](./grammar-assets): creating, importing, reimporting and exporting grammars, and their settings and parameters.
- [Grammar Editor](./grammar-editor): editing the rules with a live preview, and saving.
- [Live Actor](./live-actor): a grammar placed in a level, and its overrides.
- [Baking](./baking): the assets Bake writes, where they go, and rebaking.
- [Data Channels](./data-channels): the per-vertex values a material can read.

For the grammar language itself, see the [Syntax Guide](../grammar-guide).

## 1. Create a grammar asset

In the Content Browser, click **Add** and choose **Boju Studios > Cambriana Grammar**. Name the new asset, for example `Fern`.

The new asset opens in the grammar editor. It holds a small default grammar:

```text
generations 4
angle       25
thickness   50%

axiom A

grow {
    A -> F ^ ' A
}
```

To start from a file instead, import a `.lsys` grammar or a legacy Lparser `.ls` file. See [Importing a file](./grammar-assets#importing-a-file).

## 2. Edit the rules

The grammar editor has four tabs: **Rules**, **Viewport**, **Details** and **Outputs**.

Type in **Rules**. When you pause typing, the **Viewport** rebuilds the preview, and any errors or warnings appear in the list under the text, each with its line and column. Try changing `angle 25` to `angle 40`.

Your edits stay in the editor until you save. Click **Save** in the editor's toolbar, or press **Ctrl+S**. The tab title shows `*` while there are unsaved edits.

## 3. Place it in a level

Drag the grammar asset from the Content Browser into the level. This places a **Cambriana Live Actor** that references the asset and builds its mesh.

Select the actor and open its **Details** panel. The **Cambriana** category holds the grammar reference, the seed and the settings the actor can override, such as **Recursion** and **Basic Angle**. Tick an override's checkbox and change its value to change this actor only. See [Live Actor](./live-actor).

Every live actor that references the asset rebuilds when the asset is saved.

## 4. Bake

In the Content Browser, right-click the grammar asset and choose **Bake**.

Bake writes these assets to a folder named after the grammar, beside it. For `/Game/Fern`:

| Asset | Path |
|---|---|
| Skeletal mesh | `/Game/Fern/SK_Fern` |
| Skeleton | `/Game/Fern/SKEL_Fern` |
| Physics asset | `/Game/Fern/PHYS_Fern` |
| Static mesh, when **Static Mesh** is ticked in the asset's Bake settings | `/Game/Fern/SM_Fern` |

The baked assets are ordinary engine assets: place them, assign materials to their slots and use them like any imported mesh. See [Baking](./baking).

To bake one placed actor with its own overrides as a static mesh, click **Bake** at the end of the actor's **Cambriana** category in Details. See [Baking from a live actor](./baking#baking-from-a-live-actor).
