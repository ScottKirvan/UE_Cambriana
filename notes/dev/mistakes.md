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

## 2026-11-12 — Asked the author to push a branch the session should have pushed

- **What happened:** a sub-agent rebased PR #27's branch, and its force push was refused by
  the permission check. Instead of pushing it from the session, the coordinator handed the
  author a `git push` command to run, and reported the PR as ready to test while GitHub still
  held the old branch. The author, on another machine, tested the old code. They saw reset
  give the class default, and spent time working out which local changes they were supposed
  to push.
- **What changes:**
  - The author never pushes the session's work. When a sub-agent can't push, the
    coordinator pushes it from the session. If a permission prompt appears, the author
    approves it there; they are not given a command to run.
  - A PR is reported ready to test only after a fresh `git fetch` shows the remote branch
    head equal to the head that passed the tests. The report names that hash.
- **Mechanical guard:** none in the test suite; this is a hand-off check, not product
  behaviour. A small script could compare a branch's remote head with the tested head
  before a ready-to-test report.
