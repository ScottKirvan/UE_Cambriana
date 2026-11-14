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
- Bake writes from the grammar asset's saved state. With unsaved edits in the
  grammar editor, Bake asks to save first.
- Bake writes a skeletal mesh, skeleton and physics asset. Static mesh output is
  a bake setting on the grammar asset.
- Bake is offered on the grammar asset's right-click menu and in the grammar
  editor. The live actor's Details Bake writes a static mesh only, from the actor's own build
  input.
- The physics asset merges capsules along chains.
- **Reimport** is the standard right-click Reimport on a baked skeletal or static
  mesh, with the grammar asset recorded as its source. It rebuilds the asset in place from the
  grammar's current saved state; references to it remain valid, and the mesh's stored
  definition is updated. A change to the bone tree replaces the skeleton in place.
- **Detach** is a right-click item on a baked mesh that removes its link to its
  grammar. A detached mesh is not offered Reimport and is not listed in the grammar's
  outputs.
- Baked asset names are the grammar asset's name with the Unreal style prefixes
  `SK_`, `SKEL_`, `PHYS_` and `SM_`.
- `[Proposed — unconfirmed]` The bake settings are one struct on the grammar asset
  (`FCambrianaBakeSettings`, Details category Bake) holding the static mesh output, off by
  default, and the physics asset's overall stiffness `PhysicsStiffness` (SDD-rigging.md,
  Physics). The skeletal mesh, skeleton and physics asset need no setting: Bake always writes
  the skeletal mesh and skeleton, and the static mesh as well when static mesh output is on.
- `[Proposed — unconfirmed]` The folder beside the grammar asset is named after it: a grammar
  `/Game/Trees/Fern` bakes `/Game/Trees/Fern/SM_Fern`.
- `[Proposed — unconfirmed]` Bake is the `Bake` entry in the asset actions section of the
  grammar asset's right-click menu; it bakes each selected grammar and shows a notification
  for each; reasons are logged to `LogCambrianaBake`.
- `[Proposed — unconfirmed]` A grammar asset without a saved state (its package has unsaved
  changes or was never saved) is first offered to the engine's save prompt
  (`FEditorFileUtils::PromptForCheckoutAndSave`); when it is not saved, nothing is baked.
  Unsaved grammar editor edits are in the working copy, not the asset, and are not baked. The
  baked assets' packages are saved without a prompt.
- `[Proposed — unconfirmed]` The static mesh is built in the runtime module
  (`Cambriana::SetStaticMeshFromBuild`) from the result of `Cambriana::BuildMesh` with the
  build input of a live actor referencing the grammar with every override off and the
  grammar's seed. It has one LOD and one material slot per material ID, in ascending ID
  order, named `Material_<ID>`. The build's normals are kept; the engine computes tangents
  and generates lightmap UVs into UV3, after UV0 to UV2 (SDD-surfacing.md, Data channels). In
  the editor the mesh is stored as the static mesh's source model; outside the editor it is
  built with `UStaticMesh::BuildFromMeshDescriptions`.
- `[Proposed — unconfirmed]` The static mesh's build keeps sliver triangles: degenerate removal
  is off (`bRemoveDegenerates`), as Geometry Script's `CopyMeshToStaticMesh` leaves it by
  default (`FGeometryScriptCopyMeshToAssetOptions::bEnableRemoveDegenerates`). The engine's
  static mesh build still drops a triangle with two corners at one single-precision point
  (`StaticMeshBuilder.cpp`, `BuildVertexBuffer`); it has no area. ATS-46 and ATS-47 compare the
  rendered LOD 0 with the live actor's mesh: every live triangle without such corners pairs with
  one rendered triangle, same winding, each corner within 0.001 cm, and every live vertex has a
  rendered vertex within 0.001 cm.
- `[Proposed — unconfirmed]` ATS-42 compares the baked skeletal mesh placed in a level, its LOD 0
  skinned on the CPU at the reference pose (`USkinnedMeshComponent::ComputeSkinnedPositions`),
  with a live actor placed from the same grammar, in the same way and to the same tolerance.
- `[Proposed — unconfirmed]` A new slot holds the engine's default surface material
  (`UMaterial::GetDefaultMaterial(MD_Surface)`), as the FBX static mesh importer fills slots
  it has no material for. A slot whose name the existing static mesh already has keeps that
  slot's material, as an FBX reimport keeps materials by slot name.
