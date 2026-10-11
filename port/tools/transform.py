#!/usr/bin/env python3
"""
Transform pristine WinSCP source into a clang-compilable tree.

Reads 3rd-party/winscp (a pinned, unmodified submodule) and writes a transformed
copy. The repo never contains a modified copy of WinSCP source; this script plus
the patch series in port/patches/ is the whole delta.

Transformations are one-line-in/one-line-out. That is not an imposed constraint:
every __property declaration in core is already on a single line (verified: 0 of
591 span lines), so preserving them costs nothing and keeps `diff -u upstream gen`
reviewable with zero offsets.

Rules applied:
  __property <T> <Name> = { read = X [, write = Y] };   -> PROP_* macro call
  __property <T> <Name> = { read = X, index = I };      -> PROP_R_GI
  __property <T> <Name>[IT Index] = { ... };            -> PROP_IDX_* macro call

A read/write target is classified as a method if the enclosing file declares it
with parentheses, otherwise as a field. Fields are never followed by '('.

Usage:
  transform.py --src 3rd-party/winscp --out build/gen [--check] [--report FILE]
"""
import argparse, os, re, shutil, sys, json
from collections import Counter

PROP_RE = re.compile(
    r'(?P<indent>\s*)__property\s+(?P<decl>[^=;{}]+?)\s*(?P<idx>\[[^\]]*\])?\s*'
    r'=\s*\{(?P<body>[^}]*)\}\s*;(?P<tail>.*)$')

def split_type_name(decl):
    """'TVSFixedFileInfo *FixedApplicationInfo' -> ('TVSFixedFileInfo *', 'FixedApplicationInfo')"""
    m = re.match(r'^(?P<type>.*?)(?P<ptr>[\s*&]+)(?P<name>\w+)$', decl.strip())
    if not m:
        return None, None
    return (m.group('type') + m.group('ptr')).strip(), m.group('name')

def parse_body(body):
    """'read = GetX, write = SetY' -> {'read':'GetX','write':'SetY'}"""
    out = {}
    for part in re.split(r',(?![^(]*\))', body):   # don't split inside MACRO(a, b)
        if '=' not in part:
            continue
        k, v = part.split('=', 1)
        out[k.strip()] = v.strip()
    return out

# typedef RET __fastcall (__closure * NAME)(ARGS);   ->  using NAME = TClosure<RET(ARGS)>;
#
# Both spellings appear, with and without __fastcall, and the argument list is
# often on following lines. Line counts are preserved by splitting the result
# the same way the original was split: "TClosure<RET" closes the first line and
# the final ";" becomes ">;", because TClosure<RET(ARGS)> is exactly
# "TClosure<RET" + "(ARGS)" + ">".
# An inline closure *parameter*, as opposed to a typedef:
#   void __fastcall (__closure *f)(TLogLineType Type, const UnicodeString & Line)
# becomes
#   TClosure<void(TLogLineType Type, const UnicodeString & Line)> f
# Only two of these exist in core, both in SessionInfo, but they have to be
# handled: the declaration and the definition must agree or nothing links.
def code_only_lines(lines):
    """Each line with comments and string/character literals blanked out.

    For counting braces. A brace inside a comment once shifted a block's depth
    by one -- ScpFileSystem.cpp has `// ... in queue) }` inside a __finally --
    and put LINSCP_END_FINALLY on the wrong line. Block comments carry across
    lines, so this works on the whole list rather than line by line.
    """
    out = []
    in_block = False
    for line in lines:
        res = []
        i, n = 0, len(line)
        while i < n:
            c = line[i]
            if in_block:
                if line.startswith('*/', i):
                    in_block = False
                    i += 2
                else:
                    i += 1
                continue
            if line.startswith('//', i):
                break
            if line.startswith('/*', i):
                in_block = True
                i += 2
                continue
            if c in '"\'':
                quote = c
                i += 1
                while i < n and line[i] != quote:
                    i += 2 if line[i] == '\\' else 1
                i += 1
                res.append(' ')
                continue
            res.append(c)
            i += 1
        out.append(''.join(res))
    return out


