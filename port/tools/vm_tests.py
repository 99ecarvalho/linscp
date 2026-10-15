#!/usr/bin/env python3
"""
Run the Windows build's tests natively on the Windows test VM
(doc/qt/windows-vm-hints.md): the tests ctest knows for the cross build,
their commands made Windows ones, over SSH.

  LINSCP_VM_PASSWORD=... vm_tests.py [--build build/win] [--host 192.168.0.33]
                                     [--user user] [-R regex] [-j 1]

Copies, skipping files whose size and time match:
  build/win/deploy, the test programs and Qt6Test.dll -> C:\\temp\\linscp-test
  port/tests                                          -> C:\\temp\\linscp\\port\\tests
then runs each test as ctest would, but natively: wine dropped, the build's
programs and scripts at their VM paths, Python and rclone from C:\\temp
(see the hints for what the VM has), and Qt's windows platform instead of
offscreen (the deploy folder has only that one). A test that exits 77
skipped, as in ctest. Needs paramiko.
"""
import argparse, json, os, posixpath, re, subprocess, sys, time

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
TEST_DIR = 'C:\\temp\\linscp-test'
TESTS_DIR = 'C:\\temp\\linscp\\port\\tests'
PYTHON = 'C:\\temp\\python\\python.exe'
HOST_ONLY = {'win32_on_posix'}
WORK_DIR = 'C:\\temp\\linscp-test\\work'
WINSCP_DIR = 'C:\\temp\\linscp\\winscp-source'


def connect(host, user, password):
    import paramiko
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(host, username=user, password=password, timeout=20, allow_agent=False, look_for_keys=False)
    return client


def push(sftp, local, remote):
    """local (a file or a tree) to remote (a Windows path), unchanged files skipped."""
    remote = remote.replace('\\', '/')

    def mkdirs(path):
        parts = path.split('/')
        current = parts[0]
        for part in parts[1:]:
            current += '/' + part
            try:
                sftp.stat(current)
            except IOError:
                sftp.mkdir(current)

    def put(source, target):
        st = os.stat(source)
        try:
            r = sftp.stat(target)
            if r.st_size == st.st_size and int(r.st_mtime) == int(st.st_mtime):
                return 0
        except IOError:
            pass
        sftp.put(source, target)
        sftp.utime(target, (st.st_atime, st.st_mtime))
        return 1

    copied = 0
    if os.path.isfile(local):
        mkdirs(posixpath.dirname(remote))
        return put(local, remote)
    for directory, dirs, files in os.walk(local):
        dirs[:] = [d for d in dirs if d != '__pycache__']
        relative = os.path.relpath(directory, local).replace('\\', '/')
        target = remote if relative == '.' else remote + '/' + relative
        mkdirs(target)
        for name in files:
            copied += put(os.path.join(directory, name), target + '/' + name)
    return copied


def translate(command, build):
    """A ctest command for the cross build, as the VM runs it; None for one
    that is not a program's or a script's (builds, checks of the tree)."""
    tests = os.path.join(ROOT, 'port', 'tests')
    out = []
    skip_next = False
    for index, arg in enumerate(command):
        if skip_next:
            skip_next = False
            continue
        if os.path.basename(arg).startswith('wine') and index == 0:
            continue
        if arg == '--emulator':
            skip_next = True
            continue
        if arg.endswith('python3') or os.path.basename(arg).startswith('python3'):
            out.append(PYTHON)
            continue
        if arg.startswith(tests + os.sep) and arg.endswith('.py'):
            out.append(TESTS_DIR + '\\' + os.path.relpath(arg, tests).replace('/', '\\'))
            continue
        if arg.startswith(os.path.abspath(build) + os.sep) and arg.endswith('.exe'):
            out.append(TEST_DIR + '\\' + os.path.basename(arg))
            continue
        # The build's other paths, as the VM has them: the translations the
        # deploy folder carries, a work directory, WinSCP's forms.
        if arg == os.path.join(os.path.abspath(build), 'port', 'qt', 'Translations'):
            out.append(TEST_DIR + '\\Translations')
            continue
        if arg == os.path.join(os.path.abspath(build), 'port', 'tests'):
            out.append(WORK_DIR)
            continue
        if arg == os.path.join(ROOT, '3rd-party', 'winscp', 'source'):
            out.append(WINSCP_DIR)
            continue
        if arg.startswith(ROOT + os.sep):
            return None
        if arg == 'offscreen' and out and out[-1] == '-platform':
            out.append('windows')
            continue
        out.append(arg)
    if not out or not (out[0] == PYTHON or out[0].startswith(TEST_DIR)):
        return None
    # Of the scripts, those that run the build's programs (the others check
    # the source tree, which the VM does not have).
    if (out[0] == PYTHON) and not ((len(out) > 1) and out[1].startswith(TESTS_DIR) and
                                   re.match(r'(run_|check_menus)', out[1].rsplit('\\', 1)[-1])):
        return None
    return out


