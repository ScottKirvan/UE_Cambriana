# Software Design Description: Unreal Integration

Requirements: [SRS.md](SRS.md).

A grammar asset bakes standard engine assets. No custom renderer or component is needed to
display baked results.

## Assets

| Asset | Contents |
|---|---|
| Cambriana grammar | Grammar text (Cambriana grammar format), source file path, parameter values, seed, size mode, surfacer and UV settings, bake settings, list of baked outputs |
| Skeletal mesh | Standard; plus metadata: definition (grammar, seed, parameter values), bone kinds, generations, rig hints |
| Skeleton | Standard |
| Physics asset | Standard |
| Static mesh | Standard |
| Growth animation | Standard animation sequence; optional |
| Control Rig | Shared rig shipped in the plugin's `Content/`; standalone copies written on request |

- Baked assets are written to a folder beside the grammar asset.
- **Regenerate** rebuilds a baked asset in place from its stored grammar and parameter
  snapshot; references to it remain valid.
- **Detach** removes a baked asset's link to its grammar.
- Each variant (seed, axiom) gets its own skeleton.
- The pose component drives any variant from its per-bone metadata, independent of bone names.

## Source files

- Import: `.lsys` (Cambriana grammar) and `.ls` (Lparser) files, by drag
  and drop and by the import dialog.
  `.ls` files are converted on import.
- A file's format is set by its extension alone: `.lsys` or `.ls`.
- The grammar asset stores the source file path and uses Unreal's reimport mechanism when
  the file changes. Reimporting a `.ls` file replaces the grammar text with a new conversion.
- A `.ls` file the converter rejects shows the error with its line and column, and no asset
  is created.
- `[Proposed — unconfirmed]` Reimporting a `.lsys` file replaces the grammar text with the
  file's text. A reimport keeps the asset's seed unless the file is `.lsys` and sets `seed`,
  which then becomes the asset's seed. A file the converter rejects on reimport leaves the
  asset unchanged and shows each error with its line and column. Live actors referencing
  the asset rebuild after a reimport. The import factory is also the reimport handler.
- `[Proposed — unconfirmed]` The import factory is separate from the new-asset factory. A
  `.ls` file is rejected when its conversion reports any error; each diagnostic is logged
  with the file path, line and column. Converted text is serialised in glyph style. The
  source file path is held in the asset's `UAssetImportData`.
- **Decided:** an imported grammar asset's right-click menu shows Reimport through the
  Content Browser's standard support for assets whose `UAssetImportData` holds source files;
  it reimports from the source file.
- **Decided:** nothing but an explicit export writes to a source file on disk; saving the
  asset, or any other operation, leaves the source file unchanged.
- **Decided:** export writes the grammar asset's text to a `.lsys` file through Unreal's
  standard asset export (Content Browser Asset Actions > Export). An asset holding legacy
  `.ls` text is exported as `.lsys`, converted by the core. Export is the only operation
  that writes a grammar text file; it writes only to the path the user chooses, never to the
  asset's source file unless the user picks that path.
- **Decided:** an exported `.lsys` file is UTF-8 without a byte order mark.
- `[Proposed — unconfirmed]` A legacy text the converter rejects exports nothing and logs
  each diagnostic with the asset path, line and column.

## Grammar asset editor

Standalone asset editor, opened from the grammar asset.

| Panel | Contents |
|---|---|
| Rules | Text editor; syntax colouring; warnings and errors at line and column |
| Viewport | Transient live result; skeleton overlay; pose and growth scrubbing; blob primitive placement and editing |
| Details | Settings and parameters grouped by tier; seed field and new-seed button; size mode and target |
| Outputs | Baked assets; Bake, Regenerate, Detach |

- The editor layout reserves space for a variant grid (AR-01).
- **Decided:** the grammar asset editor is a non-destructive playground: it edits a working
  copy of the grammar. The Rules, Details and Viewport panels all use the working copy.
- **Decided:** Save copies the working copy into the asset and saves it. Placed actors
  referencing the asset rebuild then, and only then.
- **Decided:** edits are undoable inside the editor; the asset and the scene show nothing
  until the save.
- **Decided:** closing the editor with unsaved edits asks whether to save them or discard
  them.
- **Decided:** no Apply button; Save only.
- **Decided:** the Rules typing pause rebuilds the preview; the edit reaches the working copy,
  not the asset.
- `[Proposed — unconfirmed]` The working copy is a transient duplicate of the asset
  (`StaticDuplicateObject` into the transient package, without `RF_Standalone` or
  `RF_Public`, with `RF_Transactional`), as the Material editor holds its preview material.
  Unlike the Material editor, the editor holds it itself rather than registering it as an
  editing object, so the asset editor subsystem records only the asset.
- `[Proposed — unconfirmed]` The copy between asset and working copy covers every property
  the grammar class declares except its import data, which stays with the asset. Save copies
  the working copy's values into the asset outside the undo history (the Material editor's
  copy-back also runs without a transaction) and without a change event, then saves the
  package (`FEditorFileUtils::PromptForCheckoutAndSave`). When the save completes, one change
  event follows if the values changed, so live actors rebuild once, and not at all for a save
  without edits. When it does not complete (checkout or save cancelled or failed), the asset
  takes back its previous values and its package its previous dirty state; actors see no
  change and the editor keeps its edits.
