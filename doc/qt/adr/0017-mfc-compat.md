# ADR 0017 — FileZilla's MFC API is linscp's own code (`port/mfc-compat`)

**Status:** proposed 2026-10-20 (draft)
**Date:** 2026-10-20

## Context

The FTP engine (`source/filezilla`) is written against a small part of MFC:
`CString`, `CTime` and `CFile`, reached through `#include <afx.h>` in its
`stdafx.h`. Until now the build compiled the MFC subset upstream vendors in
`3rd-party/winscp/libs/mfc` (target `linscp-mfc`, library `uafxcw`) from the
generated tree, with one carried patch. That subset is not free software, so
it cannot be part of a GPL distribution of linscp.

## Options

**A. Keep building the vendored subset.** Not distributable under the GPL.

**B. Rewrite the engine's call sites onto `UnicodeString` and Win32.**
Hundreds of call sites in about 19,000 lines; a large diff against upstream
on every merge (ADR 0006).

**C. Provide the API the engine uses as original code.** One header and
three small source files; the engine compiles unchanged.

## Decision

**Option C.** `port/mfc-compat` (GPL-3.0-or-later) provides `include/afx.h`
and `mfc_string.cpp`, `mfc_time.cpp`, `mfc_file.cpp`, built as
`linscp-mfc-compat`. Its output keeps the name `uafxcw`, because
`FileZillaIntf.cpp` requests that library by name
(`#pragma comment(lib, "uafxcw.lib")`). FileZilla's include path reaches
`port/mfc-compat/include` instead of the generated `libs/mfc/include`.
`libs/mfc` is no longer transformed, and `mfc-cstring-const-search.patch`
is retired with it.

### Scope

Only what the engine's call sites and its objects' undefined symbols need,
plus the closest companions of those members:

- `CString`: construction from wide and narrow text, with or without a
  length, and (explicitly) from `UnicodeString`; assignment; `GetLength`,
  `IsEmpty`, `Empty`, `GetAt`, `SetAt`, `operator[]`, `GetString`, `c_str`,
  `operator const wchar_t *`; `+=` and `+`; `Compare`, `CompareNoCase` and the
  six comparison operators; `Mid`, `Left`, `Right`; `Find`, `ReverseFind`,
  `FindOneOf`; `Replace`, `Delete`, `Insert`, `MakeLower`, `MakeUpper`,
  `TrimLeft`, `TrimRight`, `Trim`; `Format` (format string or string
  resource id) and `FormatV`.
- `CTime`: from `__int64` seconds and from a `FILETIME`;
  `CreateForCurrentTime`; `GetTime`; `GetLocalTm`, `GetGmtTm`; `GetYear`,
  `GetMonth`, `GetDay`, `GetHour`, `GetMinute`, `GetSecond`,
  `GetDayOfWeek`; assignment and comparison.
- `CFile`: the `OpenFlags`, `SeekPosition` and `Attribute` enumerations,
  `hFileNull`, the public `m_hFile`; construction (empty, from a handle,
  opening); `Open`, `Read`, `Write`, `Flush`, `Seek`, `SeekToBegin`,
  `SeekToEnd`, `GetPosition`, `GetLength`, `Close`, `Abort`,
  `operator HANDLE`.

