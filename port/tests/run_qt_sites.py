#!/usr/bin/env python3
"""
test_qt_sites, twice over one INI file -- save, then check in a new process --
with rclone's SFTP server on loopback for the stored site to open.

  run_qt_sites.py [--emulator wine] test_qt_sites[.exe]

Exits 77 without rclone.
"""
import argparse, os, shutil, subprocess, sys, tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fixtures  # noqa: E402


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
                cmd = ([a.emulator] if a.emulator else []) + [a.test, f'/ini={ini}', phase, str(port)]
                p = subprocess.run(cmd, capture_output=True, text=True, timeout=240)
                print(f'{phase}:')
                print(p.stdout, end='')
                if p.returncode != 0:
                    print(p.stderr[-2000:], end='', file=sys.stderr)
                    return p.returncode
        return 0
    except fixtures.Missing as e:
        print(f'SKIP: {e}')
        return 77
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())
