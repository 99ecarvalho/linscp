#!/usr/bin/env python3
"""
Generate the manual pages doc/man/linscp.1 and doc/man/linscp-cli.1.

The scripting reference of linscp-cli (its switches, script commands, their
switches and the script options) comes from the website's data:

  linscp-website/src/data/cli.yaml            what each item does, in prose
  linscp-website/src/data/cli-structure.json  which items exist, their order,
                                              aliases and affecting options

Everything else (NAME, DESCRIPTION, FILES, ENVIRONMENT, the GUI's switches,
the few switches cli.yaml does not describe, and wording replacements) is
written in this script. The version comes from the top-level CMakeLists.txt.

The pages are committed, so a source tarball (which has no website) still
installs them. Nothing runs this at build time: run it by hand whenever
cli.yaml, cli-structure.json or the text below changes, and commit the result:

  python3 port/tools/gen_manpages.py            write doc/man/*.1
  python3 port/tools/gen_manpages.py --check    fail if doc/man/*.1 is stale

The date in .TH is the newest of the last commit of cli.yaml and
cli-structure.json (in the website submodule) and HAND_DATE below, so the
output is the same on every machine. Bump HAND_DATE when you change the text
written here. SOURCE_DATE_EPOCH, when set, overrides the date.
"""
import argparse
import datetime
import json
import os
import re
import subprocess
import sys

import yaml

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
WEBSITE = os.path.join(ROOT, 'linscp-website')
TEXTS = os.path.join(WEBSITE, 'src', 'data', 'cli.yaml')
STRUCT = os.path.join(WEBSITE, 'src', 'data', 'cli-structure.json')
OUT = os.path.join(ROOT, 'doc', 'man')

# The last change to the hand-written text in this script.
HAND_DATE = '2026-10-20'

ISSUES = 'https://github.com/99ecarvalho/linscp/issues'
MANUAL = 'https://linscp.org/manual/'
AUTHOR = ('Eduardo Correia, Apliant', 'ecorreia@apliant.com.br')

# ---------------------------------------------------------------------------
# Hand-written content.

# Replacements for cli.yaml texts that do not suit a manual page, keyed by
# (command or None, switch or field).
OVERRIDES = {
    (None, '/ini'): 'Read and save settings in this file instead of the default configuration '
                    'file (see FILES). With `nul`, start from defaults and save nothing.',
    (None, '/identity'): 'With `/copyid`, the key file whose public key gets installed. '
                         '`-i` is accepted as a short form.',
    ('open', '-filezilla'): 'Look `<site>` up in the site manager file of another file transfer '
                            'client, `~/.config/filezilla/sitemanager.xml`, instead of among '
                            "linscp's own sites.",
    ('help', 'body'): 'Without arguments, `help` lists every command with a one-line purpose. '
                      'With command names, it prints the built-in help for each.\n\n'
                      'Some examples in the built-in help use paths of another operating system. '
                      'On Linux, local paths use `/`, and a local target directory ends with '
                      '`/`. This manual page describes the Linux behaviour.',
}

# linscp-cli switches cli.yaml does not cover (they are not in the switch
# list of /help), as (syntax, text), inserted after the named switch.
CLI_EXTRA_SWITCHES = {
    '/xmlgroups': [
        ('/xmllogrequired', 'With `/xmllog`, fail when the XML log cannot be written, '
                            'instead of going on without it.'),
    ],
    '/fingerprintscan': [
        ('/keygen <key-file>', 'Convert `<key-file>`, an OpenSSH or other private key, to the PPK '
                               'format that linscp uses, or change an existing PPK file. '
                               'The switches below modify it. Then exit.'),
        ('/output=<file>', 'With `/keygen`, where to write the converted key.'),
        ('/changepassphrase', 'With `/keygen`, ask for a new passphrase for the key.'),
        ('/comment=<text>', 'With `/keygen`, set the comment stored in the key.'),
        ('/certificate=<file>', 'With `/keygen`, add this OpenSSH certificate to the key.'),
    ],
}

CLI_NAME = 'console and scripting client for SFTP, SCP, FTP, WebDAV and S3'
GUI_NAME = 'graphical file-transfer client for SFTP, SCP, FTP, WebDAV and S3'

