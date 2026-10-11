# Estimates

## Assumptions

- **Human baseline:** one senior C++ developer, full-time, familiar with Qt and
  cross-platform build systems, but *not* initially familiar with this codebase.
- **AI-assisted:** the same developer driving Opus 5 as the primary implementer,
  reviewing everything that lands.
- Calendar months, full-time. Includes code review, debugging and testing — not
  just first-draft authoring.
- Access to a Windows CI runner and containerised protocol servers.
- Excludes project management, release engineering, docs for end users, and
  community/maintainer negotiation.

## Where AI actually multiplies — and where it does not

The multiplier is highly uneven across this project, and the uneven parts are
what the estimate turns on.

### Strong fit (3–8×)

| Work | Why |
|---|---|
| `transform.py` (P0-05/06) | Mechanical, 4 regular forms, immediately self-testable by re-running |
| VCL RTL shim (P1-01…06) | Well-specified APIs, a maintained reference to crib from (Far-NetBox `src/base/`), and unit-testable in isolation. The single largest chunk of P1 is also the best-suited |
| `win32-on-posix` (P1-07) | ~40 functions with precise documented semantics |
| CMake / CI / toolchain files (P0-08…12) | High boilerplate, fast feedback |
| PuTTY `unix/` re-import (P1-09) | Mostly reconciling two known trees |
| `.dfm` → `.ui` + dialog port (P3-01/04/06) | `.dfm` is a *declarative spec* of layout, widgets, captions and tab order. This is the biggest single AI win in the project, and it sits in the most expensive phase |

### Moderate fit (1.5–2.5×)

| Work | Why |
|---|---|
| Qt `Interface.h` implementation, file panels, queue UI (P2-03…08) | Real interaction logic and design judgment; AI drafts well, polish is human |
| Commander dual-pane semantics (P3-02) | Deep keyboard-driven interaction behaviour users will notice immediately |
| First successful link (P1-17) | AI is excellent per-fix, but the iteration count is high and each cycle is gated on a working build |

### Weak fit (1–1.5×)

| Work | Why |
|---|---|
| **FileZilla epoll rewrite (P1-12)** | ~1,700 lines of subtle async state machine. Correctness is established by testing against real servers, not by reading code. Verification-bound |
| **Protocol correctness (P0-15, P1-19)** | The FTP server zoo, SFTP extension quirks, S3 and WebDAV edge cases. Wall-clock bound by test infrastructure and real-world behaviour |
| C++Builder / Windows-native tasks (P0-04, P0-15) | No local access; bound on a collaborator or licence |
| UX parity judgment | Deciding what "parity" means is not a code task |

## Per-phase estimate

| Phase | Human (mo) | + Opus 5 (mo) | Multiplier | Confidence |
|---|---|---|---|---|
| P0 Foundation | 2 – 3 | 0.5 – 1.0 | ~3–4× | **High** — scope is well understood and the keystone is already tested |
| P1 Headless core | 6 – 12 | 2.5 – 4.0 | ~2.4–3× | **Medium** — dominated by the shim (strong fit), dragged by P1-12 and P1-19 (weak fit) |
| P2 Qt shell | 6 – 9 | 2.0 – 3.0 | ~3× | **Medium** |
| P3 Qt parity | 18 – 36 | 6.0 – 12.0 | ~3× | **Low** — scope is undecided (Q-1) and this phase dominates the total |
| P4 Cleanup | 1 – 2 | 0.25 – 0.5 | ~4× | High |
| **Total** | **33 – 62** | **11 – 21** | **~3×** | |

**One developer + Opus 5: roughly 11–21 months**, against 2.8–5.2 years
unassisted.

### With a small team

P3 parallelises well — dialogs are largely independent — and P2 moderately.
P0 and P1-17 do not parallelise much, being serialised on a single build
bring-up.

| Config | Total |
|---|---|
| 1 dev + Opus 5 | 11 – 21 months |
| 3 devs + Opus 5 | 6 – 11 months |

## Compute cost

Unlike the rest of this document, this section is **measured, not estimated**.
Every commit on the `qt` branch so far was produced by `claude-opus-5`, and the
session transcripts carry per-turn token usage. Figures below are for the branch
state at `c801756` (2026-10-11): 22 commits, 16,960 lines added, 24 shim tests
green on both targets, 5 of 9 tracked core units compiling clean on both.

### Tokens consumed

| Category | Tokens |
|---|---|
| Base input | 2,160 |
| Cache writes (1h TTL) | 1,922,665 |
| **Cache reads** | **473,255,696** |
| Output (incl. reasoning) | 1,654,640 |
| **Total** | **476,835,161** |

Cache reads are 99.2% of all tokens. That is the signature of this kind of work:
a long-lived session re-reading a large working context (a 400k-line C++ tree,
the shim, the plan docs) on every turn. It dominates the bill, so the cache-read
rate — not the output rate — is what the cost turns on.

### API price sheet

