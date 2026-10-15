#!/usr/bin/env python3
"""
Import linscp's bundled icon sets into assets/icons/<set>/.

linscp cannot use WinSCP's own icons: they are licensed stock images
(WinSCP's license.txt, part B) that may not be reused in other software. So
the shell's commands take their icons from freely licensed sets instead,
one file per role (assets/icons/<set>/<role>.svg), the roles following
WinSCP's glyphs (forms/Glyphs.dfm) for the commands linscp has.

Each set comes from its npm package, unpacked (`npm pack <name>@<version>`,
then `tar xzf`), given here by directory:

  import_icons.py --lucide DIR --tabler DIR --fluent DIR [--fluent-license FILE]

Writes, per set, the roles' SVGs, its LICENSE, and SOURCES.md (the package,
its version, and which of its files each role is). Files with scripts or
external references are refused.
"""
import argparse, json, os, re, shutil, sys

# role: (lucide, tabler, fluent) candidates, the first that exists taken.
ROLES = {
    'back':             (['arrow-left'], ['arrow-left'], ['arrow_left']),
    'forward':          (['arrow-right'], ['arrow-right'], ['arrow_right']),
    'parent':           (['folder-up'], ['folder-up'], ['folder_arrow_up', 'arrow_up']),
    'root':             (['folder-root'], ['folder-root'], ['hard_drive']),
    'home':             (['house', 'home'], ['home'], ['home']),
    'refresh':          (['refresh-cw'], ['refresh'], ['arrow_clockwise']),
    'new-folder':       (['folder-plus'], ['folder-plus'], ['folder_add']),
    'new-file':         (['file-plus'], ['file-plus'], ['document_add']),
    'delete':           (['trash-2'], ['trash'], ['delete']),
    'rename':           (['pencil-line', 'pencil'], ['pencil'], ['rename']),
    'properties':       (['info'], ['info-circle'], ['info']),
    'edit':             (['square-pen', 'pen-square'], ['edit'], ['edit']),
    'open':             (['external-link'], ['external-link'], ['open']),
    'copy':             (['copy'], ['copy'], ['copy']),
    'download':         (['download'], ['download'], ['arrow_download']),
    'upload':           (['upload'], ['upload'], ['arrow_upload']),
    'move':             (['folder-input'], ['file-export'], ['arrow_export']),
    'duplicate':        (['copy-plus'], ['copy-plus'], ['document_copy']),
    'link':             (['link'], ['link'], ['link']),
    'find':             (['search'], ['search'], ['search']),
    'filter':           (['funnel', 'filter'], ['filter'], ['filter']),
    'synchronize':      (['folder-sync'], ['refresh-dot', 'arrows-exchange'], ['arrow_sync']),
    'keep-up-to-date':  (['eye'], ['eye'], ['eye']),
    'compare':          (['git-compare'], ['git-compare'], ['branch_compare']),
    'sync-browse':      (['arrow-left-right'], ['arrows-left-right'], ['arrow_swap']),
    'console':          (['square-terminal'], ['terminal-2'], ['window_console']),
    'putty':            (['terminal'], ['terminal'], ['code']),
    'sites':            (['server'], ['server'], ['server']),
    'new-tab':          (['square-plus', 'plus-square'], ['square-plus'], ['tab_add']),
    'local-tab':        (['laptop'], ['device-laptop'], ['laptop']),
    'close-tab':        (['x'], ['x'], ['dismiss']),
    'duplicate-tab':    (['layers'], ['copy'], ['tabs']),
    'disconnect':       (['unplug'], ['plug-x'], ['plug_disconnected']),
    'reconnect':        (['plug'], ['plug'], ['plug_connected']),
    'save-site':        (['save'], ['device-floppy'], ['save']),
    'save-workspace':   (['save-all'], ['device-floppy'], ['save_multiple']),
    'workspace':        (['layout-grid'], ['layout-grid'], ['grid']),
    'server-info':      (['badge-info'], ['info-square'], ['info']),
    'generate-url':     (['code'], ['code'], ['code']),
    'preferences':      (['settings'], ['settings'], ['settings']),
    'about':            (['circle-help', 'help-circle'], ['help'], ['question_circle']),
    'quit':             (['log-out'], ['logout'], ['sign_out']),
    'select-files':     (['list-checks'], ['list-check'], ['select_all_on']),
    'unselect-files':   (['list-x'], ['square-off'], ['select_all_off']),
    'select-all':       (['check-check'], ['checks'], ['select_all_on']),
    'invert-selection': (['flip-horizontal-2', 'flip-horizontal'], ['arrows-exchange'], ['arrow_repeat_all']),
    'clear-selection':  (['square-dashed'], ['square-off'], ['select_all_off']),
    'clipboard-copy':   (['clipboard-copy'], ['clipboard-copy'], ['clipboard']),
    'paste':            (['clipboard-paste'], ['clipboard'], ['clipboard_paste']),
    'bookmark':         (['bookmark-plus'], ['bookmark-plus'], ['bookmark_add']),
    'open-directory':   (['folder-open'], ['folder-open'], ['folder_open']),
    'explore':          (['folder-search'], ['folder-search'], ['folder_search']),
    'queue':            (['list'], ['list'], ['list']),
    'pause':            (['pause'], ['player-pause'], ['pause']),
    'resume':           (['play'], ['player-play'], ['play']),
    'cancel':           (['circle-x', 'x-circle'], ['circle-x'], ['dismiss_circle']),
    'calculate-size':   (['calculator'], ['calculator'], ['calculator']),
    'clear-caches':     (['eraser'], ['eraser'], ['eraser']),
    'change-password':  (['key-round', 'key'], ['key'], ['key']),
    'public-key':       (['key-square', 'key'], ['key'], ['key_multiple']),
    'custom-commands':  (['command'], ['command'], ['apps']),
    'swap-panels':      (['arrow-left-right'], ['arrows-left-right'], ['arrow_swap']),
    'sort-ascending':   (['arrow-up-narrow-wide'], ['sort-ascending'], ['arrow_sort_up']),
    'sort-name':        (['arrow-down-a-z'], ['sort-ascending-letters'], ['text_sort_ascending']),
    'sort-size':        (['arrow-down-1-0'], ['sort-ascending-numbers'], ['number_symbol']),
    'sort-time':        (['clock'], ['clock'], ['clock']),
    'sort-rights':      (['lock'], ['lock'], ['lock_closed']),
    'sort-owner':       (['user'], ['user'], ['person']),
    'sort-group':       (['users'], ['users'], ['people']),
}

