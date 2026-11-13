# CLAUDE.md — UE_Cambriana

## Project

Cambriana is an Unreal Engine 5.8 plugin that generates rigged skeletal meshes from
L-system grammars. It reads the legacy Lparser `.ls` dialect and a new grammar format. All
grammar, turtle and geometry logic is in CambrianaCore, a private sibling repo
(`../CambrianaCore`), delivered here as headers and prebuilt dynamic libraries.
`UE_Cambriana.code-workspace` (local, gitignored) opens both repos.

Specs: [notes/dev/README.md](notes/dev/README.md) (requirements in `SRS.md`, design in
`SDD-*.md`, formats in `FMT-*.md`, acceptance tests in `ATS.md`, open issues in `TBD.md`).
Core specs are in CambrianaCore `notes/dev/`.

Spec conventions:

- Mark status only as `[Proposed — unconfirmed]` or **Open**; text without a marking is decided.
- State decisions without rationale; no history; programmer-relevant content only.
- Refer to the project owner as "the author"; do not use their name.
- Do not define release boundaries; scope is the author's call.
- Deferred requirements are listed as anticipated requirements with design obligations.

## Build and Test

Unreal 5.8 is installed at `D:\bin\Epic Games\UE_5.8`. The plugin is built inside the
content-only base project `Cambriana_Base` (see `notes/dev/SDD-architecture.md`, Base
project), created once with:

```
powershell -File scripts/New-BaseProject.ps1     # optional -ProjectDir <path>
```

It writes `D:\3\ue\5.8\dev\Cambriana_Base\Cambriana_Base.uproject` and junctions this repo in as
`Plugins\Cambriana`. Build the editor modules (ATS-01):

```
"D:\bin\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" UnrealEditor Win64 Development -Project=D:\3\ue\5.8\dev\Cambriana_Base\Cambriana_Base.uproject -WaitMutex -NoHotReload
```

Binaries go to this repo's `Binaries/Win64/` (gitignored), with `CambrianaCore.dll` staged
beside them.

Run the plugin's automation tests (all named `Cambriana.*`) headless:

```
"D:\bin\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" D:\3\ue\5.8\dev\Cambriana_Base\Cambriana_Base.uproject -ExecCmds="Automation RunTests Cambriana" -TestExit="Automation Test Queue Empty" -unattended -nopause -nullrhi -nosplash -nosound -stdout -FullStdOutLogOutput
```

UE 5.8 needs the Visual C++ runtime 14.50.35719 or later
(`Engine\Extras\Redist\en-us\vc_redist.x64.exe`); with an older runtime the Fab and Bridge
engine plugins fail to load and an unattended editor exits before running tests.

Check that no dependency is an experimental engine plugin (ATS-05; exports the editor target
through `Build.bat -Mode=JsonExport`, so the editor must be closed), and run that check's
fixture tests:

```
powershell -NoProfile -File scripts/Test-NoExperimentalDependencies.ps1
powershell -NoProfile -File scripts/tests/Test-NoExperimentalDependencies.Tests.ps1
```

Layout `[Proposed — unconfirmed]`:

