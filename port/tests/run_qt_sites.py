#!/usr/bin/env python3
"""
test_qt_sites, twice over one INI file -- save, then check in a new process --
with rclone's SFTP server on loopback for the stored site to open; then its
keys phase against OpenSSH's sshd (key authentication) and its proxy phase
through microsocks (SOCKS5 with a password), each when that server is
installed, and said when it is not.

  run_qt_sites.py [--emulator wine] test_qt_sites[.exe]

Exits 77 without rclone.
"""
import argparse, getpass, os, shutil, subprocess, sys, tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fixtures  # noqa: E402


def run(a, *args):
    cmd = ([a.emulator] if a.emulator else []) + [a.test] + [str(x) for x in args]
    p = subprocess.run(cmd, capture_output=True, text=True, timeout=240)
    print(p.stdout, end='')
    if p.returncode != 0:
        print(p.stderr[-2000:], end='', file=sys.stderr)
    return p.returncode


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--emulator', default='')
    ap.add_argument('test')
    a = ap.parse_args()
    tmp = tempfile.mkdtemp(prefix='linscp-qt-sites-')
    try:
        root = os.path.join(tmp, 'root')
        os.makedirs(os.path.join(root, 'sub'))
        ini = os.path.join(tmp, 'WinSCP.ini')
        with fixtures.rclone_sftp(root, 'linscp', 'secret', os.path.join(tmp, 'keys')) as port:
            for phase in ('save', 'check'):
                print(f'{phase}:')
                rc = run(a, f'/ini={ini}', phase, port)
                if rc != 0:
                    return rc
            print('proxy:')
            try:
                with fixtures.socks5_proxy('proxyuser', 'proxypass', os.path.join(tmp, 'socks.log')) as proxy:
                    rc = run(a, '/ini=nul', 'proxy', port, proxy)
                if rc != 0:
                    return rc
                if os.path.getsize(os.path.join(tmp, 'socks.log')) == 0:
                    print('FAIL the proxy saw no connection')
                    return 1
            except fixtures.Missing as e:
                print(f'  skipped: no {e}')
        print('keys:')
        try:
            sshd_dir = os.path.join(tmp, 'sshd')
            os.makedirs(sshd_dir)
            with fixtures.sshd(sshd_dir) as (port, key, _):
                rc = run(a, '/ini=nul', 'keys', port, key, getpass.getuser(), tmp)
                if rc != 0:
                    return rc
        except fixtures.Missing as e:
            print(f'  skipped: no {e}')
        return 0
    except fixtures.Missing as e:
        print(f'SKIP: {e}')
        return 77
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())
