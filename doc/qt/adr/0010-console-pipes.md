# ADR 0010 — winscp.com's console protocol over inherited pipes on POSIX

**Status:** accepted 2026-10-12 (for linscp; proposed upstream for the assembly)
**Date:** 2026-10-12

## Context

WinSCP's .NET assembly drives WinSCP.exe with winscp.com's console protocol:
a `TConsoleCommStruct` (`console/Console.h`) in a named file mapping, handed
back and forth with named events, the process kept in a named job object.
`linscp-cli` speaks it on Windows (ADR 0008), and the stock assembly drives
linscp there (P1-20). Off Windows .NET has none of these named objects
(`EventWaitHandle` and `MemoryMappedFile` with a name throw
`PlatformNotSupportedException`), so the assembly cannot run there unchanged
(P1-23). Any fix needs a change to the assembly, which is upstream's.

Options weighed for the transport:

- **A Unix domain socket.** Needs `netstandard2.1` for
  `UnixDomainSocketEndPoint`, a file system name another local user could
  race to connect to first, and so a secret or peer-credential check.
- **Two anonymous pipes the process inherits.** In `netstandard2.0` and
  `net40` alike (`AnonymousPipeServerStream`); no name, so nothing to
  connect to; and with the child holding the only copies of its ends, end of
  file on either side means the other has gone -- what the job object gives.
- **Standard input and output.** Taken: the assembly already uses them for
  `/stdout` and `/stdin` transfers.

## Decision

Two inherited pipes. The caller starts the program with
`/console /consolepipes=<read>,<write>` (descriptor numbers). It first writes
the structure as it initialised it; each request is then the whole structure
written to `<write>`, and its response the whole structure read back from
`<read>`. Nothing else changes: the same structure, the same events in it,
the same version handshake (the program confirms `CurrentVersion`). The size
in the header must be `sizeof(TConsoleCommStruct)`, or the program refuses
before any request.

There is no cancel event: the assembly never sets it. The end of `<read>`
means the caller has gone; the program reports it as an abort
(`PendingAbort`) and fails its next request.

`TExternalConsole` speaks both transports, `/consoleinstance` on Windows and
`/consolepipes` elsewhere, with everything above the transport shared.

## Consequences

- The assembly needs the change: the
  `dotnet-console-pipes-transport` branch of 99ecarvalho/winscp, offered upstream. Until
  it is merged, .NET on Linux needs an assembly built from that branch.
- On Linux the assembly cannot read the executable's version resource (an
  ELF file has none), so the test sets `Session.DisableVersionCheck`; how the
  assembly should check the version there is an open question upstream.
- The pipe ends are inherited; another process the caller starts at the same
  moment could inherit them too. The caller closes its copies of the child's
  ends straight after starting it, as with any inherited handle.

## Verification

- `console_pipes` (`run_console_pipes.py`) plays the caller: the handshake,
  PRINT and INPUT events, a structure of another size refused, and the
  program exiting promptly when the caller goes mid-request.
- `dotnet` (`dotnet/run_dotnet.py`, when `LINSCP_DOTNET` and
  `LINSCP_WINSCPNET` are set): the assembly from that branch drives
  `linscp-cli` on Linux over SFTP, FTP, FTPES, WebDAV and S3; on Windows the
  same assembly, and the stock one, still pass through `/consoleinstance`.
