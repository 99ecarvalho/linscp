#!/usr/bin/env python3
"""
Keep doc/qt/TASKS.md and doc/qt/TRACKER.md consistent.

CLAUDE.md requires the tracker to be updated alongside the status change it
describes. Discipline alone let TASKS.md drift once already, so this asserts it:

  1. every task ID with a status in TRACKER is defined in TASKS
  2. every task defined in TASKS has a status row in TRACKER
  3. the per-phase done/total counts match the status rows
"""
import re, sys, collections

TASKS   = 'doc/qt/TASKS.md'
TRACKER = 'doc/qt/TRACKER.md'
VALID   = {'todo', 'wip', 'blocked', 'done', 'dropped'}
ROW     = re.compile(r'^\|\s*(P\d-\d+)\s*\|')
STATUS  = re.compile(r'^\|\s*(P\d-\d+)\s*\|[^|]*\|\s*(\w+)\s*\|')
COUNT   = re.compile(r'^\|\s*(P\d)\s[^|]*\|\s*(\d+)\s*\|\s*(\d+)\s*\|')

defined = {m.group(1) for l in open(TASKS) if (m := ROW.match(l))}
tracker = open(TRACKER).read()
status, counts = {}, {}
for l in tracker.split('\n'):
    if m := STATUS.match(l):
        status[m.group(1)] = m.group(2)
    elif m := COUNT.match(l):
        counts[m.group(1)] = (int(m.group(2)), int(m.group(3)))

errs = []
for t in sorted(status.keys() - defined):
    errs.append(f'{t}: has a status in TRACKER but is not defined in TASKS')
for t in sorted(defined - status.keys()):
    errs.append(f'{t}: defined in TASKS but has no status row in TRACKER')
for t, s in sorted(status.items()):
    if s not in VALID:
        errs.append(f'{t}: invalid status {s!r} (expected one of {sorted(VALID)})')

by_phase = collections.Counter(t.split('-')[0] for t in status)
done_by_phase = collections.Counter(t.split('-')[0] for t, s in status.items() if s == 'done')
for phase, (done, total) in sorted(counts.items()):
    actual_total, actual_done = by_phase[phase], done_by_phase[phase]
    if (done, total) != (actual_done, actual_total):
        errs.append(f'{phase}: progress table says {done}/{total}, '
                    f'status rows say {actual_done}/{actual_total}')

print(f'tasks defined: {len(defined)}   with status: {len(status)}')
print('by phase: ' + '  '.join(f'{p}={done_by_phase[p]}/{by_phase[p]}'
                               for p in sorted(by_phase)))
if errs:
    print(f'\nINCONSISTENCIES ({len(errs)}):')
    for e in errs:
        print('  ', e)
    sys.exit(1)
print('\nOK: TASKS and TRACKER agree')
