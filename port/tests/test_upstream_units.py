#!/usr/bin/env python3
"""upstream_units.py reads the unit list of each upstream project format."""
import importlib.util, os, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location(
    'upstream_units', os.path.join(HERE, '..', 'tools', 'upstream_units.py'))
uu = importlib.util.module_from_spec(spec)
spec.loader.exec_module(uu)
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
WINSCP = os.path.join(ROOT, '3rd-party', 'winscp')

failures = 0


def check(cond, what):
    global failures
    if not cond:
        print('FAIL', what)
        failures += 1


# neon: explicit rules; ne_ntlm.c exists but Makefile.bcb does not build it.
neon = uu.project_units(os.path.join(WINSCP, 'libs/neon/Makefile.bcb'))
check(len(neon) == 25, f'neon: 25 units ({len(neon)})')
check('libs/neon/src/ne_openssl.c' in neon, 'neon: ne_openssl.c')
check('libs/neon/src/ne_ntlm.c' not in neon, 'neon: ne_ntlm.c is not built upstream')

# libs3: an object list and .PATH.c.
s3 = uu.project_units(os.path.join(WINSCP, 'libs/libs3/Makefile'))
check(len(s3) == 14, f'libs3: 14 units ({len(s3)})')
check('libs/libs3/src/mingw_functions.c' in s3, 'libs3: mingw_functions.c')
check('libs/libs3/src/s3.c' not in s3, 'libs3: s3.c, the command-line tool, is not built')

# A make file naming an object it has no source for fails, rather than
# silently dropping the unit.
d = tempfile.mkdtemp()
open(os.path.join(d, 'x.bcb'), 'w').write('OBJS=$(OBJ_D)\\gone.obj\n')
try:
    uu.project_units(os.path.join(d, 'x.bcb'))
    check(False, 'an object without a source fails')
except SystemExit:
    pass

print('OK' if not failures else f'{failures} failure(s)')
sys.exit(1 if failures else 0)