CLI_SYNOPSIS = [
    '[<session>] [/script=<file>] [/command <command>...] [/parameter // <value>...] [<switch>...]',
    '/keygen <key-file> [/output=<file>] [/changepassphrase] [/comment=<text>] [/certificate=<file>]',
    '/copyid /identity=<key-file> <site>',
    '/fingerprintscan <site>',
    '/batchsettings <site-mask> <name>=<value>...',
    '/info',
    '/help',
]

CLI_DESCRIPTION = '''
`linscp-cli` transfers files between the local computer and remote servers
over SFTP, SCP, FTP (plain or with TLS), WebDAV and Amazon S3. It runs
commands typed at its `linscp>` prompt, read from a script file
(`/script`), given on the command line (`/command`) or piped to its standard
input. It shares sites, settings and host keys with `linscp`(1).

The optional `<session>` names a site saved in the Site Manager, a folder or
workspace of sites, or a session URL of the form
`<protocol>://[<user>[:<password>]@]<host>[:<port>][/<path>]`, where
`<protocol>` is one of `sftp`, `scp`, `ftp`, `ftps`, `ftpes`, `dav`, `davs` or
`s3`. When given, `linscp-cli` connects to it before it runs any command.

Switches start with a slash or a hyphen: `/script=job.txt` and
`-script=job.txt` are the same. Their names are not case-sensitive. A value
follows an equals sign; switches that take a list (`/command`,
`/parameter`, `/rawsettings`, `/rawconfig`) take the arguments that follow,
up to the next switch. Unknown switches are ignored.
'''

CLI_SCRIPTS = '''
A script has one command per line. Lines starting with `#` are comments.
Put arguments that contain spaces in double quotes; write a double quote
inside quotes as two.

`%NAME%` is replaced by the value of the environment variable `NAME`, and
`%1%`, `%2%` and so on by the values given after `/parameter //`.
`%TIMESTAMP#yyyy-mm-dd%` becomes the current local time in the given format
(`yyyy`, `mm`, `dd`, `hh`, `nn` for minutes, `ss`); an offset such as
`%TIMESTAMP-1D#yyyymmdd%` shifts it.

With `/script` or `/command`, `linscp-cli` starts with `option batch abort`
and `option confirm off`, so a script stops at the first error and overwrites
files without asking. Commands typed or piped in start with `option batch off`
and `option confirm on`.
'''

CLI_MASKS = '''
A file mask selects files by name. `*` matches any run of characters, `?`
one character, and `[a-z]` one character of a set or range. Masks ignore
letter case, except the wildcard in a local source of `put` or `lls`, which
the file system matches.

A size or time constraint can follow the name part: `>500K`, `<2G`,
`>2026-01-01`, `>7D` (modified within seven days), `<12H`; `>=` and `<=`
include the limit. A mask ending in `/` matches directories, and a path before
the name matches by location (`*/logs/*.txt`, `./build/`).

In `-filemask`, separate masks with a semicolon or a comma, and split
include from exclude masks with `|`: `-filemask="*.php; *.inc | test_*"` and
`-filemask="| .git/; *.tmp"`. A name given to a command such as `get` or `rm`
takes one simple mask in its last part.

When the target of `get`, `put`, `cp` or `mv` is not a plain name, it is an
operation mask that renames each file: with `*.bak`, `index.html` becomes
`index.bak`; with `old_*`, it becomes `old_index.html`.
'''

CLI_FILES = [
    ('~/.config/linscp/WinSCP.ini',
     'Sites, preferences and remembered host keys, shared with `linscp`(1). '
     'The directory is `$XDG_CONFIG_HOME/linscp` when that variable is set. '
     'It is created when needed.'),
    ('<program-directory>/WinSCP.ini',
     'When this file exists beside the program, it is used instead, which makes a portable copy. '
     'For an AppImage, the file goes beside the `.AppImage` file.'),
    ('~/.var/app/org.linscp.linscp/config/linscp/WinSCP.ini',
     'The configuration file of the Flatpak, which shares nothing with other installs and never '
     'uses a file beside its program.'),
]