- `[Proposed — unconfirmed]` When any output it writes already exists as an asset of its class,
  Bake asks once "Do you want to overwrite the existing asset?" (Yes/No), naming the first
  existing one in the order `SK_`, `SKEL_`, `PHYS_`, `SM_`, as the engine's import asks once per imported
  file (`AssetTools.cpp`, `ImportObjectAlreadyExists_SameClass`). Yes rebuilds every existing
  output as the same object in place, so references to it stay valid; No bakes nothing. An asset
  of another class at any output's name is left alone, an error is logged and nothing is baked.
- `[Proposed — unconfirmed]` The skeletal mesh is built in the runtime module
  (`Cambriana::SetSkeletalMeshFromBuild`) from the same build result as the static mesh, in the
  sequence of Geometry Script's `CopyMeshToSkeletalMesh` with a new reference skeleton
  (`MeshAssetFunctions.cpp`): the mesh description of LOD 0 converted from the build's mesh
  with its bones and skin weights (`FDynamicMeshToMeshDescription`), the reference skeleton
  set and committed, then `PostEditChange`. It has one LOD and the static mesh's slots and slot
  names, keeping materials by slot name; the build's normals are kept, tangents computed,
  degenerate removal off and UVs at full precision, as for the static mesh. Vertex colours are
  converted without the static mesh's sRGB-to-linear transform: the skeletal mesh build stores
  them linear (`SkeletalMeshLODImporterData.cpp`; `StaticToSkeletalMeshConverter.cpp` undoes the
  transform when it turns a static mesh into a skeletal one). Its mesh description is
  editor-only, so it is built in the editor only; the run-time construction path is M4
  (Run-time generation, below).
- `[Proposed — unconfirmed]` The skeleton is set on the skeletal mesh and its bone tree is
  recreated from the mesh (`USkeleton::RecreateBoneTree`), as the procedural vegetation export
  does for the skeleton it writes (`PVExportHelper.cpp`). A rebake rebuilds an existing skeleton
  in place the same way, so its tree follows a changed bone tree.
- `[Proposed — unconfirmed]` The physics asset's bodies and capsules are computed in the runtime
  module from the build result (`Cambriana::MakePhysicsBodies`, SDD-rigging.md, Physics). The
  editor module writes them into `PHYS_<name>` with the engine's physics asset functions
  (`FPhysicsAssetUtils::CreateNewBody`, `CreateNewConstraint`), sets the skeletal mesh as its
  preview mesh and it as the skeletal mesh's physics asset, as
  `FPhysicsAssetUtils::CreateFromSkeletalMesh` does. A rebake empties its bodies and
  constraints first, as the physics asset editor does before regenerating all bodies
  (`PhysicsAssetEditor.cpp`), and rebuilds the same object.
- `[Proposed — unconfirmed]` A grammar with errors, or one that builds no triangles (empty
  text included), or whose bone tree makes no reference skeleton (a parent after its child, or
  two bones of one name), bakes nothing and leaves an existing baked mesh unchanged; after the
  build's diagnostics an error naming the grammar is logged, as a failed import creates no
  asset.
- `[Proposed — unconfirmed]` The live actor's Details Bake is a `Bake` button on its own row at
  the end of the Cambriana category, as the procedural mesh component's Details places its
  Create StaticMesh button (`ProceduralMeshComponentDetails.cpp`). It bakes each selected
  placed live actor, shows a notification for each, and is disabled for class defaults and
  archetypes.
- `[Proposed — unconfirmed]` The live actor's Details Bake asks where to write the static mesh
  in the engine's save-asset dialog (`IContentBrowserSingleton::CreateModalSaveAssetDialog`,
  static meshes only), as Merge Actors asks for its merged mesh
  (`MergeActors/Private/MergeProxyUtils/Utils.cpp`, `GetPackageNameForMergeAction`). Cancel
  bakes nothing. The dialog allows an existing static mesh after its own question ("<name>
  already exists. Do you want to replace it?", `ESaveAssetDialogExistingAssetPolicy::AllowButWarn`,
  `ContentBrowser/Private/SAssetDialog.cpp`); Bake then rebuilds that object in place without
  asking again. Tests answer the dialog through `Cambriana::Editor::SetBakeLocationPrompt`.
- `[Proposed — unconfirmed]` The dialog defaults, for a live actor with a grammar asset, to
  `SM_<grammar asset name>_<actor label>` in that grammar's bake folder: a grammar
  `/Game/Trees/Fern` and an actor labelled `Fern2` default to `/Game/Trees/Fern/SM_Fern_Fern2`.
  For one with inline text and no grammar asset it defaults to `SM_<actor label>` at the
  project's content root (`/Game`). Merge Actors likewise defaults to `SM_` plus a name made from
  the actor at the content root (`MeshMergingTool.cpp`, `GetDefaultPackageName`;
  `MeshMergeUtilities.cpp`). The label is sanitised as an object name
  (`ObjectTools::SanitizeObjectName`).
