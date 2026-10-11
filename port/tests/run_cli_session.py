#!/usr/bin/env python3
"""
Drive linscp-cli against a throwaway OpenSSH server on loopback, as a user's
script would: convert the client key with /keygen, then run scripts given
with /command, on standard input and with /stdout, checking output, exit
codes and the files transferred. The server is run_sftp_session.py's.

  run_cli_session.py [--emulator wine] linscp-cli[.exe]

Exits 77 (ctest's skip code here) when no sshd is installed.
"""
import argparse, getpass, os, shutil, socket, subprocess, sys, tempfile, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from run_sftp_session import SKIP, fingerprint, free_port, keygen  # noqa: E402

failures = 0


def check(cond, what, detail=''):
    global failures
    if not cond:
        failures += 1
        print(f'FAIL {what}')
        if detail:
            print('  ' + detail.replace('\n', '\n  '))


def run_on_terminal(cli, lines):
    """Runs cli on a pseudo-terminal, typing each line at its prompt."""
    import pty, select
    pid, fd = pty.fork()
    if pid == 0:
        os.execv(cli, [cli, '/ini=nul'])
    out = b''

    def read_until(marker, timeout=10):
        nonlocal out
        end = time.monotonic() + timeout
        while time.monotonic() < end and marker not in out:
            ready, _, _ = select.select([fd], [], [], 0.1)
            if ready:
                try:
                    out += os.read(fd, 4096)
                except OSError:
                    return

    for line in lines:
        read_until(b'linscp> ')
        os.write(fd, line.encode() + b'\n')
    # Until the client exits and the terminal closes (EIO).
    read_until(b'\x00', 10)
    _, status = os.waitpid(pid, 0)
    return out.decode(errors='replace'), os.waitstatus_to_exitcode(status)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--emulator')
    ap.add_argument('cli')
    a = ap.parse_args()

    sshd = shutil.which('sshd') or '/usr/sbin/sshd'
    if not os.path.isfile(sshd) or not shutil.which('ssh-keygen'):
        print('SKIP: no sshd')
        return SKIP

    def native(path):
        return 'Z:' + path.replace('/', '\\') if a.emulator else path

    def run(*args, stdin=None, ini='nul', env=None):
        cmd = ([a.emulator] if a.emulator else []) + [a.cli] + ([f'/ini={ini}'] if ini else []) + list(args)
        p = subprocess.run(cmd, input=stdin, capture_output=True, timeout=120,
                           env=dict(os.environ, **(env or {})))
        return p.returncode, p.stdout, p.stderr

    tmp = tempfile.mkdtemp(prefix='linscp-cli-')
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
                'LogLevel ERROR', '']))
        server = subprocess.Popen([sshd, '-D', '-e', '-f', config])
        deadline = time.monotonic() + 10
        while True:
            try:
                socket.create_connection(('127.0.0.1', port), timeout=1).close()
                break
            except OSError:
                if server.poll() is not None or time.monotonic() > deadline:
                    print('FAIL: sshd did not start')
                    return 1
                time.sleep(0.1)

        # /keygen: the OpenSSH key to PuTTY's format, as core needs it.
        ppk = os.path.join(tmp, 'client.ppk')
        rc, out, err = run('/keygen', native(os.path.join(tmp, 'client')), '/output=' + native(ppk))
        check(rc == 0 and os.path.isfile(ppk), f'/keygen (rc {rc})', (out + err).decode(errors='replace'))
        check(b'Key saved to' in out, '/keygen reports the saved key', out.decode(errors='replace'))

        # The host key as its bare SHA-256 hash and the paths unquoted: on
        # Windows the command line is rebuilt from argv with backslash-escaped
        # quotes, which WinSCP's parser (doubled quotes) does not read, there
        # as with winscp.com. Quoting is checked on POSIX below.
        full_fingerprint = fingerprint(os.path.join(tmp, 'host.pub'))
        url = f'sftp://{getpass.getuser()}@127.0.0.1:{port}/'
        open_cmd = f'open {url} -privatekey={native(ppk)} -hostkey={full_fingerprint.split()[-1]}'
        remote_hello = remote + '/hello.txt'
        # A trailing separator: download into the directory.
        local_sep = '\\' if a.emulator else '/'

        # A session: list, download, exit.
        rc, out, err = run('/command', open_cmd, f'ls {remote}',
                           f'get {remote_hello} {native(local)}{local_sep}',
                           'exit')
        text = out.decode(errors='replace')
        check(rc == 0, f'session exits 0 (rc {rc})', text + err.decode(errors='replace'))
        check('hello.txt' in text and 'sub' in text, 'ls lists the directory', text)
        downloaded = os.path.join(local, 'hello.txt')
        check(os.path.isfile(downloaded) and open(downloaded, 'rb').read() == b'Hello, linscp\n',
              'get downloads the file')

        # A failing command: the error is reported and the exit code is 1,
        # and in batch abort mode the rest of the script does not run.
        rc, out, err = run('/command', 'option batch abort', open_cmd, f'get {remote}/missing.txt {native(local)}{local_sep}',
                           f'ls {remote}', 'exit')
        text = out.decode(errors='replace')
        check(rc == 1, f'a failed command exits 1 (rc {rc})', text)
        check('missing.txt' in text, 'the failure names the file', text)
        check('hello.txt' not in text, 'batch abort stops the script', text)

        # Commands on standard input, echoed as winscp.com echoes them.
        rc, out, err = run(stdin=f'{open_cmd}\nls "{remote}"\nexit\n'.encode())
        text = out.decode(errors='replace')
        check(rc == 0, f'piped commands exit 0 (rc {rc})', text + err.decode(errors='replace'))
        check(f'ls "{remote}"' in text and 'hello.txt' in text, 'piped commands run and are echoed', text)

        # /stdout: the download on standard output, messages on standard error.
        rc, out, err = run('/stdout', '/command', open_cmd, f'get {remote_hello} -', 'exit')
        check(rc == 0, f'/stdout exits 0 (rc {rc})', err.decode(errors='replace'))
        check(out == b'Hello, linscp\n', '/stdout carries exactly the file', repr(out))
        check(b'hello.txt' in err or b'Active session' in err or len(err) > 0, '/stdout moves messages to stderr',
              err.decode(errors='replace'))

        if not a.emulator:
            # Arguments with spaces and quotes reach the script intact.
            quoted = f'open {url} -privatekey="{ppk}" -hostkey="{full_fingerprint}"'
            rc, out, err = run('/command', quoted, f'ls "{remote}"', 'exit')
            text = out.decode(errors='replace')
            check(rc == 0 and 'hello.txt' in text, f'quoted arguments survive the command line (rc {rc})', text)

        if not a.emulator:
            # On a terminal: the prompt, a command, and a clean exit.
            text, rc = run_on_terminal(a.cli, ['help ls', 'exit'])
            check(rc == 0, f'terminal session exits 0 (rc {rc})', text)
            check(text.count('linscp> ') >= 2 and 'Lists the contents' in text, 'terminal prompt and command', text)

        # A site stored in WinSCP's INI, opened by name. On POSIX the file is
        # found where linscp keeps it ($XDG_CONFIG_HOME/linscp/WinSCP.ini),
        # on Windows given with /ini (there the default is the registry).
        config_home = os.path.join(tmp, 'config')
        if a.emulator:
            ini_path = os.path.join(tmp, 'WinSCP.ini')
            ini_arg, ini_env = native(ini_path), None
        else:
            ini_path = os.path.join(config_home, 'linscp', 'WinSCP.ini')
            ini_arg, ini_env = None, {'XDG_CONFIG_HOME': config_home}
        os.makedirs(os.path.dirname(ini_path), exist_ok=True)
        with open(ini_path, 'w') as f:
            f.write('[Sessions\\linscp-test]\n'
                    'HostName=127.0.0.1\n'
                    f'PortNumber={port}\n'
                    f'UserName={getpass.getuser()}\n'
                    f'PublicKeyFile={native(ppk)}\n')
        rc, out, err = run('/command', f'open linscp-test -hostkey={full_fingerprint.split()[-1]}',
                           f'ls {remote}', 'exit', ini=ini_arg, env=ini_env)
        text = out.decode(errors='replace')
        check(rc == 0 and 'hello.txt' in text, f'a stored site opens by name (rc {rc})', text + err.decode(errors='replace'))
        stored = open(ini_path, encoding='utf-8', errors='replace').read()
        check('[Sessions\\linscp-test]' in stored and 'HostName=127.0.0.1' in stored,
              'the stored site survives the configuration being saved', stored[:2000])

        # A script file with /parameter.
        script = os.path.join(tmp, 'script.txt')
        with open(script, 'w') as f:
            f.write(f'{open_cmd}\nls "%1%"\nexit\n')
        rc, out, err = run(f'/script={native(script)}', '/parameter', remote)
        text = out.decode(errors='replace')
        check(rc == 0 and 'hello.txt' in text, f'/script with /parameter (rc {rc})', text)
    finally:
        if server is not None:
            server.terminate()
            server.wait(timeout=10)
        shutil.rmtree(tmp, ignore_errors=True)

    if failures == 0:
        print('OK')
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