COMMON_ENV = [
    ('XDG_CONFIG_HOME', 'The base directory of the configuration file; `~/.config` when unset.'),
    ('HOME', "The user's home directory."),
    ('TMPDIR', 'Temporary files go to a private per-user directory under it; `/tmp` when unset.'),
    ('APPIMAGE', 'Set by an AppImage to its own path. A `WinSCP.ini` beside that file makes '
                 'the configuration portable.'),
    ('SSH_AUTH_SOCK', 'The SSH agent used for public key authentication.'),
    ('WINSCP_PATH', 'Set by linscp, for scripts and the programs they run, to the directory '
                    'that holds the program (or its `dotnet` subdirectory, when the package has '
                    'the .NET scripting assembly).'),
]

CLI_EXIT = [
    ('0', 'Every command succeeded.'),
    ('1', 'A command failed and stopped the script, a prompt was answered with Abort, '
          'or the connection could not be opened.'),
]

CLI_EXAMPLES = [
    ('Upload a file to a saved site, unattended:',
     "linscp-cli /nointeractiveinput \\\n"
     "    /command 'open mysite' 'put report.pdf /srv/reports/' 'exit'"),
    ('Run a script with a session log, and pass it two values:',
     'linscp-cli /script=fetch.txt /log=fetch.log \\\n'
     '    /parameter // reports /home/alice/incoming'),
    ('A script that mirrors a directory to the server:',
     'open sftp://deploy@example.com -hostkey="ssh-ed25519 255 SHA256:..."\n'
     'option batch abort\n'
     'option confirm off\n'
     'synchronize remote -delete /home/alice/site /var/www/html\n'
     'exit'),
    ('Stream a remote file to another program:',
     "linscp-cli /stdout /command 'open mysite' 'get app.log -' 'exit' |\n"
     "    grep ERROR"),
    ('Convert an OpenSSH key for use with linscp:',
     'linscp-cli /keygen ~/.ssh/id_ed25519 /output=id_ed25519.ppk'),
]

CLI_SEE_ALSO = ['linscp(1)', 'ssh(1)', 'ssh-copy-id(1)', 'ssh-keygen(1)', 'sftp(1)']

GUI_SYNOPSIS = [
    '[<switch>...] [<session>]',
    '[<switch>...] <session> /upload <file>...',
    '[<switch>...] <session> /synchronize [<local-directory> [<remote-directory>]] [/defaults]',
    '[<switch>...] <session> /keepuptodate [<local-directory> [<remote-directory>]] [/defaults]',
    '[<switch>...] <session> /edit <remote-file>',
    '/help',
]

GUI_DESCRIPTION = '''
`linscp` is a two-panel file-transfer client that moves files between the local
computer and remote servers over SFTP, SCP, FTP (plain or with TLS), WebDAV
and Amazon S3. It edits remote files, synchronizes directories, keeps a
remote directory up to date with a local one, and stores sites in a Site
Manager. Its sites, settings and host keys are shared with `linscp-cli`(1),
the console and scripting client.

Without arguments, `linscp` opens the Site Manager, unless the preferences
turn that off. The optional `<session>`
names a site saved in the Site Manager, a folder or workspace of sites (each
opens in a tab), or a session URL of the form
`<protocol>://[<user>[:<password>]@]<host>[:<port>][/<path>]`, where
`<protocol>` is one of `sftp`, `scp`, `ftp`, `ftps`, `ftpes`, `dav`, `davs` or
`s3`. A URL that names a file downloads it.

A session started while `linscp` already runs opens in the running window,
or in a new one, as the preferences say. Hold Shift while starting to get the
other behaviour.

Switches start with a slash or a hyphen: `/upload` and `-upload` are the
same. Their names are not case-sensitive. A value follows an equals sign;
switches that take a list take the arguments that follow, up to the next
switch.
'''

