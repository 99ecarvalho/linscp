#!/usr/bin/env python3
"""
Check linscp's menu bar against WinSCP's Commander's: the menus in WinSCP's
order, and each menu's items in the order WinSCP has them. linscp may leave
items out (what it does not have yet) but may not reorder them, and may add
only the items listed in EXTRA. WinSCP's menus are read from its forms
(ScpCommander.dfm, the captions from NonVisual.dfm's actions); linscp's from
`test_qt_gui menus`.

  check_menus.py --winscp <winscp/source> [--emulator wine] test_qt_gui[.exe]
"""
import argparse, os, re, subprocess, sys

# linscp's own: (menu path, caption).
EXTRA = {
    ('Files', 'Edit With'), ('Files', 'Copy'), ('Files', 'Move'),
}
# WinSCP's captions that linscp's shell gives otherwise.
RENAMES = {'LocalX': 'Left', 'RemoteX': 'Right'}


def clean(caption):
    caption = caption.replace('&', '')
    return caption[:-3] if caption.endswith('...') else caption


def dfm_string(text):
    out = ''
    for quoted, code in re.findall(r"'((?:[^']|'')*)'|#(\d+)", text):
        out += chr(int(code)) if code else quoted.replace("''", "'")
    return out


def winscp_menus(source):
    forms = os.path.join(source, 'forms')
    actions = {}
    nv = open(os.path.join(forms, 'NonVisual.dfm'), encoding='latin-1').read()
    for m in re.finditer(r'object (\w+): T\w*Action\b(.*?)\n    end', nv, re.S):
        c = re.search(r'\n\s+Caption = (.+)', m.group(2))
        actions[m.group(1)] = dfm_string(c.group(1)) if c else ''
    lines = open(os.path.join(forms, 'ScpCommander.dfm'), encoding='latin-1').read().split('\n')
    start = next(i for i, l in enumerate(lines) if 'object MenuToolbar' in l)
    base = len(lines[start]) - len(lines[start].lstrip())
    items = []  # (depth, caption)
    for i in range(start + 1, len(lines)):
        line = lines[i]
        indent = len(line) - len(line.lstrip())
        if indent <= base and line.strip() == 'end':
            break
        m = re.match(r'object \w+: (\w+)', line.strip())
        if not m:
            continue
        caption, action, visible = None, None, True
        for j in range(i + 1, len(lines)):
            t = lines[j].strip()
            tindent = len(lines[j]) - len(lines[j].lstrip())
            if tindent <= indent or t.startswith('object'):
                break
            if tindent == indent + 2:
                if t.startswith('Caption = '):
                    caption = dfm_string(t[10:])
                elif t.startswith('Action = '):
                    action = t.split('.')[-1]
                elif t == 'Visible = False':
                    visible = False
        if not visible or 'Toolbar' in m.group(1):
            continue
        text = '----' if m.group(1) == 'TTBXSeparatorItem' else (actions.get(action, '') if action else caption or '')
        items.append(((indent - base) // 2 - 1, clean(text)))
    return tree(items)


def tree(items):
    """[(depth, caption)] to {caption: [children...]} keeping the order."""
    root = []
    stack = [(-1, root)]
    for depth, caption in items:
        while stack[-1][0] >= depth:
            stack.pop()
        node = (caption, [])
        stack[-1][1].append(node)
        stack.append((depth, node[1]))
    return root


def linscp_menus(test, emulator):
    cmd = ([emulator] if emulator else []) + [test, 'menus', '/ini=nul', '-platform', 'offscreen']
    out = subprocess.run(cmd, capture_output=True, text=True, timeout=120).stdout
    items = []
    for line in out.splitlines():
        if line == 'OK' or not line.strip():
            continue
        depth = (len(line) - len(line.lstrip(' '))) // 2
        items.append((depth, clean(line.strip())))
    return tree(items)


failures = []


def compare(path, ours, theirs):
    """ours a subsequence of theirs, but for EXTRA and separators."""
    names = [RENAMES.get(c, c) for c, _ in theirs]
    position = 0
    for caption, children in ours:
        if caption == '----':
            continue
        if (path[-1] if path else '', caption) in EXTRA:
            continue
        try:
            found = names.index(caption, position)
        except ValueError:
            where = 'out of WinSCP\'s order' if caption in names else 'not in WinSCP\'s menu'
            failures.append(f"{' > '.join(path + [caption])}: {where}")
            continue
        position = found + 1
        if children and theirs[found][1]:
            compare(path + [caption], children, theirs[found][1])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--winscp', required=True)
    ap.add_argument('--emulator')
    ap.add_argument('test')
    a = ap.parse_args()
    ours = linscp_menus(a.test, a.emulator)
    if not ours:
        print('FAIL: no menus from the test program')
        return 1
    compare([], ours, winscp_menus(a.winscp))
    for f in failures:
        print('FAIL ' + f)
    if not failures:
        print('OK')
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