List prices per million tokens, from
[platform.claude.com/docs/en/about-claude/pricing](https://platform.claude.com/docs/en/about-claude/pricing)
as of 2026-10-11.

| | Opus 5 | Opus 5.5 |
|---|---|---|
| Base input | $5.00 | $4.00 |
| Cache write, 5 min | $6.25 | $5.00 |
| Cache write, 1 hour | $10.00 | $8.00 |
| Cache read (hit/refresh) | $0.50 (0.1× input) | **$0.20 (0.05× input)** |
| Output | $25.00 | $20.00 |
| Batch input / output | $2.50 / $12.50 | $2.00 / $10.00 |
| Fast mode input / output | $10.00 / $50.00 | $8.00 / $40.00 |

Batch halves the base rates but is unusable here — the work is interactive and
each step depends on the previous build result.

### Cost of the work done so far

The same measured token mix, priced under both sheets:

| Line item | Opus 5 | Opus 5.5 |
|---|---|---|
| Base input | $0.01 | $0.01 |
| Cache writes | $19.23 | $15.38 |
| Cache reads | $236.63 | $94.65 |
| Output | $41.37 | $33.09 |
| **Total** | **$297.23** | **$143.13** |
| Share of bill that is cache reads | 80% | 66% |
| Per commit (22) | $13.51 | $6.51 |
| Per 1,000 lines landed | $17.53 | $8.44 |

**Opus 5.5 is 52% cheaper on this workload, not the 20% the headline list price
suggests.** The reason is the cache-read multiplier: 0.05× base input instead of
0.1×, which on a session that is 99% cache reads is the only number that matters.
Any workload shaped like this one — long sessions over a large fixed codebase —
should expect the same disproportionate saving.

Caveat on reasoning effort: **price per token does not vary with effort.** Effort
changes how many reasoning tokens are produced, not the rate they are billed at.
So the Opus 5 column is a bill, but the Opus 5.5 column is *this session's token
mix re-priced* — not Opus 5.5 actually doing the work. If 5.5 reaches the same
result in fewer tokens the real figure is lower; if it deliberates more, higher.

### Extrapolated to completion

Progress is ~14% of the plan by month-weighted task completion (P0 26/35 and
P1 18/36 against the per-phase Opus 5 budgets above). Scaling the measured spend:

| Scenario | Plan progress | Opus 5 | Opus 5.5 |
|---|---|---|---|
| Full parity, optimistic (11 mo) | 19.8% | $1,498 | $722 |
| Full parity, midpoint (15.9 mo) | 13.7% | $2,162 | $1,041 |
| Full parity, pessimistic (21 mo) | 10.4% | $2,860 | $1,377 |
| **Narrow Qt client (Q-1), 8–14 mo** | 15–27% | **$1,090 – $1,907** | **$525 – $918** |

Order of magnitude only. The extrapolation assumes cost per unit of plan progress
stays flat, and it will not: P3 generates Qt UI from `.dfm` declarative specs,
writing more and searching a huge C++ tree less, so the cache-read share that
dominates today should fall as the project moves into the UI phases.

### Subscription seats are a different currency

The figures above are API list prices. On a Claude subscription seat (Pro, Max,
or a Team / seat-based Enterprise seat) none of it is billed per token: usage is
included up to the plan's rolling 5-hour and weekly limits. Spend beyond those
limits is only possible if **usage credits** (formerly "extra usage") are enabled
— on Team and seat-based Enterprise that is an Owner / Primary Owner decision,
with optional monthly spend caps per organisation, per seat tier and per member.
With credits disabled, hitting the limit pauses work until it resets; it cannot
generate a surprise invoice. See
[Max plan limits](https://support.claude.com/en/articles/11049741-what-is-the-max-plan)
and
[extra usage for Team and seat-based Enterprise plans](https://support.claude.com/en/articles/12005970-extra-usage-for-team-and-seat-based-enterprise-plans).

The practical consequence for planning this port: **on a subscription seat the
limit is wall-clock throughput, not money.** That reinforces the conclusion
below rather than changing it.

## The binding constraint

**Code generation is not the bottleneck; human review and real-world
verification throughput are.**

This matters for planning:

- Compute cost is not the limiter. Reviewer attention is. The measurement above
  settles the first half of that claim: the whole port costs somewhere between a
  few hundred and a few thousand dollars of inference, against 2.8–5.2
  developer-years unassisted. A reviewer who merges a 4,000-line shim without
  reading it has converted a 3× multiplier into a latent defect fund, and that
  is the cost that actually scales.
- The highest-leverage investment is therefore **P0-13/14 (the golden-output
  harness and server containers)**. It raises the trustworthy review rate for
  every phase that follows. Underfunding it caps the achievable multiplier for
  the whole project, which is why it sits in Phase 0 rather than "later".
- Phases whose cost is dominated by *testing against reality* (P1-12, P1-19)
  barely move with AI. Do not plan as though they will.

## Sensitivity

The estimate is dominated by one undecided question.

| Variable | Effect |
|---|---|
| **P3 scope (Q-1)** | A deliberately narrower Qt client cuts P3 from 6–12 to ~3–5 months → **total 8–14 months**. This is the single largest lever in the plan |
| **R1 fails** (C++Builder rejects the macro) | P0 +0.5 mo, and the zero-diff upstream story weakens — higher ongoing merge cost, not a higher build cost |
| **R2 worse than expected** (FileZilla rewrite) | P1 +1–2 mo. Mitigation: ship SFTP/SCP/S3/WebDAV first and treat FTP as a later deliverable |
| **No Windows host available** | P0-04 and P0-15 stay blocked; the dual-target guarantee becomes unverified and Windows releases cannot be certified |

## Honest caveats

- These numbers come from **static analysis only**. Nothing here has been built.
  Line counts, grep results and the two compiler experiments are solid; the
  estimates derived from them are judgment.
- The usual failure mode for ports of this shape is a long tail of small
  incompatibilities discovered only at link and run time (P1-17). The P1 range
  is wide for that reason, and it is the range most likely to be exceeded.
- **P0-09 is the cheapest way to replace guesswork with measurement.** Compiling
  one real core translation unit against a minimal shim will tell us more about
  the P1 estimate than any further analysis, and it is roughly a week of work.