GUI_SWITCHES = [
    ('/upload <file>...', 'Connect to `<session>` and upload the files to its remote directory, '
                          'showing the transfer dialog first.'),
    ('/uploadifany <file>...', 'As `/upload`, but start normally when no file is given.'),
    ('/synchronize [<local-directory> [<remote-directory>]]',
     'Connect to `<session>` and open the Synchronize dialog for the two directories.'),
    ('/keepuptodate [<local-directory> [<remote-directory>]]',
     'Connect to `<session>` and open the Keep Remote Directory up to Date dialog for the two '
     'directories.'),
    ('/defaults', 'With `/upload`, `/synchronize` or `/keepuptodate`, start the operation with '
                  'its default settings, without asking.'),
    ('/edit <remote-file>', 'Connect to `<session>` and open `<remote-file>` in the editor.'),
    ('/browse', 'With a session URL that names a file, select that file in the remote panel '
                'instead of downloading it. `/explore` is a synonym.'),
    ('/refresh [<remote-directory>]', 'Ask the running instance to reload the remote directory '
                                      'of `<session>`, then exit.'),
    ('/newinstance', 'Open `<session>` in a new window even when `linscp` already runs.'),
    ('/workspace', 'When a site and a workspace share the name given as `<session>`, open the '
                   'workspace.'),
    ('/privatekey=<file>', 'SSH private key to authenticate with, for the session given and for '
                           'sites that have none.'),
    ('/hostkey=<fingerprint>', "Fingerprint the server's SSH host key must have."),
    ('/username=<user>', 'User name to log in with, used instead of one in the site or URL.'),
    ('/password=<password>', 'Password to log in with. Other users of the machine may see it '
                             'in the process list.'),
    ('/passwordsfromfiles', 'Treat the values of the password and passphrase switches as names '
                            'of files that hold the secret.'),
    ('/clientcert=<file>', 'TLS client certificate file.'),
    ('/certificate=<fingerprint>', "Fingerprint the server's TLS certificate must have."),
    ('/passphrase=<passphrase>', 'Passphrase that unlocks an encrypted private key or client '
                                 'certificate.'),
    ('/passive[=on|off]', 'FTP passive mode (the default) or active mode.'),
    ('/implicit', 'Implicit TLS for FTP.'),
    ('/explicit', 'Explicit TLS for FTP.'),
    ('/timeout=<seconds>', 'How many seconds to wait for a server reply.'),
    ('/rawsettings <name>=<value>...', 'Set any site setting by its raw name, as stored in the '
                                       'configuration file.'),
    ('/rawtransfersettings <name>=<value>...', 'Set the default transfer settings of this run '
                                               'by their raw names.'),
    ('/rawconfig <name>=<value>...', 'Set any application setting by its raw name.'),
    ('/ini=<file>', 'Read and save settings in this file instead of the default configuration '
                    'file (see FILES). With `nul`, start from defaults and save nothing.'),
    ('/log=<file>', 'Write a session log to this file.'),
    ('/loglevel=<level>', 'How much detail the log holds, from `0` to `2`. A trailing `*` also '
                          'records passwords.'),
    ('/logsize=[<count>*]<size>', 'Start a new log file when the current one reaches the size, '
                                  'keeping at most the given number of old files.'),
    ('/xmllog=<file>', 'Write an XML log of each operation to this file.'),
    ('/unsafe', 'Mark the command line as coming from an untrusted source, such as a link '
                'opened in a web browser. `/ini`, `/rawconfig`, `/xmllog` and `/defaults` are then '
                'ignored.'),
    ('--lang=<code>', 'Use this interface language, such as `de` or `pt_BR`, instead of the '
                      'one chosen in the preferences.'),
    ('/help', 'Show a usage summary of `linscp-cli`(1) and exit. `/info`, `/keygen`, `/copyid`, '
              '`/fingerprintscan` and `/batchsettings` also run `linscp-cli`(1) with the same '
              'command line.'),
]

GUI_EXIT = [
    ('0', 'The program ended normally.'),
    ('1', 'The program could not start, or a delegated console operation failed.'),
]

GUI_EXAMPLES = [
    ('Open a saved site:', 'linscp "Production web"'),
    ('Connect to a server given by URL:', 'linscp sftp://alice@example.com/var/www/'),
    ('Upload two files to a site:', 'linscp mysite /upload report.pdf notes.txt'),
    ('Keep a remote directory up to date, starting at once:',
     'linscp mysite /keepuptodate /home/alice/site /var/www/html /defaults'),
    ('Use a separate configuration file:', 'linscp /ini=/home/alice/work/linscp.ini'),
]

GUI_SEE_ALSO = ['linscp-cli(1)', 'ssh(1)', 'sftp(1)']

# ---------------------------------------------------------------------------
# Rendering.


def roff_escape(text):
    return text.replace('\\', '\\e')


def code_dashes(text):
    return roff_escape(text).replace('-', '\\-')


