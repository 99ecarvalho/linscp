#!/usr/bin/env python3
"""
The reviewed patch series: changes to upstream that no general rule expresses.

port/patches/series lists unified diffs, in order, against the pristine
submodule (paths a/source/... b/source/...). They are applied to a file's text
before transform.py's rules, and verify_transform.py applies the same series
to its baseline.

Strict by design:
  - context and removed lines must match exactly, or the run fails: a patch
    upstream has moved past must be refreshed, never fuzzed into place;
  - every hunk must keep its line count, preserving the generated tree's
    one-line-in, one-line-out correspondence with upstream;
  - line endings are the file's own: lines are compared without a trailing
    CR and written back with it, so one patch serves CRLF and LF files.
"""
import os, re, sys

PATCHES = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'patches')
HUNK = re.compile(r'^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@')


class PatchError(Exception):
    pass


def _parse(path):
    """{file: [(old_start, [(op, text), ...]), ...]} for one unified diff."""
    files = {}
    current = None
    hunk = None
    for raw in open(path, encoding='utf-8').read().replace('\r', '').split('\n'):
        if raw.startswith('+++ '):
            name = raw[4:].split('\t')[0]
            current = name[2:] if name.startswith('b/') else name
            files.setdefault(current, [])
            hunk = None
            continue
        if raw.startswith('--- '):
            continue
        m = HUNK.match(raw)
        if m:
            if current is None:
                raise PatchError(f'{path}: hunk before any file header')
            hunk = (int(m.group(1)), [])
            files[current].append(hunk)
            continue
        if hunk is not None and raw[:1] in (' ', '-', '+'):
            hunk[1].append((raw[0], raw[1:]))
        elif hunk is not None and raw.startswith('\\'):
            continue                       # "\ No newline at end of file"
    return files


def load_series():
    """[(patch name, {file: hunks}), ...] in series order."""
    series = os.path.join(PATCHES, 'series')
    if not os.path.exists(series):
        return []
    result = []
    for line in open(series, encoding='utf-8'):
        name = line.split('#', 1)[0].strip()
        if name:
            result.append((name, _parse(os.path.join(PATCHES, name))))
    return result


def apply(rel_path, text, series, used=None):
    """text with every hunk for rel_path applied; records use in `used`."""
    rel = rel_path.replace(os.sep, '/')
    lines = text.split('\n')
    for name, files in series:
        for index, (start, ops) in enumerate(files.get(rel, [])):
            old = [t for op, t in ops if op in (' ', '-')]
            new = [t for op, t in ops if op in (' ', '+')]
            if len(old) != len(new):
                raise PatchError(f'{name}: hunk {index + 1} for {rel} changes the '
                                 f'line count ({len(old)} -> {len(new)})')
            at = start - 1
            window = [l.rstrip('\r') for l in lines[at:at + len(old)]]
            if window != old:
                raise PatchError(f'{name}: hunk {index + 1} does not apply to {rel} '
                                 f'at line {start} -- refresh the patch against upstream')
            for k, replacement in enumerate(new):
                cr = '\r' if lines[at + k].endswith('\r') else ''
                lines[at + k] = replacement + cr
            if used is not None:
                used.add((name, rel, index))
    return '\n'.join(lines)


def all_hunks(series):
    return {(name, rel, i) for name, files in series
            for rel, hunks in files.items() for i in range(len(hunks))}