- `[Proposed — unconfirmed]` Save As asks for the new asset in the Content Browser's save
  asset dialog and writes the working copy, not the asset, to a new asset there, then saves
  it, closes this editor and opens one on the new asset. The original asset, its package's
  dirty state and its actors are untouched whether Save As completes or is cancelled; the
  Material editor instead applies its edits to the original first. Choosing the asset's own
  path is a Save. Save As never replaces an existing asset: the dialog disallows existing
  assets, and a path whose package is loaded or on disk shows a message, logs a warning and
  leaves that asset, its file and the editor's edits untouched. When the new asset's save does not complete, the editor stays open with its
  edits and the new asset stays in memory, unsaved, as with the engine's Save As. Tests answer
  the dialog through the toolkit's `SetSaveAsPrompt`.
- `[Proposed — unconfirmed]` The editor has unsaved edits while the working copy differs from
  the asset or a Rules edit is still waiting for the typing pause; undoing back to the saved
  state clears them. The editor tab's label shows `*` while it has unsaved edits or the
  asset's package is dirty. Edits do not mark the asset's package dirty. (The Material editor
  shows unapplied edits only through its Apply button, which this editor does not have.)
- `[Proposed — unconfirmed]` A save of the asset from outside the editor (Content Browser,
  Save All, any package save) saves the asset as it is and leaves the working copy and its
  unsaved edits alone, as with the Material editor. Save All skips the asset unless its
  package is dirty for another reason.
- `[Proposed — unconfirmed]` When the asset changes from outside the editor (reimport, an
  edit elsewhere, undo of an asset edit) and the editor has no unsaved edits, the working copy
  takes the asset's values and the preview follows; with unsaved edits the working copy keeps
  them.
- `[Proposed — unconfirmed]` The close prompt is a Yes/No/Cancel message dialog naming the
  asset's path, as the Material editor's: Yes saves as Save does, then closes, or, when the
  save does not complete, keeps the editor open with its edits as Cancel does; No discards
  the edits and closes; Cancel keeps the editor open.
  Closing because the asset was force-deleted does not ask. In unattended runs the dialog
  answers Cancel, so the editor stays open; tests answer through the toolkit's
  `SetUnsavedEditsPrompt`.
- `[Proposed — unconfirmed]` A Rules edit reaches the working copy as one undoable change,
  rebuilding the preview once, when typing pauses for 0.5 s, when the text loses focus,
  before a save and before the close prompt.
- `[Proposed — unconfirmed]` Rules text reaches the working copy with `\n` line breaks on
  every platform.
- **Decided:** the Outputs panel is an empty tab until bake exists.
- `[Proposed — unconfirmed]` The Viewport shows an advanced preview scene holding one dynamic
  mesh, built through `Cambriana::BuildMesh` from the build input of a live actor referencing
  the working copy with every override off and the working copy's seed, in the live actor's
  preview palette. It rebuilds when the working copy changes (Rules or Details edit, undo,
  redo, a reimport the working copy follows) and its build input differs from the last
  build's. The scene, mesh and materials are transient; nothing of the preview is saved in
  the asset.
