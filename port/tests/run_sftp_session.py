#!/usr/bin/env python3
"""
Run test_sftp_session against a throwaway OpenSSH server on loopback.

Everything lives in a temporary directory: a host key, a client key that is
the only authorized key, the sshd configuration and the remote files. sshd
runs unprivileged, so it can only log in the user running the test, which is
the user the test logs in as. Nothing outside the directory is touched, and
the server listens on 127.0.0.1 only.

  run_sftp_session.py [--emulator wine] [--sshd 'Ciphers aes128-ctr' ...]
                      test_sftp_session[.exe]

Exits 77 (ctest's skip code here) when no sshd is installed.
"""
import argparse, getpass, os, shutil, socket, subprocess, sys, tempfile, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fixtures  # noqa: E402

SKIP = 77


def free_port():
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]


def keygen(path):
    subprocess.run(['ssh-keygen', '-q', '-t', 'ed25519', '-N', '', '-f', path],
                   check=True)


def fingerprint(pub):
    # "256 SHA256:<base64> comment (ED25519)" -> PuTTY's "ssh-ed25519 255 <base64>"
    out = subprocess.run(['ssh-keygen', '-l', '-E', 'sha256', '-f', pub],
                         check=True, capture_output=True, text=True).stdout.split()
    return 'ssh-ed25519 255 ' + out[1].split(':', 1)[1]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--emulator')
    ap.add_argument('--sshd', action='append', default=[],
                    help='an extra sshd_config line, e.g. to pin one cipher')
    ap.add_argument('test')
    a = ap.parse_args()
    # Windows' sshd lets no one in run unprivileged, as here.
    if os.name == 'nt':
        print('SKIP: sshd (not run on Windows)')
        return 77
    sshd = shutil.which('sshd') or '/usr/sbin/sshd'
    if not os.path.isfile(sshd) or not shutil.which('ssh-keygen'):
        print('SKIP: no sshd')
        return SKIP

    tmp = tempfile.mkdtemp(prefix='linscp-sftp-')
    server = None
    try:
        keygen(os.path.join(tmp, 'host'))
        keygen(os.path.join(tmp, 'client'))
        shutil.copy(os.path.join(tmp, 'client.pub'), os.path.join(tmp, 'authorized_keys'))
        remote = os.path.join(tmp, 'remote')
        os.makedirs(os.path.join(remote, 'sub'))
        with open(os.path.join(remote, 'hello.txt'), 'wb') as f:
            f.write(b'Hello, linscp\n')
        local = os.path.join(tmp, 'local')
        os.makedirs(local)

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
                    'LogLevel ERROR'] + a.sshd + ['']))
            return subprocess.Popen([sshd, '-D', '-e', '-f', config])
        try:
            port, server = fixtures.start_server(launch)
        except RuntimeError:
            print('FAIL: sshd did not start')
            return 1

        def native(path):
            # Wine maps the host's root to drive Z:.
            return 'Z:' + path.replace('/', '\\') if a.emulator else path

        cmd = ([a.emulator] if a.emulator else []) + [
            a.test, '127.0.0.1', str(port), getpass.getuser(),
            native(os.path.join(tmp, 'client')), fingerprint(os.path.join(tmp, 'host.pub')),
            remote, native(local)]
        return subprocess.run(cmd, timeout=120).returncode
    finally:
        if server is not None:
            server.terminate()
            server.wait(timeout=10)
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())
