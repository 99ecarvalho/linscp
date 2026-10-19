# Security policy

linscp is a Qt port of [WinSCP](https://winscp.net/). It runs WinSCP's core, so
some vulnerabilities belong to WinSCP and some to linscp's own code (the Qt
interface, the command-line client, and the POSIX/Win32 compatibility layer).

## Supported versions

Only the latest release is supported. linscp is pre-release; please reproduce
on the newest version before reporting.

## Reporting a vulnerability

**Please report privately — do not open a public issue or pull request for a
security problem.**

- Preferred: open a private advisory through GitHub, at
  <https://github.com/99ecarvalho/linscp/security/advisories/new>
  ("Report a vulnerability"). This keeps the report and the fix private until a
  release is ready.
- Or email **99ecarvalho@gmail.com** with the details and, if you can, a
  proof of concept.

Please include the linscp version, the platform (Linux or Windows), steps to
reproduce, and the impact as you see it.

### What to expect

- An acknowledgement, normally within a few days.
- Coordinated disclosure: a fix is prepared privately, released, and only then
  are the details made public, with credit to the reporter unless anonymity is
  requested.

### Scope

- Issues in linscp's own code are fixed here.
- Issues in WinSCP's core are reported upstream to the WinSCP maintainer as
  well; where users need protection sooner, linscp carries a fix until the
  upstream one is released.

Thank you for helping keep linscp's users safe.
