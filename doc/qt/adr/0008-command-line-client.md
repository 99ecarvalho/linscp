# ADR 0008 — The command-line client

**Status:** accepted 2026-10-12
**Date:** 2026-10-12

## Context

Upstream's scripting client is two programs: winscp.com owns the console and
relays it through shared memory to WinSCP.exe, whose `TConsoleRunner`
(`windows/ConsoleRunner.cpp`) runs core's `TManagementScript`. That runner
cannot be compiled without the VCL forms -- its header is the GUI's
`WinInterface.h`. WinSCP's .NET assembly drives WinSCP.exe the way winscp.com
does.

## Decision

- `linscp-cli` is a runner of its own over core's `TManagementScript`,
  following `TConsoleRunner` (queries, prompts, batch answers, timeouts,
  command expansion, `keepuptodate`) and winscp.com's console semantics; the
  script and its output are core's.
- Its console is an interface (`CliConsole.h`): standard input and output on
  every platform, and on Windows winscp.com's own protocol (`/consoleinstance`,
  over upstream's `console/Console.h`), so winscp.com and the stock .NET
  assembly drive linscp unchanged.
- On Linux the stock assembly cannot work: its transport is named events and
  named file mappings, which .NET provides only on Windows. A portable
  transport is a change to the assembly, to propose upstream (P1-23).
- It names itself linscp (`linscp> `), per the naming decision.

## Consequences

- One program on both platforms; the runner's logic is new code that must
  track `TConsoleRunner` by review.
- Verified on Windows: winscp.com driving linscp matches the golden corpus;
  the .NET assembly built from the same source runs full sessions over SFTP,
  FTP, FTPES, WebDAV and S3 (`port/tests/dotnet`).
