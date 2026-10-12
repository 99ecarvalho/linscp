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

    def scenario(self, proto, open_cmd, server, remote_root):
        """server: the served directory on disk, or a view of the server's
        storage (DiskView, S3View); remote_root: the same place as the
        client addresses it."""
        view = DiskView(server) if isinstance(server, str) else server
        view.seed()
        local = os.path.join(self.tmp, proto + '-local')
        os.makedirs(local, exist_ok=True)
        upload = os.path.join(local, 'upload.txt')
        expected = b'uploaded by ' + proto.encode() + b'\n'
        with open(upload, 'wb') as f:
            f.write(expected)
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
        mode = view.mode('newdir/moved.txt')
        if mode is not None:
            self.check(proto, mode & 0o600 == 0o600,
                       f'uploaded file is readable and writable by its owner (mode {mode:o})')
        self.check(proto, view.read('newdir/moved.txt') == expected, 'put uploads, mkdir creates, mv moves')
        self.check(proto, not view.exists('upload.txt'), 'mv leaves no source')
        self.check(proto, 'moved.txt' in text, 'ls lists the new directory', text)

        rc, text = self.run(
            '/command', 'option batch abort', 'option confirm off', open_cmd,
            f'rm {r}/newdir/moved.txt', f'rmdir {r}/newdir', 'exit')
        self.check(proto, rc == 0, f'cleanup script exits 0 (rc {rc})', text)
        self.check(proto, not view.exists('newdir/moved.txt') and not view.exists('newdir/'),
                   'rm and rmdir remove')


class DiskView:
    """A server that serves a directory: checked on disk."""

    def __init__(self, root):
        self.root = root

    def seed(self):
        os.makedirs(os.path.join(self.root, 'sub'), exist_ok=True)
        with open(os.path.join(self.root, 'hello.txt'), 'wb') as f:
            f.write(b'Hello, linscp\n')

    def path(self, rel):
        return os.path.join(self.root, *rel.rstrip('/').split('/'))

    def read(self, rel):
        p = self.path(rel)
        if not os.path.isfile(p):
            return None
        if not os.access(p, os.R_OK):
            os.chmod(p, 0o644)  # reported by mode(); read it anyway
        return open(p, 'rb').read()

    def exists(self, rel):
        return os.path.exists(self.path(rel))

    def mode(self, rel):
        p = self.path(rel)
        return (os.stat(p).st_mode & 0o777) if os.path.exists(p) else None