def masked_lines(lines):
    """Each line with comments and string/character literals turned to spaces.

    Unlike code_only_lines, every column keeps its position, so a match in the
    mask is a match at the same place in the line.
    """
    out = []
    in_block = False
    for line in lines:
        res = list(line)
        i, n = 0, len(line)
        while i < n:
            if in_block:
                if line.startswith('*/', i):
                    in_block = False
                    res[i] = res[i + 1] = ' '
                    i += 2
                else:
                    res[i] = ' '
                    i += 1
                continue
            if line.startswith('//', i):
                for j in range(i, n):
                    res[j] = ' '
                break
            if line.startswith('/*', i):
                in_block = True
                res[i] = res[i + 1] = ' '
                i += 2
                continue
            c = line[i]
            if c in '"\'':
                quote = c
                j = i + 1
                while j < n and line[j] != quote:
                    j += 2 if line[j] == '\\' else 1
                for k in range(i, min(j + 1, n)):
                    res[k] = ' '
                i = j + 1
                continue
            i += 1
        out.append(''.join(res))
    return out


def load_long_keep():
    """{(file, identifier)} from lp64_long_keep.txt, next to this script."""
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'lp64_long_keep.txt')
    keep = set()
    for line in open(path):
        line = line.split('#', 1)[0].split()
        if line:
            keep.add((line[0], line[1]))
    return keep


def strip_comments(text):
    """Remove // and /* */ comments. Deliberately simple: it is used only to
    decide whether a keyword really appears as code, so over-removing inside a
    string literal would at worst cost a false negative on a construct that
    does not occur here."""
    text = re.sub(r'/\*.*?\*/', ' ', text, flags=re.S)
    return re.sub(r'//[^\n]*', ' ', text)


CLOSURE_PARAM_RE = re.compile(
    r'(?P<ret>[A-Za-z_][\w:*&<>\s]*?)\s*(?:__fastcall\s*)?'
    r'\(\s*__closure\s*\*\s*(?P<name>\w+)\s*\)\s*'
    r'\((?P<args>[^()]*)\)')

CLOSURE_RE = re.compile(
    r'^(?P<indent>\s*)typedef\s+(?P<ret>.+?)\s*'
    r'\(\s*__closure\s*\*\s*(?P<name>\w+)\s*\)'
    r'(?P<rest>.*)$')


class Ambiguous(Exception):
    def __init__(self, name, parens, nfield, nmethod):
        super().__init__(
            f"cannot classify '{name}' as field or accessor "
            f"(declared-with-parens={parens}, looks-like-field={nfield}, "
            f"looks-like-accessor={nmethod})")
        self.name = name


_IDENT_CHAR = re.compile(r'[A-Za-z0-9_]')


def closure_expression(text, start, ident):
    """(begin, end, replacement) for the closure whose method name starts at
    `start`, or None when the shape is not one Borland's binding takes."""
    end = start + len(ident)

    def skip_space_left(k):
        while k >= 0 and text[k] in ' \t':
            k -= 1
        return k

    k = skip_space_left(start - 1)
    obj, arrow, begin = None, False, start
    if k >= 0 and text[k] == '.':
        k -= 1
    elif k >= 1 and text[k - 1:k + 1] == '->':
        arrow = True
        k -= 2
    else:
        k = None
    if k is not None:
        k = skip_space_left(k)
        stop = k + 1
        while k >= 0 and _IDENT_CHAR.match(text[k]):
            k -= 1
        if k + 1 == stop:
            return None                 # a call, subscript or cast as the object
        obj = text[k + 1:stop]
        before = skip_space_left(k)
        if before >= 0 and (text[before] == '.' or
                            (before >= 1 and text[before - 1:before + 1] == '->')):
            return None                 # a longer chain
        begin = k + 1

    # Address-of, but not logical and.
    k = skip_space_left(begin - 1)
    if k >= 0 and text[k] == '&' and not (k >= 1 and text[k - 1] == '&'):
        begin = k

    if obj is None:
        replacement = f'CLOSURE({ident})'
    elif arrow:
        replacement = f'CLOSURE_OF({obj}, {ident})'
    else:
        replacement = f'CLOSURE_OF(&{obj}, {ident})'
    return begin, end, replacement


