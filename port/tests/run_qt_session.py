#!/usr/bin/env python3
"""
A Qt shell test against rclone's SFTP server on loopback: test_qt_session
(the bridge to core, a scripted frontend in Qt's place) or test_qt_gui
session (the main window, its dialogs answered by a responder).

  run_qt_session.py [--emulator wine] [--protocol sftp|ftp|sshd] test[.exe] [argument ...]

The server is rclone's SFTP, or the suite's own FTP server (ftp_server.py);
user linscp, password secret either way. Or (sshd) OpenSSH's sshd, with a
real shell, letting the current user ({user}) in with a fresh key ({key}).
The arguments default to /ini=nul {port} {protocol}, with the server's port
and protocol filled in, {tmp} with a temporary directory of the test's own
({hosttmp}: the same as the server sees it, a host path even under Wine),
and {key} with an OpenSSH private key made there (ssh-keygen).

Exits 77 without rclone (or sshd).
"""
import argparse, getpass, os, shutil, subprocess, sys, tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fixtures  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--emulator', default='')
    ap.add_argument('--protocol', choices=('sftp', 'ftp', 'sshd'), default='sftp')
    ap.add_argument('test')
    ap.add_argument('args', nargs=argparse.REMAINDER)
    a = ap.parse_args()
    args = a.args or ['/ini=nul', '{port}', '{protocol}']
    # A Windows program under Wine is given {tmp} as a Windows path: one
    # without a drive would resolve on whichever drive it is on.
    tmp = tempfile.mkdtemp(prefix='linscp-qt-session-')
    try:
        root = os.path.join(tmp, 'root')
        os.makedirs(os.path.join(root, 'sub'))
        with open(os.path.join(root, 'hello.txt'), 'wb') as f:
            f.write(b'Hello, linscp\n')
        if any('{key}' in x for x in args) and (a.protocol != 'sshd'):
            if not shutil.which('ssh-keygen'):
                raise fixtures.Missing('ssh-keygen')
            fixtures.keygen(os.path.join(tmp, 'id_ed25519'))
        os.makedirs(os.path.join(tmp, 'local'))
        key = os.path.join(tmp, 'id_ed25519')
        if a.protocol == 'sshd':
            # Windows' sshd lets no one in run unprivileged, as here.
            if os.name == 'nt':
                raise fixtures.Missing('sshd (not run on Windows)')
            os.makedirs(os.path.join(tmp, 'sshd'))
            server = fixtures.sshd(os.path.join(tmp, 'sshd'))
        elif a.protocol == 'sftp':
            server = fixtures.rclone_sftp(root, 'linscp', 'secret', os.path.join(tmp, 'keys'))
        else:
            server = fixtures.python_server('ftp_server.py', root)
        with server as served:
            port = served
            if a.protocol == 'sshd':
                port, key, _ = served
            target_tmp = tmp
            if a.emulator and os.path.basename(a.emulator).startswith('wine'):
                target_tmp = subprocess.run(['winepath', '-w', tmp], capture_output=True, text=True,
                                            env=dict(os.environ, WINEDEBUG='-all')).stdout.strip() or tmp
            cmd = ([a.emulator] if a.emulator else []) + [a.test] + [
                x.replace('{port}', str(port)).replace('{protocol}', a.protocol).replace('{tmp}', target_tmp).replace('{hosttmp}', tmp)
                .replace('{key}', key).replace('{user}', getpass.getuser()) for x in args]
            p = subprocess.run(cmd, capture_output=True, text=True, timeout=240)
            print(p.stdout, end='')
            print(p.stderr[-2000:], end='', file=sys.stderr)
            return p.returncode
    except fixtures.Missing as e:
        print(f'SKIP: {e}')
        return 77
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())
