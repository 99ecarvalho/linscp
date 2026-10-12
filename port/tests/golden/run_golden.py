#!/usr/bin/env python3
"""
Golden-output tests: the same scripts, run by real WinSCP and by linscp,
against the same servers, their XML session logs compared.

  run_golden.py capture --client WinSCP.com|linscp-cli [--emulator wine]
                        --out DIR [--only proto,...] [--cases name,...]
  run_golden.py compare EXPECTED_DIR ACTUAL_DIR
  run_golden.py check   --client linscp-cli [--emulator wine] EXPECTED_DIR

capture runs every case (cases/*.txt) over every protocol whose server is
available and writes DIR/<protocol>/<case>.xml, normalised. compare diffs two
such directories. check is capture into a temporary directory, then compare:
what ctest runs against the committed corpus (expected/), which comes from
real WinSCP on Windows.

The servers and data are chosen to be identical on Linux and Windows, so
the logs can differ only by the client: rclone for SFTP and WebDAV (fixed
permissions), the suite's own FTP and S3 servers (fixed permissions), one
committed TLS certificate, password logins, host keys and certificates
accepted with "*", files with fixed timestamps. Normalising removes only
what cannot match between runs: the session's start time and the local
working directory (and its path separators).

Needs Python 3.8+ and rclone on PATH; a protocol whose server is missing is
skipped. On Windows run it from a checkout of this repository.
"""
import argparse, difflib, os, shutil, subprocess, sys, tempfile, time
import xml.etree.ElementTree as ET

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import fixtures  # noqa: E402

SKIP = 77
CERT = os.path.join(HERE, 'data', 'test-server.crt')
KEY = os.path.join(HERE, 'data', 'test-server.key')
USER, PASSWORD = 'linscp', 'secret'
# 2020-01-02 03:04:05 UTC: every fixture file's modification time.
FIXED_TIME = 1577934245

PROTOCOLS = ['sftp', 'dav', 'davs', 'ftp', 'ftpes', 'ftps', 's3']


def write(path, data, when=FIXED_TIME):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as f:
        f.write(data)
    os.utime(path, (when, when))


def make_tree(server_root, work):
    """The remote tree under server_root/work, and the local working files."""
    if os.path.exists(server_root):
        shutil.rmtree(server_root)
    base = os.path.join(server_root, work)
    write(os.path.join(base, 'hello.txt'), b'Hello, linscp\n')
    write(os.path.join(base, 'data.bin'), bytes(range(256)) * 4)
    write(os.path.join(base, 'sub', 'a.txt'), b'a\n')
    write(os.path.join(base, 'sub', 'b.txt'), b'bb\n')
    for d in (os.path.join(base, 'sub'), base):
        os.utime(d, (FIXED_TIME, FIXED_TIME))


def make_local(local):
    if os.path.exists(local):
        shutil.rmtree(local)
    write(os.path.join(local, 'upload.txt'), b'uploaded\n')
    write(os.path.join(local, 'mirror', 'local-only.txt'), b'only here\n')
    os.makedirs(os.path.join(local, 'masked'))


class Servers:
    """Starts each protocol's server on demand, for one case at a time."""

    def __init__(self, tmp):
        self.tmp = tmp

    def start(self, proto, root):
        """A context yielding (open command, remote root)."""
        if proto == 'sftp':
            ctx = fixtures.rclone_sftp(root, USER, PASSWORD, os.path.join(self.tmp, 'rclone-sftp'))
            return ctx, lambda port: (f'open sftp://{USER}:{PASSWORD}@127.0.0.1:{port}/ -hostkey=* -rawsettings TryAgent=0', '/work')
        if proto in ('dav', 'davs'):
            tls = proto == 'davs'
            ctx = fixtures.rclone_webdav(root, USER, PASSWORD, CERT if tls else None, KEY if tls else None)
            extra = ' -certificate=*' if tls else ''
            return ctx, lambda port: (f'open {proto}://{USER}:{PASSWORD}@127.0.0.1:{port}/{extra}', '/work')
        if proto in ('ftp', 'ftpes', 'ftps'):
            args = ['--user', USER, '--password', PASSWORD]
            if proto != 'ftp':
                args += ['--cert', CERT, '--key', KEY] + (['--implicit'] if proto == 'ftps' else [])
            ctx = fixtures.python_server('ftp_server.py', root, *args)
            extra = ' -certificate=*' if proto != 'ftp' else ''
            return ctx, lambda port: (f'open {proto}://{USER}:{PASSWORD}@127.0.0.1:{port}/{extra}', '/work')
        if proto == 's3':
            ctx = fixtures.python_server('s3_server.py', root)
            return ctx, lambda port: (f'open s3plain://LINSCPKEY:{PASSWORD}@127.0.0.1:{port}/ '
                                      f'-rawsettings S3UrlStyle=1', '/bucket/work')
        raise ValueError(proto)


