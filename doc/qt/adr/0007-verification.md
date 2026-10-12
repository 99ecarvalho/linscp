# ADR 0007 — Verification: real servers, real WinSCP

**Status:** accepted 2026-10-12
**Date:** 2026-10-12

## Context

PLAN.md §5 chose golden-output differential testing against real WinSCP. Its
value depends on the comparison being fair: a difference must be the
client's, not the servers' or the machine's.

## Decision

- **Protocol matrix** (`protocols`): one everyday script over every protocol,
  checked on the server's side, against loopback servers started per test --
  OpenSSH, rclone, pyftpdlib, moto, and the suite's own FTP and S3 servers
  where nothing else exists -- and through SOCKS5 and HTTP proxies that must
  see the traffic. A missing server skips its protocol.
- **Golden corpus** (`golden`): five cases over seven protocols, captured with
  real WinSCP on a native Windows host (WinSCP 6.5.7), against servers that
  exist identically on both platforms (rclone for SFTP and WebDAV, the suite's
  FTP and S3 servers), fixed data and permissions, one committed certificate.
  The corpus pins the servers' and client's versions (`SERVERS.txt`,
  `CLIENT.txt`); the check skips, saying why, unless the same rclone is used.
  Normalising removes only the start time and the local working directory.
- Deliberate departures are listed per case (`known-differences.txt`), cases
  kept small so an entry hides nothing else.
- SCP is outside the golden comparison: no SCP server runs on both platforms.

## Consequences

- linscp matches real WinSCP on 33 of 35 logs; the two others are WinSCP 6.5.7's
  WebDAV `rmdir` crash, fixed in linscp and reported upstream (#69).
- The corpus must be recaptured when a case changes, on the Windows host.
