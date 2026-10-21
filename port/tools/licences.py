"""Which third-party licences oblige whoever ships a component's binaries to
make its source code available: the copyleft ones (the GPL family, the MPL,
the EPL, the CDDL, the EUPL). The packaging tools (build_appimage.py,
deploy_windows.py) offer the source of those components, and
fetch_sources.py collects it; permissive licences ask only for their notices,
which the packages carry for every component.

A licence is an SPDX expression: MSYS2's package metadata for Windows, and
for the AppImage port/packaging/appimage/licences.json (Debian's copyright
files name the licences of every file in a source package, its build scripts
and tools among them, not of what is shipped from it). An expression with alternatives ("AFL-2.1 or GPL-2+") is
copyleft only when every alternative is, as the choice is the distributor's.
An exception that lifts the obligation for what is shipped makes a licence
permissive here: the GCC Runtime Library Exception (libgcc, libstdc++ and the
other runtime libraries compiled code links), and the Autoconf, Libtool and
Bison exceptions (which cover build scripts and generated parsers).
"""
import re

COPYLEFT = re.compile(r'\b(A?GPL|LGPL|MPL|EPL|CDDL|EUPL)', re.IGNORECASE)
LIFTED = re.compile(r'GCC[- ]?(runtime|exception)|runtime[- ]library[- ]exception|autoconf|libtool|bison',
                    re.IGNORECASE)
ALTERNATIVES = re.compile(r'\s+or\s+', re.IGNORECASE)


def copyleft(licences):
    """Whether any of LICENCES (names or expressions) is copyleft; unknown
    (no names at all) counts as copyleft, to be safe."""
    if not licences:
        return True
    for expression in licences:
        alternatives = [a.strip(' ()') for a in ALTERNATIVES.split(expression) if a.strip(' ()')]
        if alternatives and all(COPYLEFT.search(a) and not LIFTED.search(a) for a in alternatives):
            return True
    return False
