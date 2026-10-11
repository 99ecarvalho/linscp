#!/usr/bin/env python3
"""
The same everyday script over every protocol, through linscp-cli, against
throwaway servers on loopback: list, download, upload, make a directory,
move, delete, remove the directory -- each checked on the server's side.

  run_protocols.py [--emulator wine] [--only proto,...] linscp-cli[.exe]

A protocol whose server is not installed here is reported as skipped; the
test fails only on a protocol that ran. Exits 77 when none could run.
"""
import argparse, getpass, os, shutil, subprocess, sys, tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fixtures  # noqa: E402

SKIP = 77


class Runner:
    def __init__(self, cli, emulator, tmp):
        self.cli = cli
        self.emulator = emulator
        self.tmp = tmp
        self.failures = 0

    def native(self, path):
        return 'Z:' + path.replace('/', '\\') if self.emulator else path

    def local_dir(self, path):
        return self.native(path) + ('\\' if self.emulator else '/')

    def run(self, *args):
        cmd = ([self.emulator] if self.emulator else []) + [self.cli, '/ini=nul'] + list(args)
        p = subprocess.run(cmd, capture_output=True, timeout=180)
        return p.returncode, (p.stdout + p.stderr).decode(errors='replace')

    def check(self, proto, cond, what, detail=''):
        if not cond:
            self.failures += 1
            print(f'FAIL [{proto}] {what}')
            if detail:
                print('  ' + detail.strip().replace('\n', '\n  '))

    def scenario(self, proto, open_cmd, server_root, remote_root):
        """server_root: the served directory on disk; remote_root: the same
        directory as the client addresses it."""
        os.makedirs(os.path.join(server_root, 'sub'), exist_ok=True)
        with open(os.path.join(server_root, 'hello.txt'), 'wb') as f:
            f.write(b'Hello, linscp\n')
        local = os.path.join(self.tmp, proto + '-local')
        os.makedirs(local, exist_ok=True)
        upload = os.path.join(local, 'upload.txt')
        with open(upload, 'wb') as f:
            f.write(b'uploaded by ' + proto.encode() + b'\n')
        r = remote_root.rstrip('/')
        rc, text = self.run(
            '/command', 'option batch abort', 'option confirm off', open_cmd,
            f'ls {r}/',
            f'get {r}/hello.txt {self.local_dir(local)}',
            f'put {self.native(upload)} {r}/',
            f'mkdir {r}/newdir',
            f'mv {r}/upload.txt {r}/newdir/moved.txt',
            f'ls {r}/newdir/',
            'exit')
        self.check(proto, rc == 0, f'script exits 0 (rc {rc})', text)
        self.check(proto, 'hello.txt' in text and 'sub' in text, 'ls lists the directory', text)
        got = os.path.join(local, 'hello.txt')
        self.check(proto, os.path.isfile(got) and open(got, 'rb').read() == b'Hello, linscp\n',
                   'get downloads the file')
        moved = os.path.join(server_root, 'newdir', 'moved.txt')
        if os.path.exists(moved):
            mode = os.stat(moved).st_mode & 0o777
            self.check(proto, mode & 0o600 == 0o600, f'uploaded file is readable and writable by its owner (mode {mode:o})')
            if mode & 0o400 == 0:
                os.chmod(moved, 0o644)
        self.check(proto, os.path.isfile(moved) and
                   open(moved, 'rb').read() == b'uploaded by ' + proto.encode() + b'\n',
                   'put uploads, mkdir creates, mv moves')
        self.check(proto, not os.path.exists(os.path.join(server_root, 'upload.txt')), 'mv leaves no source')
        self.check(proto, 'moved.txt' in text, 'ls lists the new directory', text)

        rc, text = self.run(
            '/command', 'option batch abort', 'option confirm off', open_cmd,
            f'rm {r}/newdir/moved.txt', f'rmdir {r}/newdir', 'exit')
        self.check(proto, rc == 0, f'cleanup script exits 0 (rc {rc})', text)
        self.check(proto, not os.path.exists(os.path.join(server_root, 'newdir')), 'rm and rmdir remove')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--emulator')
    ap.add_argument('--only', default='')
    ap.add_argument('cli')
    a = ap.parse_args()
    only = set(filter(None, a.only.split(',')))

    tmp = tempfile.mkdtemp(prefix='linscp-protocols-')
    runner = Runner(a.cli, a.emulator, tmp)
    ran, skipped = [], []
    user = getpass.getuser()

    def wanted(proto):
        return not only or proto in only

    try:
        # SFTP and SCP: the same sshd, the key converted with /keygen.
        if wanted('sftp') or wanted('scp'):
            try:
                ssh_tmp = os.path.join(tmp, 'ssh')
                os.makedirs(ssh_tmp)
                with fixtures.sshd(ssh_tmp) as (port, key, host_key):
                    ppk = os.path.join(ssh_tmp, 'client.ppk')
                    rc, text = runner.run('/keygen', runner.native(key), '/output=' + runner.native(ppk))
                    runner.check('ssh', rc == 0, f'/keygen (rc {rc})', text)
                    for proto in ('sftp', 'scp'):
                        if not wanted(proto):
                            continue
                        root = os.path.join(tmp, proto + '-server')
                        os.makedirs(root)
                        runner.scenario(proto,
                            f'open {proto}://{user}@127.0.0.1:{port}/ -privatekey={runner.native(ppk)} '
                            f'-hostkey={host_key.split()[-1]}',
                            root, root)
                        ran.append(proto)
            except fixtures.Missing as e:
                skipped.append(f'sftp/scp ({e})')

        # WebDAV, plain and over TLS: rclone.
        for proto, tls in (('dav', False), ('davs', True)):
            if not wanted(proto):
                continue
            try:
                root = os.path.join(tmp, proto + '-server')
                os.makedirs(root)
                cert = key = fp = None
                if tls:
                    cert, key, fp = fixtures.self_signed(tmp, proto)
                with fixtures.rclone_webdav(root, 'linscp', 'secret', cert, key) as port:
                    extra = f' -certificate={fp}' if tls else ''
                    runner.scenario(proto, f'open {proto}://linscp:secret@127.0.0.1:{port}/{extra}', root, '/')
                    ran.append(proto)
            except fixtures.Missing as e:
                skipped.append(f'{proto} ({e})')
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    print(f'ran: {", ".join(ran) or "none"}')
    if skipped:
        print(f'skipped: {", ".join(skipped)}')
    if runner.failures:
        return 1
    if not ran:
        return SKIP
    print('OK')
    return 0


if __name__ == '__main__':
    sys.exit(main())
