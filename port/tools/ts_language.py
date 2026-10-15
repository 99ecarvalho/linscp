#!/usr/bin/env python3
"""Add a language to the shell, and pass its remaining texts to a translator
and back (doc/qt/translations.md).

  ts_language.py new <locale> <LANG>     port/qt/translations/linscp_<locale>.ts,
                                         filled from WinSCP's <LANG>.ini
  ts_language.py export <ts> <json>      its untranslated texts, and its
                                         translated ones as a glossary
  ts_language.py import <ts> <json>      the translator's texts into it

`new` takes the shell's texts from an existing .ts (they are the same in
each), lets lupdate lay out the language's plural forms, and runs
winscp_ts_suggest.py over the copy. `export` writes
{"language", "todo": [{"context", "source"}], "glossary": [...]}: the
translator fills each todo item's "translation" and hands the file back.
A plural's "translation" is a list, one text per form ("plural_forms").
`import` takes each one that fits its English text (check_ts_current's
problems()) and reports the rest, which stay untranslated.
"""
import argparse, glob, html, json, os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TRANSLATIONS = os.path.join(HERE, '..', 'qt', 'translations')
sys.path.insert(0, HERE)
from check_ts_current import problems  # noqa: E402


def escape(text):
    return html.escape(text, quote=False).replace("'", '&apos;').replace('"', '&quot;')


def contexts(text):
    """(name, the context's text) of each context of a .ts file's text."""
    for part in re.split(r'(?=<context>)', text):
        name = re.search(r'<name>(.*?)</name>', part)
        yield (name.group(1) if name else ''), part


MESSAGE = re.compile(r'<source>([^<]*)</source>(\s*)<translation(?: type="unfinished")?>([^<]*)</translation>')
# A plural ("%n file(s)"): one form per plural of the language, as lupdate
# lays them out.
NUMERUS = re.compile(r'<source>([^<]*)</source>(\s*)<translation(?: type="unfinished")?>((?:\s*<numerusform>[^<]*</numerusform>)+)(\s*)</translation>')
FORM = re.compile(r'(\s*)<numerusform>([^<]*)</numerusform>')


def new(a):
    target = os.path.join(TRANSLATIONS, f'linscp_{a.locale}.ts')
    if os.path.exists(target):
        sys.exit(f'{target} is there already')
    template = sorted(glob.glob(os.path.join(TRANSLATIONS, 'linscp_*.ts')))[0]
    text = open(template, encoding='utf-8').read()
    text = re.sub(r'<TS version="2.1" language="[^"]*">', f'<TS version="2.1" language="{a.locale}">', text)
    text = MESSAGE.sub(lambda m: f'<source>{m.group(1)}</source>{m.group(2)}<translation type="unfinished"></translation>', text)
    text = NUMERUS.sub(lambda m: f'<source>{m.group(1)}</source>{m.group(2)}<translation type="unfinished"></translation>', text)
    open(target, 'w', encoding='utf-8').write(text)
    # lupdate gives each plural the language's number of forms.
    sources = glob.glob(os.path.join(HERE, '..', 'qt', '*.cpp')) + glob.glob(os.path.join(HERE, '..', 'qt', '*.h'))
    subprocess.run([a.lupdate, '-silent', '-no-obsolete', '-locations', 'none'] + sources + ['-ts', target], check=True)
    subprocess.run([sys.executable, os.path.join(HERE, 'winscp_ts_suggest.py'), target, a.lang, '--write'],
                   check=True, stdout=subprocess.DEVNULL)
    print(target)
    return 0


def export(a):
    text = open(a.ts, encoding='utf-8').read()
    todo, glossary = [], []
    for name, part in contexts(text):
        for m in MESSAGE.finditer(part):
            item = {'context': name, 'source': html.unescape(m.group(1))}
            if 'type="unfinished"' in m.group(0):
                todo.append(item)
            else:
                glossary.append(dict(item, translation=html.unescape(m.group(3))))
        for m in NUMERUS.finditer(part):
            # "translation": a list, one text per plural form.
            forms = [html.unescape(f.group(2)) for f in FORM.finditer(m.group(3))]
            item = {'context': name, 'source': html.unescape(m.group(1)), 'plural_forms': len(forms)}
            if 'type="unfinished"' in m.group(0):
                todo.append(item)
            else:
                glossary.append(dict(item, translation=forms))
    language = re.search(r'language="([^"]*)"', text).group(1)
    json.dump({'language': language, 'todo': todo, 'glossary': glossary}, open(a.json, 'w', encoding='utf-8'),
              ensure_ascii=False, indent=1)
    print(f'{len(todo)} to translate, {len(glossary)} in the glossary', file=sys.stderr)
    return 0


def import_(a):
    given = {}
    for item in json.load(open(a.json, encoding='utf-8'))['todo']:
        if item.get('translation'):
            given[(item['context'], item['source'])] = item['translation']
    text = open(a.ts, encoding='utf-8').read()
    taken = refused = 0

    def take(m, context):
        nonlocal taken, refused
        source = html.unescape(m.group(1))
        translation = given.get((context, source))
        if ('type="unfinished"' not in m.group(0)) or (translation is None):
            return m.group(0)
        wrong = problems(source, translation)
        if wrong:
            refused += 1
            print(f'  refused ({", ".join(wrong)}): [{context}] {source!r} -> {translation!r}')
            return m.group(0)
        taken += 1
        return f'<source>{m.group(1)}</source>{m.group(2)}<translation>{escape(translation)}</translation>'

    def take_plural(m, context):
        nonlocal taken, refused
        source = html.unescape(m.group(1))
        forms = given.get((context, source))
        slots = list(FORM.finditer(m.group(3)))
        if ('type="unfinished"' not in m.group(0)) or (forms is None):
            return m.group(0)
        wrong = ['a plural form per slot'] if (not isinstance(forms, list)) or (len(forms) != len(slots)) else \
            sorted({w for form in forms for w in problems(source, form)})
        if wrong:
            refused += 1
            print(f'  refused ({", ".join(wrong)}): [{context}] {source!r} -> {forms!r}')
            return m.group(0)
        taken += 1
        body = ''.join(f'{slot.group(1)}<numerusform>{escape(form)}</numerusform>' for slot, form in zip(slots, forms))
        return f'<source>{m.group(1)}</source>{m.group(2)}<translation>{body}{m.group(4)}</translation>'

    text = ''.join(NUMERUS.sub(lambda m: take_plural(m, name), MESSAGE.sub(lambda m: take(m, name), part))
                   for name, part in contexts(text))
    open(a.ts, 'w', encoding='utf-8').write(text)
    left = text.count('type="unfinished"')
    print(f'{taken} taken, {refused} refused, {left} untranslated', file=sys.stderr)
    return 0


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest='command', required=True)
    p = sub.add_parser('new')
    p.add_argument('locale')
    p.add_argument('lang')
    p.add_argument('--lupdate', default='/usr/lib/qt6/bin/lupdate')
    p.set_defaults(run=new)
    for name, run in (('export', export), ('import', import_)):
        p = sub.add_parser(name)
        p.add_argument('ts')
        p.add_argument('json')
        p.set_defaults(run=run)
    a = ap.parse_args()
    return a.run(a)


if __name__ == '__main__':
    sys.exit(main())