| Path | Contents |
|---|---|
| `Cambriana.uplugin` | Plugin descriptor; modules allowed on Win64 only until other platforms' core libraries are delivered |
| `Source/Cambriana/` | Runtime module |
| `Source/Cambriana/Public/CambrianaBuild.h` | Generation seam `Cambriana::BuildMesh` (grammar text + settings + seed + stored parameter values to mesh, material slot IDs and a build report with diagnostics and budget flags), `Cambriana::ReadParameters` (the `.lsys` text's parameters: name, tier, text value, value used), the settings grammar text states (`Cambriana::ReadStatedSettings`: `.lsys` settings statements, or the `.ls` header of text without errors) and the preview palette; implemented in `Private/CambrianaBuild.cpp`, the only file that calls the core's build API, over `Private/CambrianaCoreGrammar.h` (`Cambriana::FCoreGrammar`, the owned core grammar model also used by `CambrianaGrammarConversion.cpp`). The mesh carries the data channels in UV1, UV2 and the vertex colour, the first three user attributes in its G, B and A. Empty grammar text builds nothing without calling the core; a grammar with errors builds an empty mesh `[Proposed — unconfirmed]` |
| `Source/Cambriana/Private/CambrianaBuildInternal.h` | Seam internals exposed to tests: core-availability gate (logs once), result-flag mapping, report logging to `LogCambriana` `[Proposed — unconfirmed]` |
| `Source/Cambriana/Public/CambrianaGrammar.h` | Interim grammar asset (grammar text, `.lsys` or legacy `.ls`, seed, stored parameter values by case-sensitive name, size and surfacer settings, bake settings `FCambrianaBakeSettings` with static mesh output off by default); its factories start the settings from the text; broadcasts `OnGrammarChanged` after edits `[Proposed — unconfirmed]` |
| `Source/Cambriana/Public/CambrianaLiveActor.h` | Live actor; rebuilds in `OnConstruction` only when its build input differs from the last build (so moves do not rebuild), and whenever its grammar asset changes; placement from an asset starts its overridable settings from `GetStartingSettings` (the text, the asset or the default), and each setting's Details reset goes to that starting value (`IsAtStartingValue`, `ResetToStartingValue`; the text's stated settings are cached, keyed by the exact text); `MakeBuildInputFor` gives a grammar asset's own build input (every override off, the asset's seed); `UsesOwnValue` tells whether the build uses the actor's own value of a setting (its override on, or nothing ahead of the actor gives it) `[Proposed — unconfirmed]` |
| `Source/Cambriana/Public/CambrianaPreviewMaterials.h` | Preview palette materials on a dynamic mesh component, shared by the live actor and the grammar editor preview `[Proposed — unconfirmed]` |
| `Source/Cambriana/Public/CambrianaStaticMesh.h` | Static mesh construction shared by bake and, later, run-time generation: `Cambriana::SetStaticMeshFromBuild` makes a static mesh one LOD of a build result with one material slot per material ID, named by `Cambriana::MaterialSlotName` (`Material_<ID>`), keeping the material of a same-named slot and giving new slots the engine's default surface material, keeping sliver triangles (degenerate removal off), storing UVs at full precision and vertex colours so a material reads the data channel values (SDD-surfacing.md, Data channels) `[Proposed — unconfirmed]` |
| `Source/Cambriana/Public/CambrianaSeed.h` | Seed generation from the OS random source `[Proposed — unconfirmed]` |
| `Source/CambrianaEditor/` | Editor module |
| `Source/CambrianaEditor/Private/` | Grammar factory (new assets hold the default grammar as `.lsys` text) and asset definition, `CambrianaBake` (Bake on the grammar asset's right-click menu, `Cambriana::Editor::BakeGrammar`: saves an unsaved grammar through the engine's save prompt first, builds through `Cambriana::BuildMesh` and `SetStaticMeshFromBuild`, writes and saves `SM_<name>` in a folder named after the grammar beside it, asks before rebuilding an existing one in place, and writes nothing for a grammar with errors or no triangles; tests answer the overwrite question through its `AskToOverwrite` argument; `Cambriana::Editor::BakeLiveActor` bakes a live actor's own build input through the same path to where the engine's save-asset dialog says, defaulting to `SM_<grammar>_<actor label>` in the grammar's bake folder or `/Game/SM_<actor label>` for inline text, the dialog owning the overwrite warning; tests answer it through `SetBakeLocationPrompt`) `[Proposed — unconfirmed]`, live actor factory (drag and drop), `CambrianaConversionLog` (legacy conversion diagnostics logged for import, reimport and export), seed field with new-seed button, live actor Details customization `CambrianaLiveActorCustomization` (seed field; each overridable setting's reset arrow shown while it differs from its starting value and resetting it in one undoable edit; the size and surfacer overrides drawn inline, each value greyed while the build does not use it; a Bake button row at the end of the Cambriana category) `[Proposed — unconfirmed]`; grammar asset details customization `CambrianaGrammarCustomization` (seed row; `.lsys` parameters in one category per tier, set and reset as undoable changes) `[Proposed — unconfirmed]`; grammar editor toolkit `CambrianaGrammarEditorToolkit`, editing a transient working copy (`CambrianaGrammarWorkingCopy`: duplicate, compare and copy every grammar property but the import data) that Save copies into the asset and saves, restoring the asset when the save does not complete, and Save As writes to a new asset only, never over an existing one, with `*` on the tab while edits are unsaved and a prompt on close: save, discard or cancel when its tab closes; save or discard, asking again after a save that does not complete, on the asset editor subsystem's closes, which cannot be stopped (`SetUnsavedEditsPrompt`, given the buttons offered, and `SetSaveAsPrompt` answer the dialogs in tests) (Rules tab: the working copy's text above a message log listing of the preview build's errors and warnings, each with its line and column or production position; text applied to the working copy as one undoable edit once typing pauses for `RulesApplyDelaySeconds`, on focus loss, before a save, on close and before the editor's own Undo and Redo, with `\n` line breaks; Viewport tab showing `CambrianaGrammarPreview`, the working copy's transient preview built as a live actor referencing it with every override off and its seed would build it, with the scene floor at the mesh's lowest point after each rebuild, in `SCambrianaGrammarViewport`, which redraws on each rebuild and frames the mesh on open and on F; Details tab showing the working copy without the grammar text or the import data; empty Outputs tab) `[Proposed — unconfirmed]` |
| `Source/*/Private/Tests/` | Automation tests, named `Cambriana.*`; editor bake tests share `CambrianaBakeTestFixture.h` and compare a baked static mesh's rendered LOD 0 with a live actor's mesh through `CambrianaMeshComparison.h` |
| `Source/Cambriana/Private/Tests/Fixtures/` | Byte-exact legacy `.ls` fixtures (`sk.ls`, `TREE11.LS`), kept out of line-ending conversion by `.gitattributes` `[Proposed — unconfirmed]` |
| `Source/ThirdParty/CambrianaCore/` | External module: delivered core headers, libraries and stamps; written by CambrianaCore `scripts/deliver.cmake`, never edited by hand except `CambrianaCore.Build.cs` |
| `docs/grammar-guide.md`, `docs/grammar-spec.md` | `.lsys` guide for grammar authors and formal specification; update with the format |
| `scripts/New-BaseProject.ps1` | Creates the base project |
| `scripts/Test-NoExperimentalDependencies.ps1` | ATS-05 check; fixture tests in `scripts/tests/` |

