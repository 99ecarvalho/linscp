# ADR 0009 — Configuration on POSIX is WinSCP.ini

**Status:** accepted 2026-10-12
**Date:** 2026-10-12

## Context

Without `/ini`, WinSCP keeps its configuration in the registry, and in an INI
file when one exists (portable mode). POSIX has no registry; core already
supports INI storage. On Windows linscp keeps WinSCP's registry key, so
existing sites carry over.

## Decision

On POSIX the configuration is WinSCP's INI format, named `WinSCP.ini`: `/ini`
if given, else a `WinSCP.ini` beside the executable (upstream's portable
mode), else `$XDG_CONFIG_HOME/linscp/WinSCP.ini`. Storage keys are joined with
backslashes on every platform (`core-storage-key-backslashes.patch`), as the
INI sections are.

## Consequences

- A `WinSCP.ini` exported from Windows can be dropped in.
- Every linscp program shares one file, as WinSCP.exe and winscp.com share
  one configuration; upstream's automatic `<executable>.ini` would have given
  each its own.

## Verification

`ini_sites` reads stored sites through core's INI storage; `cli_session` opens
one by name and checks the file survives the configuration being saved.
