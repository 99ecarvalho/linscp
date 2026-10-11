#!/usr/bin/env python3
"""The patch series applier refuses anything but an exact, line-preserving fit."""
import importlib.util, os, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location(
    'patchset', os.path.join(HERE, '..', 'tools', 'patchset.py'))
patchset = importlib.util.module_from_spec(spec)
spec.loader.exec_module(patchset)

failures = 0


def check(cond, what):
    global failures
    if not cond:
        print('FAIL', what)
        failures += 1


def series_of(diff):
    d = tempfile.mkdtemp()
    p = os.path.join(d, 'x.patch')
    open(p, 'w').write(diff)
    return [('x.patch', patchset._parse(p))]


GOOD = '''A description, before any header.
--- a/source/f.h
+++ b/source/f.h
@@ -2,3 +2,3 @@
 one
-two
+TWO
 three
'''
CRLF = 'zero\r\none\r\ntwo\r\nthree\r\n'

used = set()
out = patchset.apply('source/f.h', CRLF, series_of(GOOD), used)
check(out == 'zero\r\none\r\nTWO\r\nthree\r\n', 'applies, keeping CRLF')
check(used == {('x.patch', 'source/f.h', 0)}, 'records the hunk as used')
check(patchset.apply('source/other.h', CRLF, series_of(GOOD)) == CRLF, 'leaves other files alone')

try:
    patchset.apply('source/f.h', 'zero\none\nchanged\nthree\n', series_of(GOOD))
    check(False, 'stale context must fail')
except patchset.PatchError as e:
    check('refresh the patch' in str(e), 'stale context names the remedy')

GROWS = GOOD.replace('+TWO\n', '+TWO\n+extra\n').replace('+2,3', '+2,4')
try:
    patchset.apply('source/f.h', CRLF, series_of(GROWS))
    check(False, 'a line-count change must fail')
except patchset.PatchError as e:
    check('line count' in str(e), 'line-count change is named')

# The real series applies to the real upstream files.
real = patchset.load_series()
check(len(real) >= 1, 'the series is not empty')
up = os.path.join(HERE, '..', '..', '3rd-party', 'winscp')
used = set()
for name, files in real:
    for rel in files:
        text = open(os.path.join(up, rel), newline='', encoding='utf-8',
                    errors='surrogateescape').read()
        patchset.apply(rel, text, real, used)
check(used == patchset.all_hunks(real), 'every hunk of the real series applies')

if failures:
    sys.exit(1)
print('all patch series tests passed')
