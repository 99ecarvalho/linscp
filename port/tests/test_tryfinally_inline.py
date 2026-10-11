#!/usr/bin/env python3
"""A try/__finally written on one line is rewritten, by transform.py and as
verify_transform.py independently expects, to the dual-target macros."""
import importlib.util, os, sys

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


LINE = '  if (Open()) try { Read(); } __finally { Close(); }\r'
WANT = '  if (Open()) LINSCP_TRY { Read(); } LINSCP_FINALLY { Close(); LINSCP_END_FINALLY\r'

t = transform.Transformer(bind_sites=False, src_root=None)
lines = [LINE]
t.transform_try_finally('x.cpp', lines)
check(lines == [WANT], f'transform: {lines!r}')
check(verify.inline_tryfinally_of(LINE) == WANT, 'verifier expects the same rewrite')

# Nested braces in the guarded body and the cleanup.
NESTED = 'try { if (a) { b(); } } __finally { if (c) { d(); } }'
lines = [NESTED]
t.transform_try_finally('x.cpp', lines)
check(lines == ['LINSCP_TRY { if (a) { b(); } } LINSCP_FINALLY { if (c) { d(); } LINSCP_END_FINALLY'],
      f'nested: {lines!r}')
check(verify.inline_tryfinally_of(NESTED) == lines[0], 'verifier, nested')

# A return in the guarded body would return from the lambda: refused.
RET = 'try { return f(); } __finally { g(); }'
lines = [RET]
t.transform_try_finally('x.cpp', lines)
check(lines == [RET], 'a body with return is left alone')
check(any('return' in u[2] for u in t.unhandled), 'and reported as unhandled')

# Not a try/__finally: untouched by both.
for other in ['try { a(); } catch (...) { b(); }', 'int retry = 1;']:
    lines = [other]
    t.transform_try_finally('x.cpp', lines)
    check(lines == [other], f'untouched: {other}')
    check(verify.inline_tryfinally_of(other) is None, f'verifier: none for {other}')

print('OK' if not failures else f'{failures} failure(s)')
sys.exit(1 if failures else 0)
