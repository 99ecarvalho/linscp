#!/usr/bin/env python3
"""
The shell's translations (Qt Linguist .ts) hold exactly the texts its
sources have: none missing, none left over.

  check_ts_current.py --lupdate lupdate --ts linscp_pt_BR.ts ... --sources a.cpp b.h ...

Runs lupdate over the sources into a copy of each .ts and compares the
(context, source text) pairs with the committed file. When they differ, the
sources changed without `cmake --build <build> --target update_translations`.
Untranslated texts are reported, not failed: a translation may lag behind.

Each finished translation must also keep what the program relies on: the
same %1, %2 placeholders, at most one & mnemonic, the same HTML tags, and the same leading and trailing
spaces (problems()).
"""
import argparse, os, re, shutil, subprocess, sys, tempfile
import xml.etree.ElementTree as ET


def messages(path):
    """{(context, source): finished?}"""
    result = {}
    for context in ET.parse(path).getroot().iter('context'):
        name = context.findtext('name')
        for message in context.iter('message'):
            translation = message.find('translation')
            finished = (translation is not None) and (translation.get('type') not in ('unfinished', 'vanished', 'obsolete'))
            result[(name, message.findtext('source'))] = finished
    return result


def mnemonics(text):
    """The & mnemonics of a text (&& is a literal &, &lt; an entity)."""
    return len(re.findall(r'&(?!&)', re.sub(r'&(?:#?\w+;|&)', '', text)))


def edges(text):
    return re.match(r'\s*', text).group(0), re.search(r'\s*$', text).group(0)


def problems(source, translation):
    """Why a translation does not fit its English text, if it does not."""
    result = []
    if sorted(re.findall(r'%(?:\d+|n)', source)) != sorted(re.findall(r'%(?:\d+|n)', translation)):
        result.append('placeholders differ')
    # WinSCP's translators drop a mnemonic now and then; more than one is
    # a slip.
    if mnemonics(translation) > 1:
        result.append('more than one & mnemonic')
    tags = lambda text: sorted(t.lower() for t in re.findall(r'</?\s*([A-Za-z][A-Za-z0-9]*)', text))
    if tags(source) != tags(translation):
        result.append('HTML tags differ')
    if edges(source) != edges(translation):
        result.append('leading or trailing spaces differ')
    return result


def translations(path):
    """(context, source, translation) of each finished translation."""
    for context in ET.parse(path).getroot().iter('context'):
        name = context.findtext('name')
        for message in context.iter('message'):
            translation = message.find('translation')
            if (translation is not None) and (translation.get('type') is None):
                texts = [form.text or '' for form in translation.iter('numerusform')] or [translation.text or '']
                for text in texts:
                    yield name, message.findtext('source'), text


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--lupdate', required=True)
    ap.add_argument('--ts', nargs='+', required=True)
    ap.add_argument('--sources', nargs='+', required=True)
    a = ap.parse_args()
    failures = 0
    with tempfile.TemporaryDirectory() as tmp:
        for ts in a.ts:
            copy = os.path.join(tmp, os.path.basename(ts))
            shutil.copy(ts, copy)
            p = subprocess.run([a.lupdate, '-silent', '-no-obsolete', '-locations', 'none'] + a.sources + ['-ts', copy],
                               capture_output=True, text=True)
            if p.returncode != 0:
                print(p.stdout + p.stderr)
                return 1
            committed, current = messages(ts), messages(copy)
            missing = sorted(set(current) - set(committed))
            stale = sorted(set(committed) - set(current))
            name = os.path.basename(ts)
            for context, source in missing:
                print(f'FAIL {name}: not in the .ts: [{context}] {source!r}')
            for context, source in stale:
                print(f'FAIL {name}: no longer in the sources: [{context}] {source!r}')
            failures += len(missing) + len(stale)
            for context, source, translation in translations(ts):
                for problem in problems(source, translation):
                    print(f'FAIL {name}: {problem}: [{context}] {source!r} -> {translation!r}')
                    failures += 1
            untranslated = sum(1 for done in committed.values() if not done)
            print(f'{name}: {len(committed)} texts, {untranslated} untranslated')
    if failures:
        print('A .ts is behind the sources, or a translation does not fit its text.')
        print('The shell\'s texts changed: cmake --build <build> --target update_translations')
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