def recent(value):
    """A timestamp from this run (a file the client wrote without keeping
    its time) as {NOW}; fixture times are kept, so preserving them shows."""
    if len(value) >= 19 and value[4] == '-' and value[10] == 'T':
        try:
            t = time.mktime(time.strptime(value[:19], '%Y-%m-%dT%H:%M:%S')) - time.timezone
        except ValueError:
            return value
        if t > FIXED_TIME + 86400:
            return '{NOW}'
    return value


def normalise(xml_text, local_roots):
    """The log with what differs between runs replaced: the start time, and
    the local working directory, as {LOCAL} with '/' separators."""
    root = ET.fromstring(xml_text)

    # Longest first: Wine's Z:\... form contains the host path.
    local_roots = sorted(set(local_roots), key=len, reverse=True)

    def fix(value):
        value = recent(value)
        for local in local_roots:
            for form in (local, local.replace('/', '\\'), local.replace('\\', '/')):
                if form and form in value:
                    value = value.replace(form, '{LOCAL}')
        if '{LOCAL}' in value:
            value = value.replace('\\', '/')
        return value

    for element in root.iter():
        if element.tag.endswith('session'):
            element.attrib.pop('start', None)
        for k, v in list(element.attrib.items()):
            element.attrib[k] = fix(v)
        if element.text and element.text.strip():
            element.text = fix(element.text)
    ET.indent(root)
    return ET.tostring(root, encoding='unicode') + '\n'


def capture(a):
    cases_dir = os.path.join(HERE, 'cases')
    cases = sorted(f[:-4] for f in os.listdir(cases_dir) if f.endswith('.txt'))
    if a.cases:
        cases = [c for c in cases if c in a.cases.split(',')]
    protocols = [p for p in PROTOCOLS if not a.only or p in a.only.split(',')]
    tmp = tempfile.mkdtemp(prefix='linscp-golden-')
    servers = Servers(tmp)
    ran, skipped = 0, []
    try:
        for proto in protocols:
            for case in cases:
                server_root = os.path.join(tmp, 'server')
                work_root = os.path.join(server_root, 'bucket') if proto == 's3' else server_root
                make_tree(work_root, 'work')
                local = os.path.join(tmp, 'local')
                make_local(local)
                try:
                    ctx, describe = servers.start(proto, server_root)
                    with ctx as port:
                        open_cmd, remote = describe(port)
                        native_local = ('Z:' + local.replace('/', '\\')) if a.emulator else local
                        sep = '\\' if (a.emulator or os.name == 'nt') else '/'
                        script = open(os.path.join(cases_dir, case + '.txt'), encoding='utf-8').read()
                        script = '\n'.join(l for l in script.splitlines() if not l.startswith('#'))
                        script = (script.replace('{OPEN}', open_cmd).replace('{R}', remote)
                                  .replace('{L}', native_local).replace('{SEP}', sep))
                        script_path = os.path.join(tmp, 'script.txt')
                        with open(script_path, 'w', encoding='utf-8') as f:
                            f.write(script + '\n')
                        xml_path = os.path.join(tmp, 'session.xml')
                        if os.path.exists(xml_path):
                            os.remove(xml_path)
                        native = (lambda p: 'Z:' + p.replace('/', '\\')) if a.emulator else (lambda p: p)
                        cmd = ([a.emulator] if a.emulator else []) + [
                            a.client, '/ini=nul', '/xmllog=' + native(xml_path), '/script=' + native(script_path)]
                        out_dir = os.path.join(a.out, proto)
                        os.makedirs(out_dir, exist_ok=True)
                        p = subprocess.run(cmd, capture_output=True, timeout=300)
                        with open(os.path.join(out_dir, case + '.out'), 'wb') as f:
                            f.write(p.stdout + p.stderr)
                        if not os.path.exists(xml_path):
                            print(f'FAIL [{proto}/{case}] no XML log (exit {p.returncode})')
                            continue
                        text = open(xml_path, encoding='utf-8').read()
                        with open(os.path.join(out_dir, case + '.xml'), 'w', encoding='utf-8', newline='\n') as f:
                            f.write(normalise(text, [local, native_local]))
                        ran += 1
                        print(f'{proto}/{case}: exit {p.returncode}')
                except fixtures.Missing as e:
                    skipped.append(f'{proto} ({e})')
                    break
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    if skipped:
        print('skipped: ' + ', '.join(skipped))
    return ran


