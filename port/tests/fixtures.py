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


def start_server(launch, attempts=5):
    """(port, process): launch(port) run on a free port. A free port is free
    only when it is checked: a test starting at the same time may take it
    first, and then this server exits, unable to bind, while the other's
    answers in its place. So the server must still be running once its port
    answers; if not, another port is tried."""
    for _ in range(attempts):
        port = free_port()
        server = launch(port)
        try:
            wait_for_port(port, server)
            time.sleep(0.3)
            if server.poll() is None:
                return port, server
        except RuntimeError:
            pass
        if server.poll() is None:
            server.terminate()
        server.wait(timeout=10)
    raise RuntimeError('the server did not start on a free port')


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


def rclone_binary():
    """$LINSCP_RCLONE if set (the golden corpus pins a version), else rclone
    on PATH."""
    chosen = os.environ.get('LINSCP_RCLONE')
    return chosen if chosen and os.path.exists(chosen) else shutil.which('rclone')


def rclone_version():
    binary = rclone_binary()
    if not binary:
        return None
    out = subprocess.run([binary, 'version'], capture_output=True, text=True).stdout
    return out.split('\n', 1)[0].strip()


def empty_config():
    """An empty rclone configuration file: rclone on Windows rejects "nul"."""
    import tempfile
    path = os.path.join(tempfile.gettempdir(), 'linscp-rclone-empty.conf')
    if not os.path.exists(path):
        open(path, 'w').close()
    return path


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
    config = os.path.join(tmp, 'sshd_config')

    def launch(port):
        with open(config, 'w') as f:
            f.write('\n'.join([
                f'Port {port}', 'ListenAddress 127.0.0.1',
                f'HostKey {tmp}/host', f'PidFile {tmp}/sshd.pid',
                f'AuthorizedKeysFile {tmp}/authorized_keys',
                'StrictModes no', 'UsePAM no',
                'PasswordAuthentication no', 'KbdInteractiveAuthentication no',
                'PubkeyAuthentication yes', 'Subsystem sftp internal-sftp',
                'LogLevel ERROR'] + list(extra) + ['']))
        return subprocess.Popen([binary, '-D', '-e', '-f', config])
    port, server = start_server(launch)
    try:
        yield port, os.path.join(tmp, 'client'), fingerprint(os.path.join(tmp, 'host.pub'))
    finally:
        server.terminate()
        server.wait(timeout=10)


@contextlib.contextmanager
def rclone_webdav(root, user, password, cert=None, key=None):
    """rclone serving root over WebDAV (HTTPS with cert and key). Yields the port."""
    binary = rclone_binary()
    if not binary:
        raise Missing('rclone')
    def launch(port):
        cmd = [binary, 'serve', 'webdav', root, '--addr', f'127.0.0.1:{port}',
               '--user', user, '--pass', password, '--config', empty_config(), '-q']
        if cert:
            cmd += ['--cert', cert, '--key', key]
        return subprocess.Popen(cmd)
    port, server = start_server(launch)
    try:
        yield port
    finally:
        server.terminate()
        server.wait(timeout=10)


@contextlib.contextmanager
def python_server(script, root, *args):
    """One of the test servers written for this suite, run as a process.
    Yields the port."""
    import sys
    here = os.path.dirname(os.path.abspath(__file__))
    port, server = start_server(lambda port: subprocess.Popen(
        [sys.executable, '-I', os.path.join(here, script), '--port', str(port), '--root', root] + list(args)))
    try:
        yield port
    finally:
        server.terminate()
        server.wait(timeout=10)


@contextlib.contextmanager
def rclone_sftp(root, user, password, host_key_dir):
    """rclone serving root over SFTP to one user with a password; its host
    key is generated into host_key_dir on first use. Yields the port."""
    binary = rclone_binary()
    if not binary:
        raise Missing('rclone')
    port, server = start_server(lambda port: subprocess.Popen(
        [binary, 'serve', 'sftp', root, '--addr', f'127.0.0.1:{port}',
         '--user', user, '--pass', password, '--config', empty_config(), '-q',
         '--cache-dir', host_key_dir]))
    try:
        yield port
    finally:
        server.terminate()
        server.wait(timeout=10)


def have_module(name):
    import importlib.util
    return importlib.util.find_spec(name) is not None


@contextlib.contextmanager
def moto_s3():
    """moto's S3 server (an AWS mock the suite did not write). Yields the port."""
    binary = shutil.which('moto_server')
    if not binary or not have_module('boto3') or not have_module('flask_cors'):
        raise Missing('moto_server, boto3 and flask_cors (python3-flask-cors)')
    port, server = start_server(lambda port: subprocess.Popen(
        [binary, '-H', '127.0.0.1', '-p', str(port)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL))
    try:
        yield port
    finally:
        server.terminate()
        server.wait(timeout=10)


@contextlib.contextmanager
def socks5_proxy(user, password, log_path):
    """microsocks requiring user/password; its connection log goes to
    log_path. Yields the port."""
    binary = shutil.which('microsocks')
    if not binary:
        raise Missing('microsocks')
    log = open(log_path, 'wb')
    port, server = start_server(lambda port: subprocess.Popen(
        [binary, '-i', '127.0.0.1', '-p', str(port), '-u', user, '-P', password], stdout=log, stderr=subprocess.STDOUT))
    try:
        yield port
    finally:
        server.terminate()
        server.wait(timeout=10)
        log.close()


@contextlib.contextmanager
def http_proxy(tmp, user, password):
    """tinyproxy requiring basic auth, logging connections. Yields
    (port, log path)."""
    binary = shutil.which('tinyproxy')
    if not binary:
        raise Missing('tinyproxy')
    log_path = os.path.join(tmp, 'tinyproxy.log')
    config = os.path.join(tmp, 'tinyproxy.conf')

    def launch(port):
        with open(config, 'w') as f:
            f.write('\n'.join([f'Port {port}', 'Listen 127.0.0.1', 'Timeout 60', f'LogFile "{log_path}"',
                               'LogLevel Connect', 'MaxClients 20', 'Allow 127.0.0.1',
                               f'BasicAuth {user} {password}', '']))
        return subprocess.Popen([binary, '-d', '-c', config], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    port, server = start_server(launch)
    try:
        yield port, log_path
    finally:
        server.terminate()
        server.wait(timeout=10)