- `[Proposed — unconfirmed]` The live actor's Details Bake shares the grammar Bake's refusal of
  an asset of another class, nothing baked for errors or no triangles, and saving without a
  prompt. A grammar asset without a saved state is first offered to the engine's save prompt;
  when it is not saved, nothing is baked and the dialog does not show. The grammar's own bake
  settings do not apply. The grammar right-click Bake keeps its fixed location and its own
  overwrite question.
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
- An imported grammar asset's right-click menu shows Reimport through the
  Content Browser's standard support for assets whose `UAssetImportData` holds source files;
  it reimports from the source file.
- Nothing but an explicit export writes to a source file on disk; saving the
  asset, or any other operation, leaves the source file unchanged.
- Export writes the grammar asset's text to a `.lsys` file through Unreal's
  standard asset export (Content Browser Asset Actions > Export). An asset holding legacy
  `.ls` text is exported as `.lsys`, converted by the core. Export is the only operation
  that writes a grammar text file; it writes only to the path the user chooses, never to the
  asset's source file unless the user picks that path.
- An exported `.lsys` file is UTF-8 without a byte order mark.
- `[Proposed — unconfirmed]` A legacy text the converter rejects exports nothing and logs
  each diagnostic with the asset path, line and column.

## Grammar asset editor

Standalone asset editor, opened from the grammar asset.

| Panel | Contents |
|---|---|
| Rules | Text editor; syntax colouring; warnings and errors at line and column |
| Viewport | Transient live result; skeleton overlay; pose and growth scrubbing; blob primitive placement and editing |
| Details | Settings and parameters grouped by tier; seed field and new-seed button; size mode and target |
| Outputs | Baked assets; Bake |

- The editor layout reserves space for a variant grid (AR-01).
- The grammar asset editor is a non-destructive playground: it edits a working
  copy of the grammar. The Rules, Details and Viewport panels all use the working copy.
- Save copies the working copy into the asset and saves it. Placed actors
  referencing the asset rebuild then, and only then.
- Edits are undoable inside the editor; the asset and the scene show nothing
  until the save.
- Closing the editor with unsaved edits asks whether to save them or discard
  them.
- No Apply button; Save only.
- The Rules typing pause rebuilds the preview; the edit reaches the working copy,
  not the asset.
- `[Proposed — unconfirmed]` The working copy is a transient duplicate of the asset
  (`StaticDuplicateObject` into the transient package, without `RF_Standalone` or
  `RF_Public`, with `RF_Transactional`), as the Material editor holds its preview material.
  Unlike the Material editor, the editor holds it itself rather than registering it as an
  editing object, so the asset editor subsystem records only the asset.
- `[Proposed — unconfirmed]` The copy between asset and working copy covers every property
  the grammar class declares except its import data, which stays with the asset; the editor's
  Details does not show the working copy's import data (the asset's own Details shows the
  asset's). Save copies
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
- `[Proposed — unconfirmed]` The close prompt is a message dialog naming the asset's path, as
  the Material editor's. Closing the editor's tab (its close button, or the editor shutting
  down, which asks each tab first) can be stopped, so it asks Yes/No/Cancel: Yes saves as
  Save does, then closes, or, when the save does not complete, keeps the editor open with its
  edits as Cancel does; No discards the edits and closes; Cancel keeps the editor open. Every
  other close comes through the asset editor subsystem (unloading, reloading or deleting the
  asset's package, consolidating assets, closing every editor at shutdown) and cannot be
  stopped: `FAssetEditorToolkit::CloseWindow` reports the close as done whatever the editor
  answers, and the caller carries on. It asks Yes/No, as the engine's save prompt does when it
  cannot be declined (`FEditorFileUtils::SaveDirtyPackages`): Yes saves and closes, and asks
  again when the save does not complete; No discards the edits and closes. Edits discarded at a
  prompt are not asked about again by a later close until the working copy changes again.
  Closing because the asset was force-deleted does not ask. In unattended runs the dialog
  answers Cancel when it offers Cancel, otherwise No; tests answer through the toolkit's
  `SetUnsavedEditsPrompt`, which is given the buttons the dialog offers.
- `[Proposed — unconfirmed]` A Rules edit reaches the working copy as one undoable change,
  rebuilding the preview once, when typing pauses for 0.5 s, when the text loses focus,
  before a save, before the close prompt and before the editor's own undo or redo (the
  toolkit maps Undo and Redo to commands that apply it first, as the Material and Blueprint
  editors map them to their own), so an undo during the pause removes exactly the typed edit.
  An undo or redo from outside the editor during the pause leaves the typed text in Rules; it
  reaches the working copy when the pause ends.