SETS = {
    'lucide': {'name': 'Lucide', 'package': 'lucide-static', 'license': 'ISC',
               'path': lambda base, n: os.path.join(base, 'icons', n + '.svg')},
    'tabler': {'name': 'Tabler Icons', 'package': '@tabler/icons', 'license': 'MIT',
               'path': lambda base, n: os.path.join(base, 'icons', 'outline', n + '.svg')},
    'fluent': {'name': 'Fluent UI System Icons', 'package': '@fluentui/svg-icons', 'license': 'MIT',
               'path': lambda base, n: os.path.join(base, 'icons', n + '_20_regular.svg')},
}
INDEX = {'lucide': 0, 'tabler': 1, 'fluent': 2}
UNSAFE = re.compile(r'<script|on\w+\s*=|xlink:href\s*=\s*"(?!#)|href\s*=\s*"(?!#)|<foreignObject', re.I)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--lucide', required=True)
    ap.add_argument('--tabler', required=True)
    ap.add_argument('--fluent', required=True)
    ap.add_argument('--fluent-license', help="the set's LICENSE (its npm package has none)")
    ap.add_argument('--out', default=os.path.join(os.path.dirname(__file__), '..', '..', 'assets', 'icons'))
    a = ap.parse_args()
    failures = 0
    for key, info in SETS.items():
        base = getattr(a, key)
        version = json.load(open(os.path.join(base, 'package.json')))['version']
        out = os.path.join(a.out, key)
        if os.path.isdir(out):
            shutil.rmtree(out)
        os.makedirs(out)
        rows = []
        for role, candidates in ROLES.items():
            source = next((n for n in candidates[INDEX[key]] if os.path.isfile(info['path'](base, n))), None)
            if source is None:
                print(f'FAIL {key}: no icon for {role} among {candidates[INDEX[key]]}')
                failures += 1
                continue
            text = open(info['path'](base, source), encoding='utf-8').read()
            if UNSAFE.search(text) or '<svg' not in text:
                print(f'FAIL {key}: {source} is not a plain SVG')
                failures += 1
                continue
            open(os.path.join(out, role + '.svg'), 'w', encoding='utf-8').write(text)
            rows.append(f'| {role} | `{os.path.relpath(info["path"](base, source), base)}` |')
        license = a.fluent_license if (key == 'fluent') else os.path.join(base, 'LICENSE')
        if not license or not os.path.isfile(license):
            print(f'FAIL {key}: no LICENSE')
            failures += 1
        else:
            shutil.copy(license, os.path.join(out, 'LICENSE'))
        with open(os.path.join(out, 'SOURCES.md'), 'w', encoding='utf-8') as f:
            f.write(f'# {info["name"]}\n\n'
                    f'From the npm package `{info["package"]}` {version}, {info["license"]} (LICENSE here),\n'
                    f'imported by `port/tools/import_icons.py`: each role (linscp\'s, after WinSCP\'s\n'
                    f'glyphs) is the file below, unchanged.\n\n| Role | File |\n|---|---|\n' + '\n'.join(rows) + '\n')
        print(f'{key}: {len(rows)} icons, {info["package"]} {version}')
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
