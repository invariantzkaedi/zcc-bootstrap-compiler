# Agent Instructions — zcc_github_upload

Load and apply on every session:
- `docs/lexicon/actionable-lexicon.md` — verbs for search/test/refactor/optimize work
- `docs/lexicon/influence-lexicon.md` — delta/evidence/de-risk framing; burned-words list is a hard ban
- `docs/lexicon/jsonl-cheat-guide.md` — trace, log, and eval record formats

Phase 0 protocol: snapshot history → read latest FORENSIC_*.md → read recent
tickets → run `make selfhost` gate → emit verdict block. No mutations before
a GREEN baseline and an authorized target.