def syntax(text, words_italic=('switches',)):
    """A syntax string: <placeholder> italic, [ ] | ... roman, the rest bold."""
    segments = []  # [font, text], font 'B', 'I' or 'R'
    for tok in re.findall(r'<[A-Za-z][^<> ]*>|\.\.\.|[\[\]|{} ]|[^<\[\]|{} .]+|.', text):
        if tok.startswith('<') and tok.endswith('>'):
            font, tok = 'I', tok[1:-1]
        elif tok in ('[', ']', '|', '{', '}', ' ', '...'):
            font = 'R'
        elif tok in words_italic:
            font = 'I'
        else:
            font = 'B'
        if segments and segments[-1][0] == font:
            segments[-1][1] += tok
        else:
            segments.append([font, tok])
    return ''.join(code_dashes(t) if f == 'R' else '\\f%s%s\\fR' % (f, code_dashes(t)) for f, t in segments)


def code_span(text):
    # Literal output such as <DIR> is not a placeholder.
    if re.fullmatch(r'<[A-Z]+>', text):
        return '\\fB' + roff_escape(text) + '\\fR'
    return syntax(text, words_italic=())


def inline(text):
    """Markdown inline markup to roff."""
    text = re.sub(r'<kbd>([^<]*)</kbd>', r'\1', text)
    parts = re.split(r'(`[^`]*`)', text)
    out = []
    for p in parts:
        if p.startswith('`') and p.endswith('`') and len(p) >= 2:
            out.append(code_span(p[1:-1]))
        else:
            p = re.sub(r'\[([^\]]*)\]\([^)]*\)', r'\1', p)
            p = roff_escape(p)
            p = re.sub(r'\*\*([^*]+)\*\*', r'\\fB\1\\fR', p)
            p = p.replace('\u2014', '\\(em').replace('\u2013', '\\(en')
            out.append(p)
    s = ''.join(out)
    # Links whose text was a code span: [\fBx\fR](...) left behind.
    s = re.sub(r'\[(\\fB[^\]]*?\\fR)\]\([^)]*\)', r'\1', s)
    return s


def sentences(text):
    """One sentence per output line, as man-pages(7) asks."""
    text = ' '.join(text.split())
    lines = re.split(r'(?<=[.!?:])\s+(?=[A-Z`(\[*%])', text)
    # "See [a page](../../x/)." points into the website; SEE ALSO has the manual.
    lines = [l for l in lines if not re.fullmatch(r'See \[[^\]]*\]\(\.\./\.\./[^)]*\)\.', l)]
    return [safe_line(inline(line)) for line in lines]


def safe_line(line):
    line = line.rstrip()
    if line.startswith(('.', "'")):
        line = '\\&' + line
    return line


def blocks(md):
    """Paragraphs and bullet lists of a Markdown text."""
    result = []
    for chunk in re.split(r'\n\s*\n', (md or '').strip()):
        lines = chunk.split('\n')
        if lines[0].lstrip().startswith('- '):
            items = []
            for line in lines:
                if line.lstrip().startswith('- '):
                    items.append(line.lstrip()[2:])
                else:
                    items[-1] += ' ' + line.strip()
            result.append(('list', items))
        else:
            result.append(('para', ' '.join(l.strip() for l in lines)))
    return result


def render(md, nested=False):
    """Markdown blocks as roff. Nested: inside a .TP body, where paragraphs
    stay indented."""
    out = []
    first = True
    for kind, value in blocks(md):
        if kind == 'para':
            if not (nested and first):
                out.append('.IP' if nested else '.PP')
            out += sentences(value)
        else:
            if nested:
                out.append('.RS')
            for item in value:
                out.append('.IP \\(bu 2')
                out += sentences(item)
            if nested:
                out.append('.RE')
        first = False
    return out


def example(code):
    out = ['.PP', '.RS 4', '.EX']
    out += [safe_line(code_dashes(line)) for line in code.rstrip('\n').split('\n')]
    out += ['.EE', '.RE']
    return out


def see_also(refs):
    items = []
    for r in refs:
        name, sec = re.fullmatch(r'(.+)\((\d)\)', r).groups()
        items.append('.BR %s (%s)' % (name.replace('-', '\\-'), sec))
    return [i + (',' if n < len(items) - 1 else '') for n, i in enumerate(items)]