- `[Proposed — unconfirmed]` Rules text reaches the working copy with `\n` line breaks on
  every platform.
- The Outputs panel is an empty tab until bake exists.
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
- A grammar asset holds size (mode and value) and surfacer settings (closed form,
  sides, min thickness). A live actor that references the asset builds with the asset's
  settings, and the actor's own override toggles still win over them, as they win over a
  grammar file's settings.
- `[Proposed — unconfirmed]` A live actor that references a grammar asset builds with the
  asset's stored parameter values. Inline text builds with the text's values.
- Properties: recursion, basic angle, thickness, min thickness, seed, closed form, sides,
  size override, size mode and value; a grammar asset reference or inline grammar text.
  Inline `.ls` text is not converted; it builds as legacy text (below).
- Min thickness, closed form and sides each have an override toggle, off by
  default, like size. Each size and surfacer setting is taken from the first of: the actor,
  when its override is on; the grammar text, when it states the setting; the grammar asset;
  the default. Without a grammar asset, the actor's own min thickness, closed form and sides
  take the asset's place; size has no asset step and falls back to the core's default size.
- Inline `.lsys` text (no grammar asset) with neither `size` nor `step` and the
  size override off builds at the core's default size.
- Turning an actor's override off changes none of the actor's values; the build
  then takes the setting from the grammar text, the grammar asset or the default, and turning
  the override back on builds with the actor's kept value.
- A live actor placed from a grammar asset starts each setting that has an
  override (recursion, basic angle, thickness, size, min thickness, closed form, sides) from
  the first of: the grammar text, when it states the setting; the grammar asset; the default.
  Afterwards the actor's values are independent of the asset's.
- The Details reset-to-default arrow of each setting that has an override
  (recursion, basic angle, thickness, size mode and value, min thickness, closed form, sides)
  resets the actor's value to the value the setting would start from at placement, evaluated
  at the time of the reset: the grammar text, when it states the setting; else the grammar
  asset; else the default. Without a grammar asset (inline text) the same rule applies with no
  asset step, so size without a `size` or `step` statement resets to the core's default size.
  The arrow shows only when the actor's value differs from that value. A reset is undoable
  like any property edit and leaves the override checkbox as it is.
- `[Proposed — unconfirmed]` In Details each setting that has an override is editable while
  the build uses the actor's value of it, and greyed otherwise: with its override on, or, with
  it off, when nothing ahead of the actor gives the setting (without a grammar asset: min
  thickness, closed form and sides the text does not state, and the size of `.ls` text). Each
  override's checkbox stays inline, left of its value, and editable; size mode's checkbox
  also controls size value. Recursion, basic angle and thickness keep their override as an
  edit condition; the size and surfacer overrides are drawn by the live actor's Details
  customization.
- `[Proposed — unconfirmed]` The size mode and size value reset separately, each from its own
  row. With several actors selected, the arrow shows when any of them differs from its own
  value, and a reset sets each to its own value in one transaction. The default is the live
  actor's class default, which equals the core's defaults (recursion 4, basic angle 25,
  thickness 50, Fit 200 cm, sides 8, closed form, min thickness 0).
- A grammar asset's size and surfacer settings start from its grammar text, for
  each setting the text states; otherwise from the default. `[Proposed — unconfirmed]` This
  applies when the asset is created, by the new-asset factory or by import; a reimport or a
  text edit leaves the asset's settings unchanged.
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
  `.lsys` text the actor's seed applies over the grammar's `seed`. For `.lsys`
  text the grammar's `size` or `step` applies unless the actor's size override is on, as with
  the recursion, basic angle and thickness overrides; with the override on, the actor's size
  mode and value apply. A grammar with neither builds at the core's default size. The
  override is off by default. `[Proposed — unconfirmed]` `.ls` text always uses the actor's
  size mode and value, whatever the override. In either format, diagnostics are logged with their line and column, and
  errors leave an empty mesh.
- The Cambriana Grammar asset is under a "Boju Studios" category in the Content
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

- An editor subsystem exposes create, import, bake, reimport and detach to Blueprint and
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

- The plugin's producer (`CreatedBy`) is Boju Studios. The description keeps
  the Lparser credit (FR-23).
