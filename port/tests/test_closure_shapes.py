#!/usr/bin/env python3
"""The closure shapes transform.py binds, and the ones it must refuse.

Refusal matters as much as binding: a shape it does not understand has to
fail the run, not be rewritten into something that merely compiles.
"""
import importlib.util, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location(
    'transform', os.path.join(HERE, '..', 'tools', 'transform.py'))
transform = importlib.util.module_from_spec(spec)
spec.loader.exec_module(transform)

failures = 0


def bind(text, ident):
    # The method name is the last occurrence in these fixtures.
    start = text.rindex(ident)
    r = transform.closure_expression(text, start, ident)
    return None if r is None else text[:r[0]] + r[2] + text[r[1]:]


def check(text, ident, expected):
    global failures
    got = bind(text, ident)
    if got != expected:
        print(f'FAIL {text!r}: got {got!r}, expected {expected!r}')
        failures += 1


check('E = Handler;', 'Handler', 'E = CLOSURE(Handler);')
check('P(&DoProgress, x);', 'DoProgress', 'P(CLOSURE(DoProgress), x);')
check('P(& DoProgress);', 'DoProgress', 'P(CLOSURE(DoProgress));')
check('F(Proxy.Output, &A);', 'Output', 'F(CLOSURE_OF(&Proxy, Output), &A);')
check('A.On = &Board.Copy;', 'Copy', 'A.On = CLOSURE_OF(&Board, Copy);')
check('F(T->Custom, 1);', 'Custom', 'F(CLOSURE_OF(T, Custom), 1);')
check('F(&T -> Custom);', 'Custom', 'F(CLOSURE_OF(T, Custom));')
# Refused: these must reach a human, not be guessed.
check('F(a.b.Name);', 'Name', None)
check('F(a->b.Name);', 'Name', None)
check('F(Get().Name);', 'Name', None)
check('F(Arr[0].Name);', 'Name', None)
# Logical and is not address-of.
check('if (x && Name)', 'Name', 'if (x && CLOSURE(Name))')

if failures:
    sys.exit(1)
print('all closure shape tests passed')