def known_differences():
    known = {}
    for raw in open(os.path.join(HERE, 'known-differences.txt'), encoding='utf-8'):
        fields = raw.split('#', 1)[0].split(None, 1)
        if fields:
            known[fields[0]] = fields[1].strip() if len(fields) > 1 else ''
    return known


def compare(expected, actual):
    """Returns (compared, differing); prints the diffs."""
    known = known_differences()
    compared, differing, missing = 0, 0, []
    for proto in sorted(os.listdir(expected)):
        for name in sorted(os.listdir(os.path.join(expected, proto))):
            if not name.endswith('.xml'):
                continue
            want_path = os.path.join(expected, proto, name)
            got_path = os.path.join(actual, proto, name)
            if not os.path.exists(got_path):
                missing.append(f'{proto}/{name}')
                continue
            want = open(want_path, encoding='utf-8').read().splitlines()
            got = open(got_path, encoding='utf-8').read().splitlines()
            compared += 1
            if want != got:
                reason = known.get(f'{proto}/{name}')
                if reason is None:
                    differing += 1
                    print(f'DIFF {proto}/{name}')
                else:
                    print(f'EXPECTED DIFF {proto}/{name}: {reason}')
                for line in difflib.unified_diff(want, got, 'expected', 'actual', lineterm='', n=2):
                    print('  ' + line)
    if missing:
        print('not produced: ' + ', '.join(missing))
    return compared, differing + len(missing)


def main():
    if sys.version_info < (3, 9):
        sys.exit('run_golden.py needs Python 3.9 or later')
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest='command', required=True)
    c = sub.add_parser('capture')
    c.add_argument('--client', required=True)
    c.add_argument('--emulator')
    c.add_argument('--out', required=True)
    c.add_argument('--only')
    c.add_argument('--cases')
    d = sub.add_parser('compare')
    d.add_argument('expected')
    d.add_argument('actual')
    k = sub.add_parser('check')
    k.add_argument('--client', required=True)
    k.add_argument('--emulator')
    k.add_argument('expected')
    a = ap.parse_args()

    if a.command == 'capture':
        return 0 if capture(a) > 0 else SKIP
    if a.command == 'compare':
        compared, bad = compare(a.expected, a.actual)
        print(f'compared {compared}, differing {bad}')
        return 1 if bad else 0
    # check
    if not os.path.isdir(a.expected) or not os.listdir(a.expected):
        print(f'SKIP: no golden corpus in {a.expected}')
        return SKIP
    out = tempfile.mkdtemp(prefix='linscp-golden-out-')
    try:
        a.out, a.only, a.cases = out, None, None
        if capture(a) == 0:
            return SKIP
        compared, bad = compare(a.expected, out)
        print(f'compared {compared}, differing {bad}')
        return 1 if bad else 0
    finally:
        shutil.rmtree(out, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())
