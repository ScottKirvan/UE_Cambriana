# Dev Workflow Mistakes

This is not a blame log. It's a running record of process/workflow slips — mine or an
agent's — worth remembering, so the same mistake doesn't get repeated. Each entry
should be concrete enough to recognize the pattern next time, and say what changes
going forward. See the Testing Strategy section of [CLAUDE.md](../../CLAUDE.md) for how
an entry here should also prompt a check for a missing regression or acceptance test.

---

## 2026-11-11 — Grammar editor PR opened without a line-by-line review

- **What happened:** step 7a's PR (grammar editor with Rules and Details tabs) was opened
  without a line-by-line review of the diff. The review done afterwards found that a Rules
  edit reached the grammar asset only when the text box lost focus, so an edit saved with
  Ctrl+S, or left when the editor closed, could be missing from the asset, and the asset
  was not marked dirty.
- **What changes:** the milestone gates' code review (every changed line read) runs before
  a step's PR opens, not after.
- **Mechanical guard:** `Cambriana.Editor.GrammarEditor.RulesTypingIsSavedWhenTheGrammarIsSavedBeforeAPause`
  types into Rules and saves without moving focus; with
  `RulesTypingDirtiesTheGrammarAndReachesItAfterAPauseAsOneRebuildAndOneUndo` and
  `RulesTypingReachesTheGrammarWhenTheEditorClosesBeforeAPause` it fails if a Rules edit
  again depends on focus loss.
