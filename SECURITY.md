# Security policy

linscp is built on the core of [WinSCP](https://winscp.net/), so
some vulnerabilities belong to WinSCP and some to linscp's own code (the Qt
interface, the command-line client, and the POSIX/Win32 compatibility layer).

## Supported versions

Only the latest release is supported. linscp is pre-release; please reproduce
on the newest version before reporting.

## Verifying a download

From 0.7.2 on, each release carries `SHA256SUMS` and the source tarball with
OpenPGP signatures (`.asc`), made with linscp's release-signing key:

```text
A679 B8C9 915B B5AC DD45  F91E 207F B492 B5B2 14A9
Eduardo Correia (linscp release signing) <ecorreia@apliant.com.br>
```

The key is in this repository as
[doc/release-signing-key.asc](doc/release-signing-key.asc) and on
keys.openpgp.org. To check a download:

```sh
gpg --import doc/release-signing-key.asc     # or: gpg --recv-keys A679B8C9915BB5ACDD45F91E207FB492B5B214A9
gpg --verify SHA256SUMS.asc SHA256SUMS
sha256sum --check --ignore-missing SHA256SUMS
```

Release tags are signed with the same key (`git tag -v v0.7.2`).

## Reporting a vulnerability

**Please report privately — do not open a public issue or pull request for a
security problem.**

- Preferred: open a private advisory through GitHub, at
  <https://github.com/99ecarvalho/linscp/security/advisories/new>
  ("Report a vulnerability"). This keeps the report and the fix private until a
  release is ready.
- Or email **[security@linscp.org](mailto:security@linscp.org)** with the
  details and, if you can, a proof of concept. Start the subject with
  `[security]`. Email is not end-to-end encrypted: for very sensitive details,
  first describe only the kind of problem and ask for a safer channel.

A step-by-step guide to both channels is at <https://linscp.org/security/>.

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