def inline_try_finally(code_line):
    """(try, body start, body end, __finally, closing brace) columns of a
    `try { ... } __finally { ... }` contained in one line, else None. The
    line is the code-only form, so braces in comments and literals do not
    count, and has the same columns as the original."""
    m = re.search(r'\btry\s*\{', code_line)
    if m is None:
        return None
    def close(start):
        depth = 0
        for k in range(start, len(code_line)):
            if code_line[k] == '{':
                depth += 1
            elif code_line[k] == '}':
                depth -= 1
                if depth == 0:
                    return k
        return None
    body_open = m.end() - 1
    body_close = close(body_open)
    if body_close is None:
        return None
    f = re.match(r'\s*__finally\s*\{', code_line[body_close + 1:])
    if f is None:
        return None
    finally_at = body_close + 1 + code_line[body_close + 1:].index('__finally')
    cleanup_open = body_close + f.end()
    cleanup_close = close(cleanup_open)
    if cleanup_close is None:
        return None
    return m.start(), body_open + 1, body_close, finally_at, cleanup_close


class Transformer:
    def __init__(self, bind_sites=True, src_root=None):
        self.long_keep = load_long_keep()
        # The pristine tree, so rules can consult the real file names.
        self.src_root = src_root
        self.stats = Counter()
        self.unhandled = []
        self.notes = []
        # Off only for bind_closures.py --discover, which must see every site
        # still unbound so that each one errors and the list is rebuilt whole.
        self.closure_sites = load_closure_sites() if bind_sites else {}

    def transform_file(self, path, text):
        lines = text.split('\n')
        self.lp64_long_types(path, lines)
        text = '\n'.join(lines)
        methods = set(re.findall(r'\b(\w+)\s*\(', text))
        self.transform_closures(path, lines)
        self.transform_try_finally(path, lines)
        self.bind_closure_sites(path, lines)
        if path.lower().endswith('.h'):
            self.inline_header_definitions(path, lines)
        self.normalise_include_separators(path, lines)
        self.fix_include_case(path, lines)
        out = []
        for lineno, line in enumerate(lines, 1):
            if '__property' not in line or '#define' in line:
                out.append(line); continue
            # CR is carried in <tail> on CRLF files, so line endings survive
            try:
                new = self.transform_line(path, lineno, line, methods)
            except Ambiguous as e:
                self.stats['ambiguous'] += 1
                self.unhandled.append((path, lineno, f'{e}  ||  {line.strip()[:70]}'))
                new = None
            out.append(new if new is not None else line)
        return '\n'.join(out)

    LONG_TYPE = re.compile(r'\b(?:(unsigned|signed)\s+)?long\b(?:\s+int\b)?')

    def lp64_long_types(self, path, lines):
        """In core, `long` becomes `LONG` and `unsigned long` `ULONG`.

        Upstream is written for Windows, where long is 32 bits -- Win64
        included -- and core relies on it: TSFTPPacket reads and writes
        sizeof(unsigned long) bytes per SFTP uint32, and sign-extends 32-bit
        times through `signed long`. On LP64 POSIX long is 64 bits. LONG and
        ULONG are <windows.h>'s typedefs of long and unsigned long, so under
        C++Builder and mingw nothing changes; the POSIX layer defines them as
        32 bits, which is what upstream means. `long long`, `long double`
        and literal suffixes are left alone, as are comments and strings.
        """
        path = path.replace('\\', '/')
        if not path.startswith('source/core/'):
            return
        keep = [ident for f, ident in self.long_keep if f == path]
        masks = masked_lines(lines)
        for i, (line, mask) in enumerate(zip(lines, masks)):
            if 'long' not in mask:
                continue
            if any(re.search(r'\b' + re.escape(k) + r'\b', mask) for k in keep):
                self.stats['lp64_long_kept'] += 1
                continue
            pieces = []
            last = 0
            for m in self.LONG_TYPE.finditer(mask):
                if (re.match(r'\s*\b(long|double)\b', mask[m.end():]) or
                        re.search(r'\blong\s*$', mask[:m.start()])):
                    continue
                pieces.append(line[last:m.start()])
                pieces.append('ULONG' if m.group(1) == 'unsigned' else 'LONG')
                last = m.end()
                self.stats['lp64_long'] += 1
            if pieces:
                lines[i] = ''.join(pieces) + line[last:]

    # A function defined on one line, at namespace scope, in a header.
    HEADER_DEFINITION = re.compile(
        r'^(?!\s*(inline|static|template|typedef|class|struct|enum|namespace|'
        r'extern|return|if|else|for|while|do|switch)\b)'
        r'[A-Za-z_][\w\s\*&:<>,]*?\b\w+\s*\([^;{}]*\)\s*(const\s*)?\{.*\}\s*$')

    def inline_header_definitions(self, path, lines):
        """Give a header's namespace-scope function definitions `inline`.

        Common.h defines IsWideChar without it, so every unit including the
        header emits its own copy -- one definition per unit, which C++Builder's
        linker tolerates and the C++ standard does not. `inline` is what makes
        it legal; nothing else changes, and members (already implicitly
        inline) are out of scope because only depth zero is considered.
        """
        code = code_only_lines(lines)
        depth = 0
        for i, line in enumerate(lines):
            if depth == 0 and self.HEADER_DEFINITION.match(code[i].rstrip('\r')):
                indent = len(line) - len(line.lstrip())
                lines[i] = line[:indent] + 'inline ' + line[indent:]
                self.stats['header_inline'] += 1
            depth += code[i].count('{') - code[i].count('}')

    INCLUDE_WITH_BACKSLASH = re.compile(r'^(\s*#\s*include\s*)([<"])([^>"]*\\[^>"]*)([>"])')

    def normalise_include_separators(self, path, lines):
        """`#include "ssh\\gss.h"` becomes `#include "ssh/gss.h"`.

        Only Windows resolves a backslash in an include; clang accepts it when
        targeting Windows and not otherwise. A forward slash works everywhere,
        Windows included, so this changes nothing there.
        """
        for i, line in enumerate(lines):
            m = self.INCLUDE_WITH_BACKSLASH.match(line)
            if m:
                lines[i] = (line[:m.start(3)] + m.group(3).replace('\\', '/') +
                            line[m.end(3):])
                self.stats['include_slash'] += 1

    INCLUDE_NAME = re.compile(r'^(\s*#\s*include\s*[<"])([^>"/]+)([>"])')

    def fix_include_case(self, path, lines):
        """Spell an include as the file is actually named, when the two differ
        only in case and the file is in the including file's own directory.

        Only a case-insensitive file system resolves `FileZillaTools.h` to
        FilezillaTools.h, so a Linux host fails even cross-compiling for
        Windows. Same directory only, because that is what such a file system
        finds first: putty/windows/gss.c includes `security.h`, the Windows SDK
        header, which a match anywhere would map onto core's Security.h.
        """
        if self.src_root is None:
            return
        here = os.path.dirname(os.path.join(self.src_root, path))
        try:
            names = os.listdir(here)
        except OSError:
            return
        exact = set(names)
        folded = {n.lower(): n for n in names}
        for i, line in enumerate(lines):
            m = self.INCLUDE_NAME.match(line)
            if not m:
                continue
            spelled = m.group(2)
            real = folded.get(spelled.lower())
            if real and spelled not in exact and real != spelled:
                lines[i] = line[:m.start(2)] + real + line[m.end(2):]
                self.stats['include_case'] += 1

    def bind_closure_sites(self, path, lines):
        """Rewrite Borland's implicit closures as explicit bindings.

        Each recorded site is the column of a method name. Borland accepts it
        in four shapes, recognised from the text around it:

            Name     &Name      ->  CLOSURE(Name)
            obj.Name &obj.Name  ->  CLOSURE_OF(&obj, Name)
            p->Name  &p->Name   ->  CLOSURE_OF(p, Name)

        Anything else -- a longer chain, a call or subscript as the object --
        fails the run rather than being guessed at. Lines are preserved.
        """
        sites = self.closure_sites.get(path.replace(os.sep, '/'))
        if not sites:
            return
        # Right to left within each line, so rewriting one site does not shift
        # the recorded column of another on the same line.
        for lineno, col, ident in sorted(sites, key=lambda s: (-s[0], -s[1])):
            if lineno < 1 or lineno > len(lines):
                self.stats['closure_site_out_of_range'] += 1
                self.unhandled.append((path, lineno, f'line out of range for {ident}'))
                continue
            text = lines[lineno - 1]
            start = col - 1
            if text[start:start + len(ident)] != ident:
                # An upstream change moved this site. Fail rather than wrap the
                # wrong token: bind_closures.py --discover regenerates the list.
                self.stats['closure_site_stale'] += 1
                self.unhandled.append(
                    (path, lineno,
                     f'site no longer holds {ident!r}: {text.strip()[:60]}'))
                continue
            bound = closure_expression(text, start, ident)
            if bound is None:
                self.stats['closure_site_unparsed'] += 1
                self.unhandled.append(
                    (path, lineno,
                     f'closure shape not recognised for {ident!r}: {text.strip()[:60]}'))
                continue
            begin, end, replacement = bound
            lines[lineno - 1] = text[:begin] + replacement + text[end:]
            self.stats['closure_bound'] += 1

    def transform_try_finally(self, path, lines):
        """Rewrite Borland try/__finally into the dual-target macro form.

        Three lines change per block -- the `try`, the `__finally`, and the
        closing brace -- so the line count is preserved and the diff against
        upstream stays reviewable. See port/shim/vcl_tryfinally.h for what the
        macros expand to.

        Done in two passes. Brace depth is measured against the original lines
        and every edit applied afterwards, because LINSCP_END_FINALLY replaces a
        `}` with text containing no brace: editing as we went miscounted the
        depth for any enclosing block, and core nests these.
        """
        original = list(lines)
        code = code_only_lines(original)
        blocks = []

        for i, line in enumerate(original):
            if line.strip() != '__finally':
                continue

            # Walk back to the matching `try`. The line immediately before is
            # the `}` closing the guarded body, so depth returns to zero
            # exactly at the `try`.
            try_at = None
            depth = 0
            for j in range(i - 1, max(-1, i - 2000), -1):
                depth += code[j].count('}') - code[j].count('{')
                if depth == 0 and original[j].strip() == 'try':
                    try_at = j
                    break
            if try_at is None:
                self.stats['tryfinally_no_try'] += 1
                self.unhandled.append(
                    (path, i + 1, 'no matching `try` for this __finally'))
                continue

            # A `return` in the guarded body would return from the lambda
            # rather than the enclosing function. Refuse rather than silently
            # change control flow; these few belong in the patch series.
            # Walk forward to the brace closing the cleanup block.
            end_at = None
            depth = 0
            for j in range(i + 1, min(len(original), i + 2000)):
                depth += code[j].count('{') - code[j].count('}')
                if depth == 0 and original[j].strip() == '}':
                    end_at = j
                    break
            if end_at is None:
                self.stats['tryfinally_no_end'] += 1
                self.unhandled.append(
                    (path, i + 1, 'no closing brace for this __finally block'))
                continue

            # A `return` in the guarded body returns from the lambda, not the
            # enclosing function. Comments are stripped first, because two of
            # the three initial hits were the word in a comment.
            #
            # One case is still safe: a bare `return;` where the block is the
            # last statement of the function. Returning from the lambda then
            # runs the cleanup via the guard and falls off the end of the
            # function, which is what Borland would do. That is exactly the one
            # real occurrence in core, so the condition is checked rather than
            # assumed -- a value-returning `return`, or any code after the
            # block, still refuses.
            body = strip_comments('\n'.join(original[try_at:i]))
            if re.search(r'\breturn\b', body):
                bare_only = not re.search(r'\breturn\s+[^;\s]', body)
                after = end_at + 1
                while after < len(original) and not original[after].strip():
                    after += 1
                # rstrip the CR too: the core is CRLF and the transform
                # preserves line endings, so a raw comparison never matches.
                closes_function = (after < len(original) and
                                   original[after].rstrip('\r') == '}')
                if not (bare_only and closes_function):
                    self.stats['tryfinally_has_return'] += 1
                    self.unhandled.append(
                        (path, try_at + 1,
                         'guarded body contains `return`; cannot wrap in a '
                         'lambda'))
                    continue
                self.notes.append(
                    (path, try_at + 1,
                     'bare `return` in the guarded body, but the block ends '
                     'the function, so the lambda form is equivalent'))

            blocks.append((try_at, i, end_at))

        for try_at, finally_at, end_at in blocks:
            lines[try_at] = lines[try_at].replace('try', 'LINSCP_TRY', 1)
            lines[finally_at] = lines[finally_at].replace(
                '__finally', 'LINSCP_FINALLY', 1)
            lines[end_at] = lines[end_at].replace('}', 'LINSCP_END_FINALLY', 1)
            self.stats['tryfinally'] += 1

        # The same construct written on one line, as in Configuration.cpp's
        # BLOCK macro. Missed, it compiled as clang's SEH __try/__finally on
        # Windows -- not the dual-target form -- and not at all on Linux.
        code = code_only_lines(lines)
        for i, line in enumerate(lines):
            span = inline_try_finally(code[i])
            if span is None:
                continue
            try_at, body_start, body_end, finally_at, end_at = span
            if re.search(r'\breturn\b', code[i][body_start:body_end]):
                self.stats['tryfinally_has_return'] += 1
                self.unhandled.append(
                    (path, i + 1, 'guarded body contains `return`; cannot wrap in a lambda'))
                continue
            lines[i] = (line[:try_at] + 'LINSCP_TRY' + line[try_at + 3:finally_at] +
                        'LINSCP_FINALLY' + line[finally_at + 9:end_at] +
                        'LINSCP_END_FINALLY' + line[end_at + 1:])
            self.stats['tryfinally_inline'] += 1

    def transform_closures(self, path, lines):
        """Rewrite __closure typedefs in place, preserving the line count."""
        for i, line in enumerate(lines):
            if '__closure' not in line:
                continue
            m = CLOSURE_RE.match(line)
            if not m:
                # Not a typedef: try the inline-parameter form.
                pm = CLOSURE_PARAM_RE.search(line)
                if pm:
                    ret = pm.group('ret').replace('__fastcall', '').strip()
                    replacement = (f"TClosure<{ret}({pm.group('args')})> "
                                   f"{pm.group('name')}")
                    lines[i] = (line[:pm.start()] + replacement +
                                line[pm.end():])
                    self.stats['closure_param'] += 1
                    continue
                self.stats['closure_unmatched'] += 1
                self.unhandled.append((path, i + 1, line.strip()[:90]))
                continue
            ret = m.group('ret').replace('__fastcall', '').strip()
            head = f"{m.group('indent')}using {m.group('name')} = TClosure<{ret}"
            rest = m.group('rest')
            if ';' in rest:
                # Entirely on this line: close the template here.
                lines[i] = head + rest.rstrip()[:-1] + '>;'
                self.stats['closure_typedef'] += 1
                continue
            # Argument list continues on following lines; close at the ';'.
            end = None
            for j in range(i + 1, min(i + 8, len(lines))):
                if ';' in lines[j]:
                    end = j
                    break
            if end is None:
                self.stats['closure_unterminated'] += 1
                self.unhandled.append((path, i + 1, line.strip()[:90]))
                continue
            lines[i] = head + rest
            k = lines[end].rfind(';')
            lines[end] = lines[end][:k] + '>;' + lines[end][k + 1:]
            self.stats['closure_typedef'] += 1

    def transform_line(self, path, lineno, line, methods):
        m = PROP_RE.match(line)
        if not m:
            self.stats['unmatched'] += 1
            self.unhandled.append((path, lineno, line.strip()))
            return None
        typ, name = split_type_name(m.group('decl'))
        if not name:
            self.stats['unsplittable'] += 1
            self.unhandled.append((path, lineno, line.strip()))
            return None

        b = parse_body(m.group('body'))
        r, w, i = b.get('read'), b.get('write'), b.get('index')
        indexed = bool(m.group('idx'))
        ind, tail = m.group('indent'), m.group('tail')

        def kind(x, write=False):
            """Classify a read/write target as Field or Getter/Setter.

            WinSCP's naming convention is the primary signal and is airtight:
            fields are F<Upper>, accessors are Get*/Set*/Is*/Has*/Do*. A method is
            never named F<Upper>.

            Whether the file declares the name with parentheses is only a
            fallback, because it false-positives on constructor initialiser lists
            (`FSessionData(ASessionData)` looks like a call). An accessor the file
            does not declare is recorded as a note, not an error: it may be
            inherited from a base class, and if it genuinely does not exist the
            compiler says so clearly.

            Anything matching no convention and never declared with parentheses
            raises, rather than being guessed into a wrong-arity macro call.
            """
            by_parens = x in methods
            looks_field = bool(re.match(r'^F[A-Z_]', x))
            looks_accessor = bool(re.match(r'^(Get|Set|Is|Has|Do)[A-Z_]', x))
            if looks_accessor:
                if not by_parens:
                    self.notes.append((path, lineno, f"accessor '{x}' not declared in this file"))
                return 'S' if write else 'G'
            if looks_field:
                return 'F'
            if by_parens:
                return 'S' if write else 'G'
            raise Ambiguous(x, by_parens, looks_field, looks_accessor)

        if indexed:
            it = re.match(r'\[\s*([\w:<>\s*&]+?)\s+\w+\s*\]', m.group('idx'))
            itype = it.group(1).strip() if it else 'int'
            if r and w:
                call = f'PROP_IDX_RW_GS({typ}, {name}, {itype}, {r}, {w})'
                self.stats['idx_rw'] += 1
            elif r:
                call = f'PROP_IDX_R_G({typ}, {name}, {itype}, {r})'
                self.stats['idx_r'] += 1
            else:
                self.stats['unhandled_idx'] += 1
                self.unhandled.append((path, lineno, line.strip())); return None
        elif i is not None and r:
            call = f'PROP_R_GI({typ}, {name}, {r}, {i})'
            self.stats['r_gi'] += 1
        elif r and w:
            kr, kw = kind(r), kind(w, True)
            if kr == 'F' and kw == 'F':
                if r == w:
                    call = f'PROP_RW_FF({typ}, {name}, {r})'
                    self.stats['rw_FF'] += 1
                else:
                    call = f'PROP_RW_F2({typ}, {name}, {r}, {w})'
                    self.stats['rw_F2'] += 1
            else:
                call = f'PROP_RW_{kr}{kw}({typ}, {name}, {r}, {w})'
                self.stats[f'rw_{kr}{kw}'] += 1
        elif r:
            call = f'PROP_R_{kind(r)}({typ}, {name}, {r})'
            self.stats[f'r_{kind(r)}'] += 1
        elif w:
            call = f'PROP_W_{kind(w, True)}({typ}, {name}, {w})'
            self.stats[f'w_{kind(w, True)}'] += 1
        else:
            self.stats['empty_body'] += 1
            self.unhandled.append((path, lineno, line.strip())); return None

        self.stats['transformed'] += 1
        return f'{ind}{call};{tail}'

