---
outline: [2, 3]
---

# Grammar Assets

A **Cambriana Grammar** asset holds a grammar's text, a seed, the values you set for its parameters, size and surfacer settings, and bake settings. Live actors build from it, and Bake writes meshes from it.

Double-click a grammar asset to open it in the [grammar editor](./grammar-editor).

## Creating a grammar asset

In the Content Browser, click **Add** and choose **Boju Studios > Cambriana Grammar**. The new asset holds a small default `.lsys` grammar and a newly generated seed, and opens in the grammar editor.

## Importing a file

Import a grammar file by dragging it into the Content Browser, or with the Content Browser's **Import** button. Two file types are accepted, chosen by the file's extension (upper or lower case):

| Extension | File type | On import |
|---|---|---|
| `.lsys` | Cambriana grammar | The text is stored unchanged |
| `.ls` | Lparser grammar | The text is converted to `.lsys` |

The asset records the file's path, for reimport.

**Seed.** When a `.lsys` file states `seed`, that becomes the asset's seed. Otherwise, and for every `.ls` file, the asset gets a newly generated seed.

**Settings.** Each of the asset's size and surfacer settings starts from the file, where the file states it, and otherwise from its default. See [Size and surfacer settings](#size-and-surfacer-settings).

**Legacy conversion.** A `.ls` file's three header numbers become `generations`, `angle` and `thickness`, and its rules become `grow` rules. See [Legacy `.ls` files](../grammar-guide#legacy-ls-files) in the Syntax Guide. When the conversion reports an error, no asset is created. Each error and warning is written to the Output Log with the file's path, line and column.

## Reimporting

An imported grammar asset's right-click menu offers **Reimport**, which reads its source file again:

- A `.lsys` file's text replaces the asset's text.
- A `.ls` file is converted again, and the conversion replaces the asset's text. When the conversion reports an error, the asset is left unchanged, and each error is written to the Output Log with its line and column.
- The asset keeps its seed, unless the file is `.lsys` and states `seed`; that seed then becomes the asset's.
- The asset's size, surfacer and bake settings and its parameter values are unchanged.

Reimport marks the asset as modified, so save it to keep the result. It can be undone. Live actors that reference the asset rebuild after a reimport.

An asset that was created in the editor, not imported, has no source file and is not offered Reimport.

## Exporting

To write a grammar asset's text to a file, right-click it and choose **Asset Actions > Export**. The file is a `.lsys` file, UTF-8 without a byte order mark.

An asset holding legacy `.ls` text exports as `.lsys`, converted on the way out. When that conversion reports an error, no file is written, and each error is written to the Output Log with the asset's path, line and column.

Export is the only operation that writes a grammar file. Saving the asset, reimporting or anything else leaves the source file on disk unchanged. Export writes only to the path you choose.

## Grammar text formats

An asset's text can be `.lsys` or legacy `.ls`. Which one is decided from the text itself: the text is legacy `.ls` when its first line that is neither blank nor a `#` comment starts with a number (an optional `+` or `-`, an optional `.`, then a digit). Anything else, including empty text, is `.lsys`.

Legacy `.ls` text typed or pasted into an asset is not converted. It builds as legacy text, has no parameters, and its size always comes from the asset or the live actor. Export converts it.

## Settings

The grammar editor's **Details** tab shows the asset's settings. The grammar text itself is edited in the **Rules** tab.

### Seed

**Grammar > Seed** is the number that drives every random choice in the grammar, such as `~` turns and weighted rules. Click **New Seed** beside it to generate a new random seed.

The asset's seed is what the grammar editor's preview and Bake build with. A `seed` statement in `.lsys` text is read when the file is imported or reimported, and becomes the asset's seed then.

A live actor has a seed of its own; see [Seed](./live-actor#seed).

### Size and surfacer settings

| Category | Property | Values | Default | What it does |
|---|---|---|---|---|
| Size | **Size Mode** | Fit, Step | Fit | Fit scales the whole form so its largest dimension is **Size Value**. Step makes one bone **Size Value** long |
| Size | **Size Value** | 0.01 cm or more | 200 cm | The target size in Fit mode, the step length in Step mode |
| Surfacer | **Closed Form** | On or off | On | On wraps each bone in a tube; off gives each bone a box. The grammar's `surface tubes` and `surface blocks` |
| Surfacer | **Sides** | 3 or more | 8 | The number of sides of each tube |
| Surfacer | **Min Thickness** | 0 cm or more | 0 cm | Keeps every bone at least this thick |

A value the `.lsys` text states wins over the asset's: the asset's **Size Mode** and **Size Value** apply only when the text states neither `size` nor `step`, and **Sides**, **Closed Form** and **Min Thickness** apply only when the text does not state `sides`, `surface` or `minthickness`. A live actor's override, when it is on, wins over both. See [Where each setting comes from](./live-actor#where-each-setting-comes-from).

The asset's settings start from the text when the asset is created or imported. Editing the text later, or reimporting, leaves them as they are.

### Bake settings

| Category | Property | Default | What it does |
|---|---|---|---|
| Bake | **Static Mesh** | Off | Bake also writes a static mesh, `SM_<grammar name>` |
| Bake | **Physics Stiffness** | 25000 | Scales how stiffly the whole baked physics asset springs back to its bind pose. 0 or more |

See [Baking](./baking).

## Parameters

For `.lsys` text, the grammar's parameters appear in Details as number fields, one category per tier, each in the order the text declares them:

| Category | Parameters declared with |
|---|---|
| **Pose Parameters** | `pose param` |
| **Shape Parameters** | `shape param` |
| **Structure Parameters** | `structure param` |
| **Parameters** | `param` |

A category appears only when the text declares a parameter of that tier. See [Parameters](../grammar-guide#parameters) in the Syntax Guide for what each tier allows.

Each field shows the value the build uses. Until you set it, that is the text's value, worked out with the values you set for earlier parameters. Committing a new value stores it on the asset, and the build uses it whatever the text says. The field's reset arrow, shown only while a value is stored, removes the stored value, so the text's value applies again. Each change is one undoable step.

Stored values are kept by the parameter's name, which is case-sensitive:

- Editing the text or reimporting leaves stored values alone. A parameter whose value you have not set follows a change to the text's value; one you have set keeps your value.
- When the text stops declaring a name, because the parameter was removed or renamed, its stored value is kept but unused and not shown. It applies again if the name is declared again.
- A parameter the text adds starts unset.

Legacy `.ls` text has no parameters, and stored values do not affect its build.

Live actors that reference the asset build with its stored parameter values. A live actor with inline text builds with the text's values.
