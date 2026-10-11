#!/usr/bin/env python3
"""Core's `long` is respelled LONG and `unsigned long` ULONG, by transform.py
and as verify_transform.py independently expects -- in code only, never
`long long` or `long double`, and not on the lines lp64_long_keep.txt keeps."""
import importlib.util, os

HERE = os.path.dirname(os.path.abspath(__file__))


def load(name):
    spec = importlib.util.spec_from_file_location(
        name, os.path.join(HERE, '..', 'tools', name + '.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


transform = load('transform')
verify = load('verify_transform')
failures = 0


def check(cond, what):
    global failures
    if not cond:
        print('FAIL', what)
        failures += 1


CASES = [
    ('unsigned long X; long Y;', 'ULONG X; LONG Y;'),
    ('static_cast<unsigned long>(a)', 'static_cast<ULONG>(a)'),
    ('static_cast<__int64>(static_cast<signed long>(GetCardinal()))',
     'static_cast<__int64>(static_cast<LONG>(GetCardinal()))'),
    ('return (RemainingLength >= sizeof(unsigned long));', 'return (RemainingLength >= sizeof(ULONG));'),
    ('long long A; unsigned long long B; long double C;', 'long long A; unsigned long long B; long double C;'),
    ('unsigned long int D; signed long E; long int F;', 'ULONG D; LONG E; LONG F;'),
    ('unsigned  long G;\r', 'ULONG G;\r'),
    ('const long * P = 0L;', 'const LONG * P = 0L;'),
    ('// unsigned long, in a comment', '// unsigned long, in a comment'),
    ('S = "long"; long Z; /* long */ long W;', 'S = "long"; LONG Z; /* long */ LONG W;'),
    ("C = 'l'; GetLongPathName(long_name, longest);", "C = 'l'; GetLongPathName(long_name, longest);"),
]

t = transform.Transformer(bind_sites=False, src_root=None)
for old, want in CASES:
    lines = [old]
    t.lp64_long_types('source/core/X.cpp', lines)
    check(lines == [want], f'transform: {old!r} -> {lines[0]!r}, want {want!r}')
    check(verify.long_rule('source/core/X.cpp', [old]) == [want], f'verifier: {old!r}')

# A block comment spanning lines.
BLOCK = ['/* long', 'still long */ long L;']
WANT_BLOCK = ['/* long', 'still long */ LONG L;']
lines = list(BLOCK)
t.lp64_long_types('source/core/X.cpp', lines)
check(lines == WANT_BLOCK, f'transform, block comment: {lines!r}')
check(verify.long_rule('source/core/X.cpp', list(BLOCK)) == WANT_BLOCK, 'verifier, block comment')

# Outside core, nothing changes: PuTTY and FileZilla keep their own long.
for path in ('source/putty/X.c', 'source/filezilla/X.cpp'):
    lines = ['unsigned long X;']
    t.lp64_long_types(path, lines)
    check(lines == ['unsigned long X;'], f'transform touched {path}')
    check(verify.long_rule(path, ['unsigned long X;']) == ['unsigned long X;'], f'verifier touched {path}')

# A kept line: core implementing PuTTY's frontend API keeps PuTTY's type.
KEPT = 'unsigned long schedule_timer(int ticks, timer_fn_t /*fn*/, void * /*ctx*/)'
lines = [KEPT]
t.lp64_long_types('source/core/PuttyIntf.cpp', lines)
check(lines == [KEPT], 'transform rewrote a kept line')
check(verify.long_rule('source/core/PuttyIntf.cpp', [KEPT]) == [KEPT], 'verifier rewrote a kept line')

if failures == 0:
    print('OK')
raise SystemExit(1 if failures else 0)
