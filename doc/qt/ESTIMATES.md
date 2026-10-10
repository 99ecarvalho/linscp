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

## The binding constraint

**Code generation is not the bottleneck; human review and real-world
verification throughput are.**

This matters for planning:

- Compute cost is not the limiter. Reviewer attention is. A reviewer who merges
  a 4,000-line shim without reading it has converted a 3× multiplier into a
  latent defect fund.
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