def header(title, name, summary, date, version, sources):
    return [
        '.\\" Manual page for %s.' % name,
        '.\\" Generated by port/tools/gen_manpages.py from %s.' % ', '.join(sources),
        '.\\" Do not edit this file: change the sources or the generator, then run',
        '.\\"   python3 port/tools/gen_manpages.py',
        '.\\" and commit the result.',
        '.\\" SPDX-License-Identifier: GPL-3.0-or-later',
        '.TH %s 1 %s "linscp %s" "User Commands"' % (title, date, version),
        '.\\" Literals and URLs read better unbroken.',
        '.nh',
        '.SH NAME',
        '%s \\- %s' % (name.replace('-', '\\-'), summary),
    ]


def synopsis(name, forms):
    """One .SY/.YS block per form: filled, with a hanging indent."""
    out = ['.SH SYNOPSIS']
    for form in forms:
        out += ['.SY %s' % name.replace('-', '\\-'), syntax(form, words_italic=()), '.YS']
    return out


def tp(tag, body_lines):
    return ['.TP', tag] + body_lines


def common_tail(exit_codes, examples, refs, files, env):
    out = ['.SH FILES']
    for path, text in files:
        out += tp(syntax(path, words_italic=()), sentences(text))
    out.append('.SH ENVIRONMENT')
    for var, text in env:
        out += tp('.B ' + var, sentences(text))
    out.append('.SH EXIT STATUS')
    for code, text in exit_codes:
        out += tp('.B ' + code, sentences(text))
    out.append('.SH EXAMPLES')
    for note, code in examples:
        out += ['.PP'] + sentences(note) + example(code)[1:]
    out += ['.SH SEE ALSO'] + see_also(refs)
    out += ['.PP', 'The full manual, with guides and the scripting reference:', '.UR %s' % MANUAL, '.UE']
    out += ['.SH BUGS', 'Report bugs at', '.UR %s' % ISSUES, '.UE .']
    out += ['.SH AUTHOR', AUTHOR[0], '.MT %s' % AUTHOR[1], '.ME']
    out += ['.SH COPYRIGHT',
            'linscp is free software, licensed under the GNU General Public License,',
            'version 3 or (at your option) any later version (GPL\\-3.0\\-or\\-later).',
            'There is NO WARRANTY, to the extent permitted by law.',
            'The licences of the components it includes are installed in',
            '.IR /usr/share/doc/linscp .']
    return out


def text(cmd, key, default):
    return OVERRIDES.get((cmd, key), default)


def cli_page(t, s, date, version):
    out = header('LINSCP\\-CLI', 'linscp-cli', CLI_NAME, date, version,
                 ['linscp-website/src/data/cli.yaml', 'cli-structure.json'])
    out += synopsis('linscp-cli', CLI_SYNOPSIS)
    out.append('.SH DESCRIPTION')
    out += render(CLI_DESCRIPTION)
    out.append('.SH OPTIONS')
    for sw in s['switches']:
        out += tp(syntax(t.get('switchSyntax', {}).get(sw, sw), words_italic=()),
                  sentences(text(None, sw, t['switches'][sw])))
        for extra, extra_text in CLI_EXTRA_SWITCHES.get(sw, []):
            out += tp(syntax(extra, words_italic=()), sentences(extra_text))
    out.append('.SH SCRIPTS')
    out += render(CLI_SCRIPTS)
    out.append('.SH COMMANDS')
    out += render(t.get('intro', '').split('For how scripts')[0])
    for name in sorted(s['commands']):
        c, d = s['commands'][name], t['commands'][name]
        out.append('.SS %s' % name)
        out.append('.nf')
        out += [syntax(line) for line in d['syntax'].strip().split('\n')]
        out.append('.fi')
        out += render(text(name, 'body', d['body']))
        if c['aliases']:
            out += ['.PP', 'Also available as %s.' % ', '.join('\\fB%s\\fR' % roff_escape(a) for a in c['aliases'])]
        if c['switches']:
            out += ['.PP', 'Switches:']
            for sw in c['switches']:
                out += tp(syntax(d.get('switchSyntax', {}).get(sw, sw), words_italic=()),
                          sentences(text(name, sw, d['switches'][sw])))
        if c['options']:
            out += ['.PP', 'Affected by the script option%s %s.' % ('s' if len(c['options']) > 1 else '',
                    ', '.join('\\fB%s\\fR' % o for o in c['options']))]
        for e in d.get('examples') or []:
            label = 'Examples' if e['code'].strip().count('\n') else 'Example'
            if e.get('note'):
                out += ['.PP'] + sentences(label + ': ' + e['note'][0].lower() + e['note'][1:])
            else:
                out += ['.PP', label + ':']
            out += example(e['code'])[1:]
    out.append('.SH MASKS')
    out += render(CLI_MASKS)
    out.append('.SH SCRIPT OPTIONS')
    out += ['Set with \\fBoption\\fR \\fIname\\fR \\fIvalue\\fR.']
    for o in s['commands']['option']['scriptOptions']:
        od = t['options'][o]
        out += tp('\\fB%s\\fR %s' % (o, syntax(od['values'].replace(' ', ''), words_italic=()).replace('|', ' | ')),
                  render(od['text'], nested=True))
    out += common_tail(CLI_EXIT, CLI_EXAMPLES, CLI_SEE_ALSO, CLI_FILES, COMMON_ENV)
    return out


