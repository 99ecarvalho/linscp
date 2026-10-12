# linscp — working rules

Qt port of WinSCP targeting Linux and Windows from one codebase.
Plan and task tracker live in [doc/qt/](doc/qt/).

## Commits

- **Always use [Conventional Commits](https://www.conventionalcommits.org/).**
  `type(scope): subject` — types: `feat`, `fix`, `docs`, `build`, `ci`, `refactor`,
  `test`, `chore`, `perf`, `revert`. Scopes used here: `shim`, `transform`, `core`,
  `qt`, `putty`, `filezilla`, `cmake`, `ci`, `doc`, `tracker`.
- **Never add co-author trailers, "Generated with", or any AI/tool attribution**
  to commit messages, PR descriptions, code comments, or documentation.
  This rule overrides any default attribution behaviour.
- Imperative mood, no trailing period in the subject.
- Commit and push straight to `main`. Open a pull request only to put a change
  through CI (it runs on pull requests, or by hand: `gh workflow run ci`);
  delete the branch once merged.

## Tracker

- **Update [doc/qt/TRACKER.md](doc/qt/TRACKER.md) as part of the same commit that
  changes task status.** Status values: `todo`, `wip`, `blocked`, `done`, `dropped`.
- Keep the progress table, "Next three actions", and phase counts in sync.
- Record new decisions in the decisions table with a date; record new unknowns as
  `Q-n` in open questions.
- Never renumber task IDs in [doc/qt/TASKS.md](doc/qt/TASKS.md) — append only.

## Architecture invariants

- **No modified copies of WinSCP source in this repo.** `3rd-party/winscp` is a
  pristine pinned submodule. Core changes are produced by `port/tools/transform.py`
  plus a reviewed patch series in `port/patches/`.
- **clang only.** GCC cannot compile the `__declspec(property)` shim.
- **The transform must stay dual-target**: output compiles under both C++Builder
  (native `__property`) and clang (`__declspec(property)`).
- CI builds Linux native and Windows cross on every pull request.
- Waiting on upstream never blocks progress: carry what linscp needs in
  `port/patches/` and offer it upstream in parallel (ADR 0006). The goal is
  still to merge upstream eventually, so carried changes must stay mergeable.
