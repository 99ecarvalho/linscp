"""
Throwaway servers on loopback for the protocol tests, and the client runner.

Each server serves a directory the test owns, listens on 127.0.0.1 only and
is stopped with its context. Nothing outside the test's temporary directory
is touched. A server whose program is missing raises Missing, which the
tests turn into a skip for that protocol.
"""
import contextlib, os, shutil, socket, subprocess, time


class Missing(Exception):
    """The program a fixture needs is not installed."""


def free_port():
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]


def wait_for_port(port, process, timeout=10):
    deadline = time.monotonic() + timeout
    while True:
        try:
            socket.create_connection(('127.0.0.1', port), timeout=1).close()
            return
        except OSError:
            if (process is not None and process.poll() is not None) or time.monotonic() > deadline:
                raise RuntimeError(f'server on port {port} did not start')
            time.sleep(0.1)


def keygen(path):
    subprocess.run(['ssh-keygen', '-q', '-t', 'ed25519', '-N', '', '-f', path], check=True)


def fingerprint(pub):
    """PuTTY's form of an OpenSSH public key's SHA-256 fingerprint."""
    out = subprocess.run(['ssh-keygen', '-l', '-E', 'sha256', '-f', pub],
                         check=True, capture_output=True, text=True).stdout.split()
    return 'ssh-ed25519 255 ' + out[1].split(':', 1)[1]


def self_signed(directory, name='server'):
    """A certificate for 127.0.0.1 and its key; returns (cert, key, sha256)."""
    if not shutil.which('openssl'):
        raise Missing('openssl')
    cert = os.path.join(directory, name + '.crt')
    key = os.path.join(directory, name + '.key')
    subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '2',
                    '-subj', '/CN=127.0.0.1', '-addext', 'subjectAltName=IP:127.0.0.1',
                    '-keyout', key, '-out', cert], check=True, capture_output=True)
    out = subprocess.run(['openssl', 'x509', '-in', cert, '-noout', '-fingerprint', '-sha256'],
                         check=True, capture_output=True, text=True).stdout
    return cert, key, out.strip().split('=', 1)[1].lower()


@contextlib.contextmanager
def sshd(tmp, extra=()):
    """An unprivileged sshd that only lets the current user in with a fresh
    key. Yields (port, client key path, host key fingerprint)."""
    binary = shutil.which('sshd') or '/usr/sbin/sshd'
    if not os.path.isfile(binary) or not shutil.which('ssh-keygen'):
        raise Missing('sshd')
    keygen(os.path.join(tmp, 'host'))
    keygen(os.path.join(tmp, 'client'))
    shutil.copy(os.path.join(tmp, 'client.pub'), os.path.join(tmp, 'authorized_keys'))
    port = free_port()
    config = os.path.join(tmp, 'sshd_config')
    with open(config, 'w') as f:
        f.write('\n'.join([
            f'Port {port}', 'ListenAddress 127.0.0.1',
            f'HostKey {tmp}/host', f'PidFile {tmp}/sshd.pid',
            f'AuthorizedKeysFile {tmp}/authorized_keys',
            'StrictModes no', 'UsePAM no',
            'PasswordAuthentication no', 'KbdInteractiveAuthentication no',
            'PubkeyAuthentication yes', 'Subsystem sftp internal-sftp',
            'LogLevel ERROR'] + list(extra) + ['']))
    server = subprocess.Popen([binary, '-D', '-e', '-f', config])
    try:
        wait_for_port(port, server)
        yield port, os.path.join(tmp, 'client'), fingerprint(os.path.join(tmp, 'host.pub'))
    finally:
        server.terminate()
        server.wait(timeout=10)


@contextlib.contextmanager
def rclone_webdav(root, user, password, cert=None, key=None):
    """rclone serving root over WebDAV (HTTPS with cert and key). Yields the port."""
    binary = shutil.which('rclone')
    if not binary:
        raise Missing('rclone')
    port = free_port()
    cmd = [binary, 'serve', 'webdav', root, '--addr', f'127.0.0.1:{port}',
           '--user', user, '--pass', password, '--config', os.devnull, '-q']
    if cert:
        cmd += ['--cert', cert, '--key', key]
    server = subprocess.Popen(cmd)
    try:
        wait_for_port(port, server)
        yield port
    finally:
        server.terminate()
        server.wait(timeout=10)


@contextlib.contextmanager
def python_server(script, root, *args):
    """One of the test servers written for this suite, run as a process.
    Yields the port."""
    import sys
    port = free_port()
    here = os.path.dirname(os.path.abspath(__file__))
    server = subprocess.Popen([sys.executable, '-I', os.path.join(here, script),
                               '--port', str(port), '--root', root] + list(args))
    try:
        wait_for_port(port, server)
        yield port
    finally:
        server.terminate()
        server.wait(timeout=10)
