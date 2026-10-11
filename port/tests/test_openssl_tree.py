#!/usr/bin/env python3
"""openssl_tree.py accounts for every WinSCP change and overlays only adopted ones."""
import importlib.util, os, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location(
    'openssl_tree', os.path.join(HERE, '..', 'tools', 'openssl_tree.py'))
ot = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ot)

failures = 0


def check(cond, what):
    global failures
    if not cond:
        print('FAIL', what)
        failures += 1


def tree(files):
    d = tempfile.mkdtemp()
    for rel, data in files.items():
        p = os.path.join(d, rel)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        open(p, 'wb').write(data)
    return d


release = tree({'crypto/a.c': b'int a;\n', 'crypto/b.c': b'int b;\n',
                'crypto/c.c': b'int c;\n', 'Configure': b'#!perl\n'})
# a.c: a real change; b.c: identical but for CRLF; c.c: absent (trimmed);
# gen.h: generated, no counterpart in the release.
winscp = tree({'crypto/a.c': b'int a = 1;\r\n', 'crypto/b.c': b'int b;\r\n',
               'include/gen.h': b'#define X\n'})

check(ot.differing(release, winscp) == ['crypto/a.c'],
      'only real differences count; CRLF, trimmed and generated files do not')
check(ot.check({}, release, winscp) != [], 'an unlisted difference fails')
check(ot.check({'crypto/a.c': 'skip'}, release, winscp) == [], 'a listed one passes')
check(any('no longer differs' in p for p in
          ot.check({'crypto/a.c': 'skip', 'crypto/b.c': 'adopt'}, release, winscp)),
      'a listed file that no longer differs fails')

out = tempfile.mkdtemp()
written, total = ot.assemble(out, {'crypto/a.c': 'adopt'}, release, winscp)
check(total == 4 and written == 4, f'every release file is mirrored ({written}/{total})')
check(open(os.path.join(out, 'crypto/a.c'), 'rb').read() == b'int a = 1;\n',
      'an adopted file is WinSCP\'s, with LF endings')
check(open(os.path.join(out, 'crypto/c.c'), 'rb').read() == b'int c;\n',
      'files WinSCP trimmed come from the release')
check(not os.path.exists(os.path.join(out, 'include/gen.h')),
      'WinSCP\'s generated headers are not copied')
check(ot.assemble(out, {'crypto/a.c': 'adopt'}, release, winscp)[0] == 0,
      'a second run writes nothing, so mtimes are kept')
open(os.path.join(out, 'crypto/gone.c'), 'w').write('int gone;\n')
ot.assemble(out, {'crypto/a.c': 'adopt'}, release, winscp)
check(not os.path.exists(os.path.join(out, 'crypto/gone.c')),
      'a file the release no longer has is removed')
out2 = tempfile.mkdtemp()
ot.assemble(out2, {'crypto/a.c': 'skip'}, release, winscp)
check(open(os.path.join(out2, 'crypto/a.c'), 'rb').read() == b'int a;\n',
      'a skipped change leaves the release\'s file')

# The real trees, against the reviewed list.
if os.path.isfile(os.path.join(ot.RELEASE, 'Configure')):
    problems = ot.check(ot.load_changes())
    for p in problems:
        print('  ', p)
    check(not problems, 'the pinned trees match port/openssl/winscp-changes.txt')

print('OK' if not failures else f'{failures} failure(s)')
sys.exit(1 if failures else 0)