CI is not set up yet; self-hosted runners for this repo are pending.

## Keeping This File Current

This file is the primary context for any agent working in this repo — keep it accurate
as the project evolves. When you learn what the project is, add a brief description at
the top. As key files, build commands, and architectural decisions emerge, record them
here so future sessions start with full context rather than re-deriving it.

Update this file in the same commit as the work it documents.

## Working Conventions

- Never commit or push directly to `main`. Always branch first, then PR.
- Before pushing to, or building new commits on, a previously-used branch, run `git
  fetch --prune` and confirm its remote ref still exists — a merged PR's branch may
  already be gone. At the start of any session resuming an existing branch, `git fetch
  origin && git rebase origin/main` before touching files — branches drift silently
  between sessions.
- Branch names must describe the work (e.g. `fix/login-timeout`, `feat/export-csv`).
  No random characters, UUIDs, or generated suffixes to ensure uniqueness — if a name
  is already taken, pick a more specific descriptive name instead.
- If a branch name is pre-assigned by tooling (a hosted agent session, a CI runner)
  rather than chosen by you, verify it against this convention before the first push.
  Rename locally (`git branch -m <name>`) if it doesn't match — being handed a name
  isn't an exemption from the rule.
- One concern per branch and PR. If work naturally splits into independent problems,
  split the branches too — resist bundling unrelated changes into one PR.
- Conventional commits: `feat:` / `fix:` / `docs:` / `chore:` / `refactor:` / `test:`.
  Breaking: `feat!:`.
- `feat:` is for genuinely new user-facing capabilities only. Bug fixes and corrections
  use `fix:`, even when they close a tracked issue.
- Unit tests must be written alongside all new code. All bug fixes require red/green
  tests — a failing test that reproduces the bug, then the fix that makes it pass.
- CI, lint, and formatting must all pass before committing or opening a PR. Discover
  the project's commands from the CI config, `package.json`, `Makefile`, or equivalent
  — do not assume they match another project's toolchain.
- Prefer narrow, localised changes. Favour modularity that contains the blast radius
  of future edits — a fix or feature should not require touching unrelated parts of
  the codebase. If it does, that's a design signal worth surfacing.
- Refactoring is a first-class activity, not something to defer. Improve structure as
  you go rather than accumulating technical debt for a later pass.