- `[Proposed — unconfirmed]` The Rules tab shows, under the text, the preview build's
  diagnostics as a message log listing (`IMessageLogListing`): one row per diagnostic, in report order,
  with the severity's icon and `line L, column C: <problem>`, or `production position P:
  <problem>` for a diagnostic without a line. The listing is replaced after every preview
  build, so diagnostics go away when the text is fixed. It comes from the preview's build
  report; nothing else parses or builds for it. Its rows are not mirrored to the Output Log.
- `[Proposed — unconfirmed]` The asset stores a value per parameter, by case-sensitive name.
  A parameter without a stored value is unchanged and uses the text's value; one with a stored
  value uses it, whatever the text's value. Text edits and reimports leave stored values
  alone: a parameter the text adds is unchanged; a changed text value moves only unchanged
  parameters; a stored value whose name the text no longer declares (removed or renamed) is
  kept, unused and not shown, and applies again if the name is declared again. Legacy `.ls`
  text has no parameters, and stored values do not affect its build.
- `[Proposed — unconfirmed]` Details is a details customization of the grammar asset: the
  seed row (number field and new-seed button) and, for `.lsys` text, one category per tier
  (Pose Parameters, Shape Parameters, Structure Parameters, and Parameters for untiered
  ones), each holding its parameters in file order as a number field. A field shows the value
  the build uses: the stored value, else the text's value computed with earlier parameters'
  stored values. Committing a field stores the value; the row's reset-to-default arrow, shown
  only for a stored value, removes it. Each is one undoable change to the working copy that
  rebuilds the preview. The categories are rebuilt when the text's parameter names or tiers
  change.
- `[Proposed — unconfirmed]` Parameter values reach the core through the generation seam:
  `FCambrianaBuildInput` carries the asset's stored values, and `Cambriana::BuildMesh` builds
  `.lsys` text from its parsed model with them applied (`cambriana_grammar_set_parameter`,
  `cambriana_build_grammar_model`). The parameter list is read by `Cambriana::ReadParameters`
  in the same file.
- `[Proposed — unconfirmed]` Each preview rebuild asks the Viewport to redraw, so the new
  mesh shows without user input.
- `[Proposed — unconfirmed]` After each rebuild the preview scene's floor moves to the
  height of the mesh's lowest vertex, as the Static Mesh editor sets its floor offset from
  the mesh bounds; the mesh stays at the origin. An empty preview puts the floor at the
  origin. Placed live actors are unaffected.
- `[Proposed — unconfirmed]` The Viewport camera starts at pitch −20°, yaw −45° and frames
  the mesh's bounds when the editor opens (`FEditorViewportClient::FocusViewportOnBox`).
  Frame Selected (F) frames them again, animated as in the Static Mesh editor. The camera
  does not move on its own after edits.

## Live actor

- Dynamic-mesh actor. Rebuilds through the core on property change.
- **Decided:** a grammar asset holds size (mode and value) and surfacer settings (closed form,
  sides, min thickness). A live actor that references the asset builds with the asset's
  settings, and the actor's own override toggles still win over them, as they win over a
  grammar file's settings.
- `[Proposed — unconfirmed]` A live actor that references a grammar asset builds with the
  asset's stored parameter values. Inline text builds with the text's values.
- Properties: recursion, basic angle, thickness, min thickness, seed, closed form, sides,
  size override, size mode and value; a grammar asset reference or inline grammar text.
  Inline `.ls` text is converted.
- **Decided:** min thickness, closed form and sides each have an override toggle, off by
  default, like size. Each size and surfacer setting is taken from the first of: the actor,
  when its override is on; the grammar text, when it states the setting; the grammar asset;
  the default. Without a grammar asset, the actor's own value takes the asset's place.
- The interim grammar asset holds only grammar text, a seed, parameter values, size and
  surfacer settings and, when imported, its source file path. Its settings default to the live actor's. A new one holds the default grammar as `.lsys` text; one holding legacy `.ls`
  text is read directly by the core. M2 adds import, conversion and the grammar editor, and
  moves stored text to the Cambriana grammar format.
- The format of inline text and grammar asset text is detected from the text, with no
  setting. `[Proposed — unconfirmed]` The text is `.ls` if its first line that is neither
  blank nor a `#` comment (leading whitespace ignored) starts with a number: an optional `+`
  or `-`, an optional `.`, then a digit. Otherwise it is `.lsys`, including empty text.
- `.ls` text builds through `cambriana_build_legacy`; `.lsys` text through
  `cambriana_build_grammar` with the unnamed or first axiom. `[Proposed — unconfirmed]` For
  `.lsys` text the actor's seed applies over the grammar's `seed`. **Decided:** for `.lsys`
  text the grammar's `size` or `step` applies unless the actor's size override is on, as with
  the recursion, basic angle and thickness overrides; with the override on, the actor's size
  mode and value apply. A grammar with neither builds at the core's default size. The
  override is off by default. `[Proposed — unconfirmed]` `.ls` text always uses the actor's
  size mode and value, whatever the override. In either format, diagnostics are logged with their line and column, and
  errors leave an empty mesh.
- **Decided:** the Cambriana Grammar asset is under a "Boju Studios" category in the Content
  Browser's Add menu, and the live actor is under a "Boju Studios" category when placing
  actors.
- Dragging a grammar asset into a level spawns a live actor referencing it. A change to the
  grammar asset rebuilds every live actor referencing it.
- No skeleton.

## Seed

- A new grammar asset or live actor gets a newly generated seed, stored on the object.
- A live actor placed from a grammar asset copies the asset's seed at placement; afterwards
  the actor's seed is independent of the asset's. A live actor always builds with its own
  seed.
- Seed UI: an editable number field and a button that generates a new seed.
- New seeds come from a non-deterministic source (time or the OS random source).
- A grammar file's `seed` setting, if present, is used on import. Converted `.ls` files get
  a generated seed.

## Run time

- Pose component on a skeletal mesh actor: holds per-instance pose and growth values and
  applies the bone rotations returned by core pose evaluation ([SDD-rigging.md](SDD-rigging.md)). Values are properties,
  keyframeable in Sequencer and settable from Blueprint and C++.
- Rig controls: the shared Control Rig, run from a Control Rig component or an Animation
  Blueprint, builds controls from the mesh's rig hints ([SDD-rigging.md](SDD-rigging.md)).

## Run-time generation

Requirement: FR-18 ([SRS.md](SRS.md)).

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
| CambrianaCore | External | Core headers, import libraries and dynamic libraries |
| Cambriana | Runtime | Asset types, live actor, generation path, pose component, runtime function library |
| CambrianaEditor | Editor | Asset editor, importers, factories, bake, standalone Control Rig writer, editor subsystem, integrations |

Engine dependencies: stable modules only; no experimental engine plugins.

## Plugin descriptor

- **Decided:** the plugin's producer (`CreatedBy`) is Boju Studios. The description keeps
  the Lparser credit (FR-23).
