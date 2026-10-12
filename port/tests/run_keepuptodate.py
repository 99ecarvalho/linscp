#!/usr/bin/env python3
"""
keepuptodate through linscp-cli against a throwaway sshd on loopback: while
the client watches, local changes are made and each must reach the server
-- a new file, a new directory with a file in it, a changed file, a
deletion (-delete) -- while a directory the file mask excludes must not.
Then Ctrl+C ends the watching, as a user would.

  run_keepuptodate.py linscp-cli

On POSIX the server is OpenSSH's sshd and Ctrl+C is delivered as SIGINT.
On Windows (rclone's SFTP server, password login) a script cannot press
Ctrl+C in another process's console, so the client is terminated instead
and only the synchronization is checked. Exits 77 without a server.
"""
import argparse, getpass, os, queue, shutil, signal, subprocess, sys, tempfile, threading, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fixtures  # noqa: E402

SKIP = 77
failures = 0


def check(cond, what, detail=''):
    global failures
    if not cond:
        failures += 1
        print(f'FAIL {what}')
        if detail:
            print('  ' + detail.strip().replace('\n', '\n  '))


def wait_for(predicate, timeout=20):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if predicate():
            return True
        time.sleep(0.1)
    return False


def read_file(path):
    try:
        with open(path, 'rb') as f:
            return f.read()
    except OSError:
        return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('cli')
    a = ap.parse_args()
    windows = os.name == 'nt'
    tmp = tempfile.mkdtemp(prefix='linscp-keepuptodate-')
    try:
        server = (fixtures.rclone_sftp(os.path.join(tmp, 'remote'), 'linscp', 'secret', os.path.join(tmp, 'keys'))
                  if windows else fixtures.sshd(tmp))
        os.makedirs(os.path.join(tmp, 'remote'))
        with server as started:
            if windows:
                port = started
                open_cmd = f'open sftp://linscp:secret@127.0.0.1:{port}/ -hostkey=* -rawsettings TryAgent=0'
                remote_path = '/'
            else:
                port, key, host_key = started
                ppk = os.path.join(tmp, 'client.ppk')
                subprocess.run([a.cli, '/ini=nul', '/keygen', key, '/output=' + ppk], capture_output=True, check=True)
                open_cmd = (f'open sftp://{getpass.getuser()}@127.0.0.1:{port}/ -privatekey={ppk} '
                            f'-hostkey={host_key.split()[-1]}')
                remote_path = os.path.join(tmp, 'remote')
            local = os.path.join(tmp, 'local')
            remote = os.path.join(tmp, 'remote')
            os.makedirs(os.path.join(local, 'existing'))
            os.makedirs(os.path.join(local, 'excluded'))
            with open(os.path.join(local, 'doomed.txt'), 'wb') as f:
                f.write(b'to be deleted\n')
            shutil.copy(os.path.join(local, 'doomed.txt'), os.path.join(remote, 'doomed.txt'))

            cmd = [a.cli, '/ini=nul', '/command', 'option batch abort', 'option confirm off',
                   open_cmd, f'keepuptodate -delete -filemask="|excluded/" {local} {remote_path}', 'exit']
            client = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            lines = queue.Queue()
            output = []
            threading.Thread(target=lambda: [lines.put(l) for l in iter(client.stdout.readline, b'')],
                             daemon=True).start()

            def collect():
                while not lines.empty():
                    output.append(lines.get().decode(errors='replace'))
                return ''.join(output)

            check(wait_for(lambda: 'Watching for changes in ' in collect()), 'keepuptodate starts watching', collect())
            # The excluded directory is not counted among the watched ones.
            check('Watching for changes in 2 directories' in collect(), 'the watch covers local/ and existing/',
                  collect())

            with open(os.path.join(local, 'new.txt'), 'wb') as f:
                f.write(b'new\n')
            check(wait_for(lambda: read_file(os.path.join(remote, 'new.txt')) == b'new\n'),
                  'a new file is uploaded', collect())

            os.makedirs(os.path.join(local, 'added'))
            with open(os.path.join(local, 'added', 'inner.txt'), 'wb') as f:
                f.write(b'inner\n')
            check(wait_for(lambda: read_file(os.path.join(remote, 'added', 'inner.txt')) == b'inner\n'),
                  'a new directory and the file in it are uploaded', collect())

            time.sleep(1.1)  # a later modification time
            with open(os.path.join(local, 'new.txt'), 'wb') as f:
                f.write(b'changed\n')
            check(wait_for(lambda: read_file(os.path.join(remote, 'new.txt')) == b'changed\n'),
                  'a changed file is uploaded again', collect())

            os.remove(os.path.join(local, 'doomed.txt'))
            check(wait_for(lambda: not os.path.exists(os.path.join(remote, 'doomed.txt'))),
                  'a deleted file is deleted remotely (-delete)', collect())

            with open(os.path.join(local, 'excluded', 'secret.txt'), 'wb') as f:
                f.write(b'stays local\n')
            time.sleep(2)
            check(not os.path.exists(os.path.join(remote, 'excluded')), 'an excluded directory stays local', collect())

            if windows:
                client.terminate()
                client.wait(timeout=20)
            else:
                client.send_signal(signal.SIGINT)
                try:
                    rc = client.wait(timeout=20)
                except subprocess.TimeoutExpired:
                    client.kill()
                    rc = None
                text = collect()
                check(rc is not None, 'Ctrl+C ends keepuptodate', text)
                check('Terminated by user' in text, 'and says so', text)
    except fixtures.Missing as e:
        print(f'SKIP: {e}')
        return SKIP
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    if failures == 0:
        print('OK')
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