- When working in unfamiliar domain territory, prefer primary sources — official docs,
  specs, RFCs — over general knowledge. Flag domain uncertainty explicitly rather than
  proceeding on an assumption.
- Default to writing no comments. Add one only when the *why* is non-obvious — a
  hidden constraint, a subtle invariant, a workaround for a specific bug. If code is
  hard to understand, the fix is clearer naming and structure, not a comment explaining
  what it does.

## Change as Experiment

Work proceeds in small, verifiable, safe, directed steps — not a plan executed end to
end. Each step is small enough to evaluate on its own: land it, check whether it moved
things in the right direction, then decide the next step from what was just learned
rather than from what was originally guessed. Treat every change as an experiment with
a check at the end, not a commitment to a predetermined path.

This project runs in a constrained environment on purpose — the impulse to run ahead,
anticipate the next three steps, or solve adjacent problems while already in the code
is explicitly suppressed. Staying inside the current step is a discipline, not a
limitation to work around.

## Testing Strategy

Tests are the fastest mechanical check that generated code matches intent — treat them
as load-bearing, not optional scaffolding. Two tiers matter most here:

- **Unit tests**, written alongside the code (see Working Conventions above) — TDD
  where practical: the test exists before the implementation it verifies.
- **Acceptance tests**, written in behavior terms (given/when/then or equivalent) —
  describing what the system does from the outside, not how. These are the actual spec
  for a feature or fix; if a change can't be stated as an acceptance criterion, the
  requirement isn't clear enough yet to build against.

When a process mistake gets logged in `notes/dev/mistakes.md`, ask whether it's also a
missing test — a regression or acceptance test that would have caught it mechanically
next time, not just a process note relying on memory.

### Milestone gates

A milestone's PR opens only after these steps, in order:

1. **Code review.** A read-only reviewer agent reads every changed line, looking for defects,
   dead or repeated code, over-long units and tests that can't fail.
2. **Fixes.** Findings are fixed red/green. The failing test is committed on its own before the
   fix, so the red step is in the history.
3. **Test audit.** A read-only agent breaks the behaviour each acceptance test and each new or
   changed test claims to check (mutation testing), and lists every test that still passes.
4. **Test fixes.** Every surviving test is repaired the same way.

Expected values come from the specs, legacy output or hand calculation. A value recorded from
the code under test is a regression pin and is labelled as one: a passing pin is not evidence
of correctness.

## No Shortcuts

Nothing is deferred without explicit permission from the user. A known issue is still
a bug — do not mark it "won't fix", "by design", or "out of scope" unilaterally.

If a library or package cannot meet the stated requirements, the answer is to find an
alternative or do the work from first principles — not to defer the requirement or
revise it to fit the limitation. The requirements define what the project needs; the
implementation serves the requirements, not the other way around.

## Verification Discipline

Never state that something works, is fixed, or is verified unless it was checked at
that exact moment with a command whose output is the actual basis for the claim — not
memory of an earlier check, not knowledge of what the code is supposed to do, and not
a sub-agent's self-report taken at face value. The standard is identical in both
directions: the skepticism applied to a sub-agent's "done" (see Sub-Agent Workflow)
applies just as much to Claude's own claims to the user.

Before reporting a task or verification as complete:
- State the concrete, checkable success criteria before running anything — specific
  facts ("a PR exists against branch X containing files A and B"), not a general
  expectation ("it should work").
- Check every criterion with a fresh command at the time of the claim, and cite its
  actual output as the basis for what's reported.
- If a task has multiple required scenarios (e.g. two code paths, or a dev environment
  and the real deployment target), track them explicitly and don't report the whole
  task done until every one has been checked — a passing sub-step is not a finished
  task.
- Report against the criteria list: state plainly what's verified and what isn't,
  rather than describing the completed part in success language and leaving gaps
  implicit.

## Communication

Ask questions in natural language. Never use a multiple choice / structured question
tool — including Claude Code's `AskUserQuestion` tool — if clarification is needed,
just ask directly in plain text. This is a project-wide preference, not a
per-session one: some interfaces render binned/multiple-choice questions poorly,
and forcing a question into fixed options loses the nuance an open question
would surface. Standard engineering practice is to ask a real question and read
a real answer, not to pick from a menu.

## Autonomy

