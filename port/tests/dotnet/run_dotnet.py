#!/usr/bin/env python3
"""
WinSCP's .NET assembly driving a WinSCP-compatible executable, over SFTP,
FTP, FTPES, WebDAV and S3 against the suite's loopback servers.

  run_dotnet.py --dotnet dotnet.exe --winscpnet WinSCPnet.dll --client linscp-cli.exe

The stock assembly speaks winscp.com's protocol through named events and
file mappings, which .NET provides only on Windows; elsewhere it needs the
assembly with console pipes (ADR 0010, offered upstream). WinSCPnet.dll
must be built from the same WinSCP source as the executable -- on Windows
the assembly refuses an executable of another product version.
"""
import argparse, os, shutil, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
sys.path.insert(0, os.path.join(os.path.dirname(HERE), 'golden'))
import fixtures  # noqa: E402

CERT = os.path.join(os.path.dirname(HERE), 'golden', 'data', 'test-server.crt')
KEY = os.path.join(os.path.dirname(HERE), 'golden', 'data', 'test-server.key')


def seed(root):
    os.makedirs(os.path.join(root, 'work', 'sub'), exist_ok=True)
    with open(os.path.join(root, 'work', 'hello.txt'), 'wb') as f:
        f.write(b'Hello, linscp\n')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dotnet', required=True)
    ap.add_argument('--winscpnet', required=True)
    ap.add_argument('--client', required=True)
    a = ap.parse_args()
    for path, what in ((a.dotnet, 'dotnet (port/tools/fetch_dotnet.py)'), (a.winscpnet, 'WinSCPnet.dll')):
        if not os.path.isfile(path):
            print(f'SKIP: no {what} at {path}')
            return 77
    tmp = tempfile.mkdtemp(prefix='linscp-dotnet-')
    failures = 0
    try:
        # Built from a copy, so that the build's obj/ stays out of the source tree.
        project = os.path.join(tmp, 'project')
        os.makedirs(project)
        for name in ('DotnetSession.csproj', 'Program.cs'):
            shutil.copy(os.path.join(HERE, name), project)
        build = subprocess.run([a.dotnet, 'build', os.path.join(project, 'DotnetSession.csproj'), '-c', 'Release',
                                '-o', os.path.join(tmp, 'bin'), f'-p:WinSCPnetPath={os.path.abspath(a.winscpnet)}'],
                               capture_output=True, text=True)
        if build.returncode != 0:
            print(build.stdout[-3000:])
            return 1
        program = os.path.join(tmp, 'bin', 'DotnetSession.dll')
        cases = [
            ('sftp', lambda root: fixtures.rclone_sftp(root, 'linscp', 'secret', os.path.join(tmp, 'keys')), '/work'),
            ('ftp', lambda root: fixtures.python_server('ftp_server.py', root), '/work'),
            ('ftpes', lambda root: fixtures.python_server('ftp_server.py', root, '--cert', CERT, '--key', KEY), '/work'),
            ('webdav', lambda root: fixtures.rclone_webdav(root, 'linscp', 'secret'), '/work'),
            ('s3', lambda root: fixtures.python_server('s3_server.py', root), '/bucket/work'),
        ]
        for proto, server, remote in cases:
            root = os.path.join(tmp, proto + '-server')
            seed(os.path.join(root, 'bucket') if proto == 's3' else root)
            local = os.path.join(tmp, proto + '-local')
            os.makedirs(local)
            try:
                with server(root) as port:
                    p = subprocess.run([a.dotnet, program, a.client, proto, str(port), remote, local],
                                       capture_output=True, text=True, timeout=300)
                    ok = p.returncode == 0
                    print(f'{proto}: {"OK" if ok else "FAILED"}')
                    if not ok:
                        failures += 1
                        print('  ' + (p.stdout + p.stderr).strip().replace('\n', '\n  ')[-4000:])
            except fixtures.Missing as e:
                print(f'{proto}: skipped ({e})')
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