class S3View:
    """An S3 server checked through its API (boto3): moto has no files."""

    def __init__(self, endpoint, bucket, prefix):
        import boto3
        self.s3 = boto3.client('s3', endpoint_url=endpoint, region_name='us-east-1',
                               aws_access_key_id='LINSCPKEY', aws_secret_access_key='secret')
        self.bucket = bucket
        self.prefix = prefix

    def seed(self):
        self.s3.create_bucket(Bucket=self.bucket)
        self.s3.put_object(Bucket=self.bucket, Key=self.prefix + 'hello.txt', Body=b'Hello, linscp\n')
        self.s3.put_object(Bucket=self.bucket, Key=self.prefix + 'sub/', Body=b'')

    def read(self, rel):
        try:
            return self.s3.get_object(Bucket=self.bucket, Key=self.prefix + rel)['Body'].read()
        except self.s3.exceptions.NoSuchKey:
            return None

    def exists(self, rel):
        key = self.prefix + rel
        listed = self.s3.list_objects_v2(Bucket=self.bucket, Prefix=key).get('Contents', [])
        return any(o['Key'] == key or (key.endswith('/') and o['Key'].startswith(key)) for o in listed)

    def mode(self, rel):
        return None


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

        # FTP, and FTPS explicit (ftpes://) and implicit (ftps://): the
        # suite's own server, as none is installed here (ftp_server.py).
        for proto, tls, implicit in (('ftp', False, False), ('ftpes', True, False), ('ftps', True, True)):
            if not wanted(proto):
                continue
            try:
                root = os.path.join(tmp, proto + '-server')
                os.makedirs(root)
                args = []
                fp = None
                if tls:
                    cert, key, fp = fixtures.self_signed(tmp, proto)
                    args = ['--cert', cert, '--key', key] + (['--implicit'] if implicit else [])
                with fixtures.python_server('ftp_server.py', root, *args) as port:
                    extra = f' -certificate={fp}' if tls else ''
                    runner.scenario(proto, f'open {proto}://linscp:secret@127.0.0.1:{port}/{extra}', root, '/')
                    ran.append(proto)
            except fixtures.Missing as e:
                skipped.append(f'{proto} ({e})')

        # S3: the suite's own server (s3_server.py), plain HTTP, path-style
        # URLs as an IP address needs; buckets are directories of its root.
        if wanted('s3'):
            root = os.path.join(tmp, 's3-server')
            os.makedirs(os.path.join(root, 'bucket'))
            with fixtures.python_server('s3_server.py', root) as port:
                runner.scenario('s3', f'open s3plain://LINSCPKEY:secret@127.0.0.1:{port}/ -rawsettings S3UrlStyle=1',
                                os.path.join(root, 'bucket'), '/bucket')
                ran.append('s3')

        # Independent servers the suite did not write: pyftpdlib for FTP and
        # explicit FTPS, moto for S3 (checked through its API).
        for proto, tls in (('ftp-pyftpdlib', False), ('ftpes-pyftpdlib', True)):
            if not wanted(proto):
                continue
            if not fixtures.have_module('pyftpdlib'):
                skipped.append(f'{proto} (pyftpdlib)')
                continue
            try:
                root = os.path.join(tmp, proto + '-server')
                os.makedirs(root)
                args, extra = [], ''
                if tls:
                    cert, key, fp = fixtures.self_signed(tmp, proto)
                    args, extra = ['--cert', cert, '--key', key], f' -certificate={fp}'
                scheme = proto.split('-')[0]
                with fixtures.python_server('pyftpd_server.py', root, *args) as port:
                    runner.scenario(proto, f'open {scheme}://linscp:secret@127.0.0.1:{port}/{extra}', root, '/')
                    ran.append(proto)
            except fixtures.Missing as e:
                skipped.append(f'{proto} ({e})')
        if wanted('s3-moto'):
            try:
                with fixtures.moto_s3() as port:
                    view = S3View(f'http://127.0.0.1:{port}', 'linscp', 'work/')
                    runner.scenario('s3-moto', f'open s3plain://LINSCPKEY:secret@127.0.0.1:{port}/ '
                                    '-rawsettings S3UrlStyle=1', view, '/linscp/work')
                    ran.append('s3-moto')
            except fixtures.Missing as e:
                skipped.append(f's3-moto ({e})')

        # Through proxies, which must see the connection: SOCKS5 (microsocks)
        # and HTTP CONNECT (tinyproxy), both requiring credentials. PuTTY does
        # not proxy connections to the local host unless asked (ProxyLocalhost).
        def proxied(proto, method, proxy_port, target_open, server, remote, target_port, log_path):
            settings = (f' -rawsettings ProxyMethod={method} ProxyHost=127.0.0.1 ProxyPort={proxy_port} '
                        'ProxyUsername=proxyuser ProxyPassword=proxypass ProxyLocalhost=1')
            runner.scenario(proto, target_open + settings, server, remote)
            log = open(log_path, errors='replace').read() if os.path.exists(log_path) else ''
            runner.check(proto, str(target_port) in log, 'the proxy carried the connection', log[-1500:])
            ran.append(proto)

        if wanted('sftp-socks5') or wanted('sftp-http'):
            try:
                ssh_tmp = os.path.join(tmp, 'ssh-proxied')
                os.makedirs(ssh_tmp)
                with fixtures.sshd(ssh_tmp) as (port, key, host_key):
                    ppk = os.path.join(ssh_tmp, 'client.ppk')
                    runner.run('/keygen', runner.native(key), '/output=' + runner.native(ppk))
                    target = (f'open sftp://{user}@127.0.0.1:{port}/ -privatekey={runner.native(ppk)} '
                              f'-hostkey={host_key.split()[-1]}')
                    for proto in ('sftp-socks5', 'sftp-http'):
                        if not wanted(proto):
                            continue
                        root = os.path.join(tmp, proto + '-server')
                        os.makedirs(root)
                        try:
                            if proto == 'sftp-socks5':
                                log = os.path.join(tmp, proto + '.log')
                                with fixtures.socks5_proxy('proxyuser', 'proxypass', log) as pport:
                                    proxied(proto, 2, pport, target, root, root, port, log)
                            else:
                                with fixtures.http_proxy(tmp, 'proxyuser', 'proxypass') as (pport, log):
                                    proxied(proto, 3, pport, target, root, root, port, log)
                        except fixtures.Missing as e:
                            skipped.append(f'{proto} ({e})')
            except fixtures.Missing as e:
                skipped.append(f'sftp proxies ({e})')
        if wanted('ftp-socks5'):
            try:
                root = os.path.join(tmp, 'ftp-socks5-server')
                os.makedirs(root)
                log = os.path.join(tmp, 'ftp-socks5.log')
                with fixtures.python_server('ftp_server.py', root) as port, \
                        fixtures.socks5_proxy('proxyuser', 'proxypass', log) as pport:
                    proxied('ftp-socks5', 2, pport, f'open ftp://linscp:secret@127.0.0.1:{port}/', root, '/', port, log)
            except fixtures.Missing as e:
                skipped.append(f'ftp-socks5 ({e})')
        if wanted('dav-http'):
            try:
                root = os.path.join(tmp, 'dav-http-server')
                os.makedirs(root)
                with fixtures.rclone_webdav(root, 'linscp', 'secret') as port, \
                        fixtures.http_proxy(tmp, 'proxyuser', 'proxypass') as (pport, log):
                    proxied('dav-http', 3, pport, f'open dav://linscp:secret@127.0.0.1:{port}/', root, '/', port, log)
            except fixtures.Missing as e:
                skipped.append(f'dav-http ({e})')
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