EXTS = {'.h', '.hpp', '.cpp', '.c', '.inc', '.inl'}

# Call sites where core names a bare member function where a closure is
# expected. Discovered by the compiler (port/tools/bind_closures.py), recorded,
# and applied here so the result is reproducible rather than depending on a
# compile. See that script for why this cannot be inferred by regex.
# Relative to this script, not the working directory: CMake runs the transform
# from the build tree, where a relative path found nothing and every binding
# was silently skipped.
CLOSURE_SITES = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                             '..', 'patches', 'closure_sites.json')


def load_closure_sites():
    """{relative path: [(line, column, identifier), ...]}"""
    if not os.path.exists(CLOSURE_SITES):
        # Part of the reviewed patch series, not optional: without it core does
        # not compile, and a quietly empty list is how that went unnoticed.
        sys.exit(f'missing {CLOSURE_SITES}')
    import json
    by_file = {}
    for site in json.load(open(CLOSURE_SITES))['sites']:
        by_file.setdefault(site['file'], []).append(
            (site['line'], site['column'], site['identifier']))
    return by_file

def _load_patchset():
    # By path: under python -I the script's directory is not on sys.path.
    import importlib.util
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'patchset.py')
    spec = importlib.util.spec_from_file_location('patchset', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--src', default='3rd-party/winscp')
    ap.add_argument('--out', default='build/gen')
    # PuTTY, FileZilla, WinSCP's MFC subset, neon and libs3 are compiled from
    # the generated tree too: they need the include rule or the patch series.
    ap.add_argument('--subdirs', nargs='*',
                    default=['source/core', 'source/putty', 'source/filezilla', 'libs/mfc',
                             'libs/neon', 'libs/libs3'])
    ap.add_argument('--check', action='store_true',
                    help='report only; do not write the output tree')
    ap.add_argument('--report')
    ap.add_argument('--list-outputs', action='store_true',
                    help='print the generated files, relative to --out, and exit')
    ap.add_argument('--no-closure-sites', action='store_true',
                    help='skip binding recorded closure sites (for discovery)')
    a = ap.parse_args()

    if a.list_outputs:
        # For the build: CMake declares these as the transform's byproducts.
        for sub_ in a.subdirs:
            for dirpath, _, names in os.walk(os.path.join(a.src, sub_)):
                for n in sorted(names):
                    if os.path.splitext(n)[1].lower() in EXTS:
                        print(os.path.relpath(os.path.join(dirpath, n), a.src)
                              .replace(os.sep, '/'))
        return 0

    t = Transformer(bind_sites=not a.no_closure_sites, src_root=a.src)
    patchset = _load_patchset()
    series = patchset.load_series()
    used = set()
    nfiles = 0
    produced = set()
    for sub in a.subdirs:
        root = os.path.join(a.src, sub)
        if not os.path.isdir(root):
            sys.exit(f'missing source dir: {root}')
        for dirpath, _, names in os.walk(root):
            for n in names:
                src = os.path.join(dirpath, n)
                rel = os.path.relpath(src, a.src)
                dst = os.path.join(a.out, rel)
                if os.path.splitext(n)[1].lower() not in EXTS:
                    continue
                text = open(src, encoding='utf-8', errors='surrogateescape', newline='').read()
                try:
                    patched = patchset.apply(rel, text, series, used)
                except patchset.PatchError as e:
                    sys.exit(str(e))
                new = t.transform_file(rel, patched)
                nlines_ok = len(text.split('\n')) == len(new.split('\n'))
                if not nlines_ok:
                    sys.exit(f'LINE COUNT CHANGED in {rel} — transform is not 1:1')
                nfiles += 1
                produced.add(os.path.normpath(dst))
                if not a.check:
                    # Unchanged output keeps its mtime, so the build recompiles
                    # only what a rule or patch actually changed.
                    if os.path.exists(dst) and open(
                            dst, encoding='utf-8', errors='surrogateescape',
                            newline='').read() == new:
                        continue
                    os.makedirs(os.path.dirname(dst), exist_ok=True)
                    open(dst, 'w', encoding='utf-8', errors='surrogateescape', newline='').write(new)

    # Outputs whose source upstream deleted or renamed. Left in place they
    # still compile and link -- a stale KeyGen.cpp survived a pin bump this
    # way and was "rediscovered" by bind_closures.py.
    pruned = 0
    if not a.check:
        for sub in a.subdirs:
            for dirpath, _, names in os.walk(os.path.join(a.out, sub)):
                for n in names:
                    p = os.path.normpath(os.path.join(dirpath, n))
                    if os.path.splitext(n)[1].lower() in EXTS and p not in produced:
                        os.remove(p)
                        pruned += 1
    if pruned:
        t.stats['pruned'] = pruned

    # A hunk for a file in scope that never applied means the file is gone or
    # renamed upstream; the patch needs attention either way.
    in_scope = {h for h in patchset.all_hunks(series)
                if any(h[1].startswith(sub.rstrip('/') + '/') for sub in a.subdirs)}
    unused = sorted(in_scope - used)
    if unused:
        sys.exit('patch hunks that applied nowhere: ' +
                 ', '.join(f'{n} ({r} #{i + 1})' for n, r, i in unused))
    if series:
        t.stats['patch_hunks'] = len(used)

    print(f'files processed: {nfiles}')
    for k, v in sorted(t.stats.items()):
        print(f'  {k:16} {v}')
    if t.notes:
        print(f'\nnotes ({len(t.notes)}):')
        for p_, l, m in t.notes[:10]:
            print(f'  {p_}:{l}: {m}')
    if t.unhandled:
        print(f'\nUNHANDLED ({len(t.unhandled)}):')
        for p, l, s in t.unhandled[:20]:
            print(f'  {p}:{l}: {s[:100]}')
    if a.report:
        json.dump({'stats': dict(t.stats), 'unhandled': t.unhandled},
                  open(a.report, 'w'), indent=2)
    return 1 if t.unhandled else 0

if __name__ == '__main__':
    sys.exit(main())
