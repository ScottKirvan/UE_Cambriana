---
outline: [2, 3]
---

# Live Actor

A **Cambriana Live Actor** builds a grammar's mesh in the level and rebuilds it whenever its inputs change. Use it to see a grammar in place, try settings on one instance, and bake that instance as a static mesh.

## Placing a live actor

- **From a grammar asset:** drag the asset from the Content Browser into the level. The actor references the asset, copies its seed, and starts each of its overridable settings from the grammar text where the text states it, otherwise from the asset, otherwise from the default.
- **Empty:** in the **Place Actors** panel, open the **Boju Studios** category and drag **Cambriana Live Actor** into the level. The actor has no grammar, a newly generated seed and the default settings. Set its **Grammar**, or type text into **Inline Grammar**.

Once placed, an actor's settings and seed are its own: changing the asset's later does not change them.

## Details

The actor's settings are in the **Cambriana** category of its Details panel.

| Property | What it is |
|---|---|
| **Grammar** | The grammar asset the actor builds from |
| **Inline Grammar** | Grammar text typed into the actor itself, `.lsys` or legacy `.ls`. Editable only while **Grammar** is empty |
| **Seed** | The actor's seed, with a **New Seed** button |
| **Recursion** | The number of generations; overrides the text's `generations` |
| **Basic Angle** | The turning angle in degrees; overrides the text's `angle` |
| **Thickness** | Bone thickness as a percentage of bone length; overrides the text's `thickness` |
| **Min Thickness** | Keeps every bone at least this many centimetres thick |
| **Closed Form** | On wraps each bone in a tube; off gives each bone a box |
| **Sides** | The number of sides of each tube, 3 or more |
| **Size Mode** | Fit scales the whole form so its largest dimension is **Size Value**; Step makes one bone **Size Value** long |
| **Size Value** | The target size in Fit mode, the step length in Step mode, in centimetres |

At the end of the category, **Bake** writes a static mesh from this actor. See [Baking from a live actor](./baking#baking-from-a-live-actor).

Inline `.ls` text is not converted; it builds as legacy text. The actor detects the format from the text, as a grammar asset does; see [Grammar text formats](./grammar-assets#grammar-text-formats).

### Overrides

Each setting from **Recursion** to **Size Mode** has an override checkbox on its row, off by default. With the override on, the actor's own value wins over the grammar text and the grammar asset. The checkbox on **Size Mode** also covers **Size Value**.

A value is greyed while the build does not use it, and editable while it does. Turning an override off leaves the actor's value as it is: the build takes the setting from elsewhere, and turning the override back on builds with the kept value again.

### Where each setting comes from

Each setting is taken from the first of these that gives it:

| Setting | 1 | 2 | 3 | 4 |
|---|---|---|---|---|
| **Recursion**, **Basic Angle**, **Thickness** | The actor, override on | The grammar text (`generations`, `angle`, `thickness`, or the `.ls` header) | The default | |
| **Min Thickness**, **Closed Form**, **Sides** | The actor, override on | The `.lsys` text (`minthickness`, `surface`, `sides`) | The grammar asset | The actor's own value, for inline text |
| **Size Mode**, **Size Value** | The actor, override on | The `.lsys` text (`size` or `step`) | The grammar asset | For inline text: the default size for `.lsys`, the actor's own value for `.ls` |

The defaults are recursion 4, basic angle 25, thickness 50, min thickness 0 cm, closed form on, 8 sides, and Fit at 200 cm.

So, with inline text, **Min Thickness**, **Closed Form** and **Sides** are editable without their override when the text does not state them, and **Size Mode** and **Size Value** are editable without their override for legacy `.ls` text.

### Resetting a setting

The reset arrow on each overridable setting's row shows while the actor's value differs from the value it would start from if placed now: the grammar text's, else the grammar asset's, else the default. Clicking it sets the actor's value to that, as one undoable step. It leaves the override checkbox as it is. **Size Mode** and **Size Value** reset separately.

With several actors selected, the arrow shows when any of them differs, and resets each one to its own starting value.

## Seed

A live actor always builds with its own **Seed**. Placed from a grammar asset, it starts with the asset's seed; placed empty, it gets a newly generated one. Click **New Seed** to generate a new random seed; with several actors selected, each gets its own new seed.

The actor's seed and the asset's are independent: changing one leaves the other.

## Parameters

An actor that references a grammar asset builds with the parameter values stored on the asset; see [Parameters](./grammar-assets#parameters). An actor with inline text builds with the text's own parameter values.

## When the actor rebuilds

The actor rebuilds when anything its build uses changes: its own properties, or the grammar asset it references. It does not rebuild when you move it.

A grammar asset changes for its actors when the grammar editor saves it, when it is reimported, or when it is edited outside the grammar editor. Edits in the grammar editor that are not saved do not reach the actors.

Errors in the grammar leave the actor's mesh empty, and are written to the Output Log with their line and column. The actor's material IDs show in the preview colours listed under [Viewport](./grammar-editor#viewport).

The live actor has no skeleton. To get a skeletal mesh, [bake](./baking) the grammar asset.
