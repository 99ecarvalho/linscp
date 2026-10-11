# port/casefold

Forwarding headers for includes whose spelling differs from the upstream file
name only in case. They resolve on Windows' case-insensitive file systems and
nowhere else, so a Linux build -- or a Linux host cross-compiling for Windows
-- cannot find them.

Each file here carries the spelling upstream uses and includes the real file by
relative path. Upstream stays untouched.

The set is explicit rather than generated. A basename match is not enough:
`putty/windows/gss.c` includes `security.h`, which is the Windows SDK's SSPI
header, not core's `Security.h`. A mismatch counts only when the real file sits
in the including file's own directory, which is what a case-insensitive file
system would have found. `port/tools/check_include_case.py` applies that rule,
and fails when upstream gains a mismatch with no forwarding header here, or
when one of these is no longer needed or no longer reaches its target.
