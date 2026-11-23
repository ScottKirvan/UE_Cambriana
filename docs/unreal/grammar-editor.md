---
outline: [2, 3]
---

# Grammar Editor

Double-click a grammar asset to open the **Cambriana Grammar Editor**. It is a playground: everything you change happens on a working copy, and nothing reaches the asset, or the live actors placed from it, until you save.

The editor has four tabs, which you can reopen from the editor's **Window** menu if you close them.

## Rules

**Rules** shows the grammar text. Edit it as plain text.

When you stop typing for half a second, the edit reaches the working copy and the preview rebuilds. It also does so as soon as the text loses focus, and before a save, a close, or the editor's own Undo and Redo.

Under the text, a list shows the problems the preview build found, errors and warnings, in the order they were reported. Each row reads `line L, column C: <problem>`, or `production position P: <problem>` for a problem with no line. The list is replaced after every rebuild, so a problem goes away once the text is fixed. A grammar with errors builds an empty preview.

See [Reading errors](../grammar-guide#reading-errors) in the Syntax Guide for what each problem means.

## Viewport

**Viewport** shows the working copy's result, built as a live actor that references the grammar would build it, with every override off and the grammar's own seed. Each material ID shows in a preview colour: IDs 0 to 9, repeating, are black, brown, red, orange, yellow, green, blue, violet, grey and white.

- The camera orbits the mesh. It frames the mesh when the editor opens; press **F** to frame it again. Edits do not move the camera.
- After each rebuild the floor moves to the mesh's lowest point, so the mesh rests on it.
- The preview is not saved in the asset.

## Details

**Details** shows the working copy's settings: the seed, the size, surfacer and bake settings, and, for `.lsys` text, the parameters grouped by tier. The grammar text is not shown here, since **Rules** edits it, and neither is the asset's source file path. See [Settings](./grammar-assets#settings) and [Parameters](./grammar-assets#parameters).

A change in Details rebuilds the preview.

## Outputs

**Outputs** is an empty tab.

## Saving

**Save** (the editor toolbar's Save button, **Ctrl+S**, or **File > Save**) copies the working copy into the asset and saves the asset's package. Live actors that reference the asset rebuild once the save completes. When the save does not complete, because it was cancelled or failed, the asset keeps its previous values and the editor keeps your edits.

The editor tab's title shows `*` while you have unsaved edits: while the working copy differs from the asset, or a Rules edit is waiting for the typing pause. Undoing back to the saved state clears it. Edits in the editor do not mark the asset as modified.

**Save As** (**File > Save As**) asks for a new asset in the **Save Grammar As** dialog, writes the working copy to it and saves it, then closes this editor and opens one on the new asset. The original asset and its live actors are untouched. Save As never replaces an existing asset: choosing a name that already exists shows a message and saves nothing. Choosing the asset's own name is a Save.

Saving the asset from outside the editor, for example with **Save All**, saves the asset as it is and leaves the editor's unsaved edits alone.

When the asset changes from outside the editor, for example by a reimport, and the editor has no unsaved edits, the editor takes the asset's new values. With unsaved edits it keeps yours.

## Undo

Edits in the editor are undoable. A Rules edit is one undo step, from the typing pause; an undo during the pause removes exactly what you typed.

## Closing with unsaved edits

Closing the editor's tab with unsaved edits asks `Save your changes to <asset>?`:

- **Yes** saves, as Save does, then closes. If the save does not complete, the editor stays open with your edits.
- **No** discards the edits and closes.
- **Cancel** keeps the editor open.

When the editor is closed by something that cannot be stopped, such as deleting or reloading the asset, or closing every editor at shutdown, the question offers only **Yes** and **No**. After **Yes**, it asks again if the save does not complete.

Edits discarded at the question are not asked about again unless you edit again.