def gui_page(date, version):
    out = header('LINSCP', 'linscp', GUI_NAME, date, version, ['the text in the generator'])
    out += synopsis('linscp', GUI_SYNOPSIS)
    out.append('.SH DESCRIPTION')
    out += render(GUI_DESCRIPTION)
    out.append('.SH OPTIONS')
    for sw, desc in GUI_SWITCHES:
        out += tp(syntax(sw, words_italic=()), sentences(desc))
    files = [(p, d.replace('`linscp`(1)', '`linscp-cli`(1)')) for p, d in CLI_FILES]
    out += common_tail(GUI_EXIT, GUI_EXAMPLES, GUI_SEE_ALSO, files, COMMON_ENV)
    return out


def finish(lines):
    return '\n'.join(l.rstrip() for l in lines) + '\n'


def git_date(path):
    try:
        out = subprocess.run(['git', '-C', os.path.dirname(path), 'log', '-1', '--format=%cs', '--',
                              os.path.basename(path)], capture_output=True, text=True, check=True).stdout
    except (OSError, subprocess.CalledProcessError):
        return ''
    return out.strip()


def page_date():
    epoch = os.environ.get('SOURCE_DATE_EPOCH')
    if epoch:
        return datetime.datetime.fromtimestamp(int(epoch), datetime.timezone.utc).strftime('%Y-%m-%d')
    return max([HAND_DATE] + [d for d in (git_date(TEXTS), git_date(STRUCT)) if d])


def project_version():
    with open(os.path.join(ROOT, 'CMakeLists.txt'), encoding='utf-8') as f:
        m = re.search(r'project\(\s*linscp\s+VERSION\s+([\d.]+)', f.read())
    if not m:
        sys.exit('gen_manpages: no project(linscp VERSION ...) in CMakeLists.txt')
    return m.group(1)


def main():
    ap = argparse.ArgumentParser(description='Generate doc/man/linscp.1 and doc/man/linscp-cli.1.')
    ap.add_argument('--check', action='store_true', help='only report whether the pages are current')
    args = ap.parse_args()
    for path in (TEXTS, STRUCT):
        if not os.path.exists(path):
            sys.exit('gen_manpages: %s is missing (git submodule update --init linscp-website)' % path)
    with open(TEXTS, encoding='utf-8') as f:
        t = yaml.safe_load(f)
    with open(STRUCT, encoding='utf-8') as f:
        s = json.load(f)
    for sw in s['switches']:
        if sw not in t['switches']:
            sys.exit('gen_manpages: cli.yaml does not describe %s' % sw)
    date, version = page_date(), project_version()
    pages = {
        'linscp-cli.1': finish(cli_page(t, s, date, version)),
        'linscp.1': finish(gui_page(date, version)),
    }
    stale = []
    for name, content in pages.items():
        path = os.path.join(OUT, name)
        old = open(path, encoding='utf-8').read() if os.path.exists(path) else None
        if old != content:
            stale.append(name)
            if not args.check:
                os.makedirs(OUT, exist_ok=True)
                with open(path, 'w', encoding='utf-8', newline='\n') as f:
                    f.write(content)
    if args.check:
        if stale:
            sys.exit('gen_manpages: out of date: %s' % ', '.join(stale))
        print('doc/man is current')
    else:
        print('wrote %s' % (', '.join('doc/man/' + n for n in stale) or 'nothing (current)'))


if __name__ == '__main__':
    main()