Make implementation decisions independently — don't ask permission for technical
choices within the stated requirements. Escalate only when something would change
scope, defer a requirement, or contradict what the user has described as the goal.

A structural choice made while implementing a functional request — naming, module
boundaries, a relationship between two pieces — is mine to propose, but must be
flagged as a proposal, not written into this file, a spec, or code comments with the
same authority as something the user actually decided.

A description of a desired change is not, by itself, authorization to execute it. If a
message separates *what* to do from *when* ("I'll tell you when"), wait for the
explicit go-ahead before acting — even on a fully-specified, low-risk change.

**IF YOU CANNOT DO EXACTLY WHAT WAS ASKED — DUE TO A TECHNICAL CONSTRAINT OR ANY OTHER
REASON — STATE THE CONSTRAINT AND STOP.** Do not silently substitute an alternative and
proceed to implement it in the same turn. Naming the blocker is not itself permission
to pick a workaround; the user decides which alternative (if any) to pursue. This
applies even when the substitute seems obviously reasonable.

**Two-strike auto-comply.** If corrected twice on the same point, treat the second
correction as an automatic stop: comply immediately, with no further justification or
re-explanation. Don't make the user repeat themselves a third time or invoke a
stop-word to get compliance — repetition itself is the signal.

**Mark proposals as proposals.** Any architectural or structural choice made while
implementing — one not a direct restatement of something the user actually decided —
gets written into a spec, `CLAUDE.md`, or other persistent doc as `[Proposed —
unconfirmed]`, not plain declarative text carrying the same authority as a real
decision. Don't unmark your own proposal; only the user confirming it (or leaving it
alone) makes it settled.

## Attribution

No attribution of any kind in commit messages, PR bodies, or issue text — no
"Generated with", "Co-Authored-By", "Created by Claude", or any AI/tool credit lines.

**Verify by reading the repo, not from memory.** Some git hosting integrations inject
a footer server-side even into a request that omitted one — treat that as expected
behavior, not a surprise. After every commit and after every PR create/update, re-read
the actual result and strip any attribution found, regardless of source:
- Run `git log` and read the actual commit messages
- Re-fetch and read the actual PR body text
- Remove any attribution found, regardless of source

A commit or PR is not finished until this read-back check has run — don't rely on what
you wrote, check what actually landed.

## GitHub Issues and PRs

Issue and PR templates live in `ScottKirvan/.github` (or your org's equivalent) and
apply to this repo automatically via GitHub's community health file fallback.

- Bug reports → `[BUG]` title prefix, `bug_report.md` sections
- Feature requests → `[FEATURE]` title prefix, `feature_request.md` sections
- General → `[GENERAL]` title prefix, `general_report.md` sections
- PRs → fill all checklist sections; no attribution anywhere in the body

Before creating any issue: check for duplicates first — `gh issue list --state open
--limit 100` where the `gh` CLI is available, or the equivalent GitHub search/list
tool (e.g. an MCP GitHub server's `search_issues`/`list_issues`) in hosted sessions
that don't have `gh`. Don't skip the check just because the literal command doesn't
apply in a given environment.
Create issues only when explicitly asked — don't preemptively file future work.

## Sub-Agent Workflow

When using sub-agents for implementation:

- Brief sub-agents on **what** to build, not **how** — implementation decisions belong
  to the sub-agent, which serves as an independent second opinion on the approach.
- Not every implementation choice is "how." A choice is **load-bearing** — and belongs
  in the brief as a stated constraint, not left implicit — if getting it wrong would
  foreclose a decision already made elsewhere, or if fixing it later would cascade into
  sibling components rather than staying local to the one being built. The test: would
  changing this later touch only this component, or would it touch others or
  contradict something already decided? Local and reversible → genuinely "how,"
  delegate freely. Cascading or hard to reverse → state it explicitly in the brief.
  (Architecture — how two components relate, e.g. whether one delegates to the other —
  is the case that's easiest to misclassify as "how" when it's actually load-bearing.)
- Sub-agents follow all conventions in this file except they do not create PRs.
- After a sub-agent completes, review its diff and tests before creating the PR.
  This review is a genuine code review, not a compliance check — evaluate correctness,
  requirement alignment, and test quality independently.
- Simple issues found in review may be fixed directly. Significant deviations from the
  stated requirements or complex problems go back to the sub-agent rather than being
  patched over.
- Create the PR only after review passes.