`CString` stores a `UnicodeString`; `CTime` a 64-bit count of seconds since
1970 UTC; `CFile` a Win32 `HANDLE` used through the Win32 file API (the
platform layer's on POSIX, ADR 0002). No C runtime wide function is called
(ADR 0001); case folding is ASCII only, as in the shim.

### Behaviour

The layer behaves identically on Linux and Windows:

- **Narrow text.** A null-terminated `const char *` is decoded exactly as
  `UnicodeString` decodes one: UTF-8, one U+FFFD per maximal invalid
  subsequence (`"caf\xE9"` gives `caf` + U+FFFD). With an explicit length,
  each byte becomes the code point of the same value (Latin-1), embedded
  nulls kept. Neither form uses the system code page.
- **File errors.** Every `Open`, successful or not, remembers
  `"\r\n" + ExpandFileName(FileName)` as of that moment. A failed read,
  write, flush, seek, size query or close, and a short write (as
  `ERROR_DISK_FULL`), raise `EOSError` with exactly
  `RaiseLastOSError(Code, RememberedText)`'s message, where `Code` is the
  Win32 last error taken right after the failed call (no `errno` fallback; 0
  gives "A call to an OS function failed" alone). `Close` detaches and
  forgets the text before it raises. `Open` itself returns `FALSE` and
  leaves the last error for the engine's `LastSysErrorMessage`.
- **File times.** `CTime(FILETIME)` converts through local time:
  `FileTimeToLocalFileTime`, `FileTimeToSystemTime` (milliseconds dropped),
  then the C runtime's `mktime` with `tm_isdst = -1`. It returns `mktime`'s
  result, -1 included, and 0 for an all-zero FILETIME, a failed conversion
  or a local year before 1900. Core converts this timestamp back assuming
  that encoding; an exact UTC conversion would shift the local file time in
  the overwrite prompt by an hour on Windows whenever the DST state of the
  file's date differs from the current one.
- **Search, compare and edit results.** Searches treat the string as
  null-terminated and stop at its first null; character 0 finds the
  terminator (`Find`, `ReverseFind`); an empty substring is found at any
  `Start` from 0 to the length. `Compare` returns exactly -1, 0 or +1 by
  unsigned code-unit order; `CompareNoCase` returns the difference of the
  first differing ASCII-folded code units, or 0. `Delete` returns the
  length before the removal. `Replace` of text returns 1 if the content
  changed, else 0; `Replace` of a character returns the number replaced, 0
  when old and new are the same.
- **Inputs outside the documented ranges** are defined and safe:
  out-of-range `GetAt`/`SetAt` throw `EListError`; null arguments to
  `Compare` and `FindOneOf` are empty (-1 for `FindOneOf`); a negative
  `Start` is 0; counts and positions are clamped (a negative count gives an
  empty result, a count past the end stops at the end); a default `CTime`
  is 0, and its getters read 0 when the runtime cannot convert the time.
- **Destructor.** By design, a `CFile` destructor whose close fails neither
  raises nor ends the process: the failure is counted
  (`CFile::DestructorCloseFailures()`) and, on Windows, reported with
  `OutputDebugString`. An explicit `Close` raises instead.

The header records each behaviour next to the member.

### How it was written

Clean-room: from the engine's own call sites in `source/filezilla`, the
undefined symbols of its compiled objects, linscp's shim and platform layer,
and Microsoft's public reference documentation only. The implementer did not
view the vendored subset's source, its generated copies, the retired patch or
the compiled subset library.

Documentation consulted:

- <https://learn.microsoft.com/en-us/cpp/atl-mfc-shared/reference/cstringt-class>
- <https://learn.microsoft.com/en-us/cpp/atl-mfc-shared/reference/csimplestringt-class>
- <https://learn.microsoft.com/en-us/cpp/atl-mfc-shared/reference/ctime-class>
- <https://learn.microsoft.com/en-us/cpp/mfc/reference/cfile-class>

Where the interface differs from those pages it follows the call sites:
`CString` is a plain class, constructible from `UnicodeString` and with a
`c_str()`, its `UnicodeString` constructor explicit; `CFile::Open` takes
no exception argument; the current time is
`CTime::CreateForCurrentTime()` (`<windows.h>` defines `GetCurrentTime` as a
macro).

## Consequences

- Nothing non-free is compiled, generated or patched. Two guards keep it so:
  `cmake/no-mfc.cmake` fails configuration if any target's sources, include
  or link directories, or the transform's outputs, are under `libs/mfc`; the
  `no_mfc` test checks `compile_commands.json`, the transform's file lists,
  the generated tree and the patch series. Both look at paths only.
- The `mfc_compat` test covers every member against the behaviour above, on
  Linux and under Wine; the FTP, FTPES and FTPS session tests cover the
  engine on top of it.
- `wide_imports` and `shim_no_wide_abi` scan the layer as part of the port.
- A member the engine starts using upstream fails to compile and is added
  here, from the documentation.

## Verification

See the commit that introduces `port/mfc-compat`: Linux and Windows-cross
builds, `mfc_compat`, `no_mfc`, `wide_imports`, the link tests and the FTP
session tests.