def quote(arg):
    return '"' + arg + '"' if (' ' in arg or '{' in arg or '|' in arg or '&' in arg) else arg


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--build', default=os.path.join(ROOT, 'build', 'win'))
    ap.add_argument('--host', default='192.168.0.33')
    ap.add_argument('--user', default='user')
    ap.add_argument('-R', dest='regex', default='')
    ap.add_argument('--no-push', action='store_true')
    ap.add_argument('-V', dest='verbose', action='store_true', help="each test's output, not only a failure's")
    a = ap.parse_args()
    password = os.environ.get('LINSCP_VM_PASSWORD')
    if not password:
        sys.exit('set LINSCP_VM_PASSWORD')
    listing = json.loads(subprocess.run(['ctest', '--test-dir', a.build, '--show-only=json-v1'],
                                        check=True, capture_output=True, text=True).stdout)
    tests = []
    for test in listing['tests']:
        if a.regex and not re.search(a.regex, test['name']):
            continue
        # The POSIX layer's own test compares it with the real API (Wine's):
        # on Windows there is no POSIX layer to test.
        if test['name'] in HOST_ONLY:
            continue
        command = translate(test.get('command', []), a.build)
        if command is not None:
            skip = None
            for prop in test.get('properties', []):
                if prop['name'] == 'SKIP_RETURN_CODE':
                    skip = int(prop['value'])
            tests.append((test['name'], command, skip))
    client = connect(a.host, a.user, password)
    if not a.no_push:
        sftp = client.open_sftp()
        copied = push(sftp, os.path.join(a.build, 'deploy'), TEST_DIR)
        # Every test program the commands name, wherever in the build it is.
        for _, command, _ in tests:
            for arg in command:
                if arg.startswith(TEST_DIR + '\\') and arg.endswith('.exe'):
                    name = arg.rsplit('\\', 1)[-1]
                    for directory, _, files in os.walk(a.build):
                        if name in files and 'deploy' not in directory:
                            copied += push(sftp, os.path.join(directory, name), arg)
                            break
        mkdir = WORK_DIR.replace('\\', '/')
        try:
            sftp.stat(mkdir)
        except IOError:
            sftp.mkdir(mkdir)
        for form in ('NonVisual.dfm', 'ScpCommander.dfm'):
            copied += push(sftp, os.path.join(ROOT, '3rd-party', 'winscp', 'source', 'forms', form),
                           WINSCP_DIR + '\\forms\\' + form)
        qt_test = os.path.join(a.build, '..', 'tools', 'msys2-qt', 'clang64', 'bin', 'Qt6Test.dll')
        if os.path.exists(qt_test):
            copied += push(sftp, qt_test, TEST_DIR + '\\Qt6Test.dll')
        copied += push(sftp, os.path.join(ROOT, 'port', 'tests'), TESTS_DIR)
        sftp.close()
        print(f'{copied} files copied to the VM')
    passed, failed, skipped = [], [], []
    for name, command, skip in tests:
        # (Quoted: cmd keeps the space before && in a plain set's value.)
        line = ('set "PATH=C:\\temp;' + TEST_DIR + ';%PATH%"&& set "LINSCP_QT_PLATFORM=windows"&& ' +
                ' '.join(quote(x) for x in command))
        start = time.monotonic()
        _, stdout, stderr = client.exec_command(line, timeout=900)
        out = stdout.read().decode(errors='replace') + stderr.read().decode(errors='replace')
        rc = stdout.channel.recv_exit_status()
        took = time.monotonic() - start
        if rc == 0:
            passed.append(name)
            verdict = 'Passed'
        elif skip is not None and rc == skip:
            skipped.append(name)
            verdict = 'Skipped'
        else:
            failed.append(name)
            verdict = f'Failed (rc {rc})'
        print(f'{name:40} {verdict} {took:.1f} s', flush=True)
        if verdict.startswith('Failed') or a.verbose:
            lines = out.strip().splitlines()
            print('  ' + '\n  '.join(lines if a.verbose else lines[-25:]))
    client.close()
    print(f'{len(passed)} passed, {len(failed)} failed, {len(skipped)} skipped of {len(tests)}')
    if failed:
        print('failed: ' + ', '.join(failed))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
