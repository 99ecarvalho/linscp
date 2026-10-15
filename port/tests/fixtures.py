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


# --- Real servers in a container (P3-18) ------------------------------------

SERVERS_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'servers')


def container_engine():
    """$LINSCP_CONTAINER_ENGINE, else docker where its daemon answers, else
    podman; None without either."""
    chosen = os.environ.get('LINSCP_CONTAINER_ENGINE')
    candidates = [chosen] if chosen else ['docker', 'podman']
    for engine in candidates:
        if shutil.which(engine) and subprocess.run([engine, 'info'], capture_output=True).returncode == 0:
            return engine
    return None


def servers_image(engine):
    """The servers' image, built once per content of servers/ (its tag that
    content's hash)."""
    import hashlib
    digest = hashlib.sha256()
    for name in sorted(os.listdir(SERVERS_DIR)):
        digest.update(name.encode() + b'\0' + open(os.path.join(SERVERS_DIR, name), 'rb').read())
    image = 'linscp-test-servers:' + digest.hexdigest()[:16]
    if subprocess.run([engine, 'image', 'inspect', image], capture_output=True).returncode != 0:
        build = subprocess.run([engine, 'build', '-q', '-f', os.path.join(SERVERS_DIR, 'Containerfile'), '-t', image,
                                SERVERS_DIR], capture_output=True, text=True)
        if build.returncode != 0:
            raise Missing(f'the servers image (building it failed: {build.stderr[-500:]})')
    return image


@contextlib.contextmanager
def servers(root, tmp, services, s3_root=None, s3_access='linscp', mounts=()):
    """Real servers in a container (servers/Containerfile) serving root:
    services among 'ssh', 'ftp', 'ftps', 'dav', 'davs', 's3', each on a free
    port of 127.0.0.1 (the host's network). User linscp, password secret (S3:
    access key s3_access, its root s3_root, else root). A TLS certificate for
    127.0.0.1 and an SSH host key are made in tmp. Yields a dict: each
    service's port, 'certificate' (its SHA-256), 'hostkey' (PuTTY's
    fingerprint), 'key' (an OpenSSH client key the server accepts). mounts:
    more host directories the servers see, at the same paths."""
    engine = container_engine()
    if engine is None:
        raise Missing('docker or podman')
    image = servers_image(engine)
    keys = os.path.join(tmp, 'server-keys')
    os.makedirs(keys, exist_ok=True)
    cert, key, cert_fp = self_signed(keys, 'server')
    keygen(os.path.join(keys, 'ssh_host_ed25519_key'))
    keygen(os.path.join(keys, 'client'))
    shutil.copy(os.path.join(keys, 'client.pub'), os.path.join(keys, 'authorized_keys'))
    for name in os.listdir(keys):
        os.chmod(os.path.join(keys, name), 0o644)
    names = {'ssh': 'SSH_PORT', 'ftp': 'FTP_PORT', 'ftps': 'FTPS_PORT', 'dav': 'DAV_PORT', 'davs': 'DAVS_PORT',
             's3': 'S3_PORT'}
    for attempt in range(3):
        ports = {service: free_port() for service in services}
        env = {names[service]: str(port) for service, port in ports.items()}
        # The passive data ports: ten, from a free one on.
        for service, prefix in (('ftp', 'FTP'), ('ftps', 'FTPS')):
            if service in ports:
                base = free_port()
                env[prefix + '_PASV_MIN'], env[prefix + '_PASV_MAX'] = str(base), str(base + 9)
        env.update(ROOT=os.path.abspath(root), UID_GID=f'{os.getuid()}:{os.getgid()}', S3_ACCESS=s3_access)
        if s3_root:
            env['S3_ROOT'] = os.path.abspath(s3_root)
        seen = {os.path.abspath(root), os.path.abspath(s3_root or root)} | {os.path.abspath(m) for m in mounts}
        # A directory inside another is seen through it.
        seen = {m for m in seen if not any(m != o and m.startswith(o + os.sep) for o in seen)}
        cmd = [engine, 'run', '-d', '--rm', '--network', 'host']
        if engine == 'podman':
            # Rootless: the host user is itself inside, the entrypoint root.
            cmd += ['--userns', 'keep-id', '--user', 'root']
        for mount in sorted(seen):
            cmd += ['-v', f'{mount}:{mount}']
        cmd += ['-v', f'{keys}:/keys:ro']
        for name, value in env.items():
            cmd += ['-e', f'{name}={value}']
        cmd.append(image)
        started = subprocess.run(cmd, capture_output=True, text=True)
        if started.returncode != 0:
            raise RuntimeError(f'the servers container did not start: {started.stderr[-500:]}')
        container = started.stdout.strip()
        try:
            # Its own word that every server listens (entrypoint.sh), then
            # the ports.
            deadline = time.monotonic() + 30
            while True:
                logs = subprocess.run([engine, 'logs', container], capture_output=True, text=True)
                if 'linscp-servers: started' in logs.stdout:
                    break
                running = subprocess.run([engine, 'inspect', '-f', '{{.State.Running}}', container],
                                         capture_output=True, text=True).stdout.strip()
                if (running != 'true') or (time.monotonic() > deadline):
                    raise RuntimeError('not started')
                time.sleep(0.2)
            for port in ports.values():
                wait_for_port(port, None, timeout=30)
        except RuntimeError:
            logs = subprocess.run([engine, 'logs', container], capture_output=True, text=True)
            subprocess.run([engine, 'rm', '-f', container], capture_output=True)
            if attempt == 2:
                raise RuntimeError('the servers did not start: ' + (logs.stdout + logs.stderr)[-1500:])
            continue
        try:
            yield dict(ports, certificate=cert_fp, hostkey=fingerprint(os.path.join(keys, 'ssh_host_ed25519_key.pub')),
                       key=os.path.join(keys, 'client'))
        finally:
            subprocess.run([engine, 'rm', '-f', container], capture_output=True)
        return
