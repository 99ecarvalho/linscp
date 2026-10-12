#!/usr/bin/env python3
"""
winscp.com's console protocol over /consolepipes, with this script in the
caller's place -- what WinSCP's .NET assembly does where it has no named
events or file mappings (ExternalConsole.cpp): it writes the initialised
structure, then answers each request the client writes with the structure
read back.

  run_console_pipes.py linscp-cli

Checked: the handshake (the client confirms the version and reports its
options in INIT), a script's output arriving as PRINT events, an
interactive INPUT answered by the caller, a structure of another size
refused, and the client exiting when the caller goes away mid-request
rather than hanging. POSIX only.
"""
import os, struct, subprocess, sys, time

failures = 0

# console/Console.h's TConsoleCommStruct: the header, then the largest
# member of the union (TInputEvent, TTransferEvent: 20488 bytes).
SIZE = 12 + 20488
VERSION, VERSION_CONFIRMED = 0x000A, 0x010A
NONE, PRINT, INPUT, CHOICE, TITLE, INIT, PROGRESS, TRANSFEROUT, TRANSFERIN = range(9)
FILE_TYPE_PIPE = 3


def check(cond, what, detail=''):
    global failures
    if not cond:
        failures += 1
        print(f'FAIL {what}')
        if detail:
            print('  ' + str(detail).strip().replace('\n', '\n  '))


def wide(data, offset, chars):
    raw = data[offset:offset + chars * 2]
    text = raw.decode('utf-16-le')
    return text.split('\0', 1)[0]


def put_wide(data, offset, text):
    raw = (text + '\0').encode('utf-16-le')
    data[offset:offset + len(raw)] = raw


class Caller:
    """The caller's ends of the two pipes, and the client started on the others."""

    def __init__(self, cli, args, size=SIZE):
        self.size = size
        to_client_r, self.to_client = os.pipe()
        self.from_client, from_client_w = os.pipe()
        self.client = subprocess.Popen(
            [cli, '/ini=nul', '/nointeractiveinput', '/stdout', '/stdin', '/console',
             f'/consolepipes={to_client_r},{from_client_w}'] + args,
            pass_fds=(to_client_r, from_client_w), stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        # The client holds the only copies of its ends.
        os.close(to_client_r)
        os.close(from_client_w)
        hello = bytearray(size)
        struct.pack_into('<IiI', hello, 0, size, VERSION, NONE)
        self.write(hello)

    def write(self, frame):
        try:
            os.write(self.to_client, bytes(frame))
        except BrokenPipeError:
            pass

    def read(self):
        """The next request, or None at the end of the pipe."""
        data = bytearray()
        while len(data) < SIZE:
            chunk = os.read(self.from_client, SIZE - len(data))
            if not chunk:
                return None
            data += chunk
        return data

    def close(self):
        for fd in (self.to_client, self.from_client):
            try:
                os.close(fd)
            except OSError:
                pass

    def wait(self, timeout=30):
        try:
            rc = self.client.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            self.client.kill()
            rc = None
        out = self.client.stdout.read().decode(errors='replace')
        return rc, out


def converse(caller, inputs=()):
    """Answers requests as the assembly does until the client is done.
    Returns the requests' versions, the printed lines and the INIT options."""
    inputs = list(inputs)
    versions, printed, init = [], [], None
    while True:
        frame = caller.read()
        if frame is None:
            break
        size, version, event = struct.unpack_from('<IiI', frame, 0)
        versions.append(version)
        if event == INIT:
            input_type, output_type, wants_progress, use_stderr, out_format, in_format = \
                struct.unpack_from('<II??xxii', frame, 12)
            init = (use_stderr, out_format, in_format)
            struct.pack_into('<II?', frame, 12, FILE_TYPE_PIPE, FILE_TYPE_PIPE, False)
        elif event == PRINT:
            printed.append(wide(frame, 12, 10240))
        elif event == INPUT:
            # TInputEvent: Echo, Result, Str[10240], Timer.
            if inputs:
                struct.pack_into('<?', frame, 13, True)
                put_wide(frame, 14, inputs.pop(0))
            else:
                struct.pack_into('<?', frame, 13, False)
        elif event == CHOICE:
            # TChoiceEvent: Options[64], Cancel, Break, Result: break.
            brk = struct.unpack_from('<i', frame, 12 + 128 + 4)[0]
            struct.pack_into('<i', frame, 12 + 128 + 8, brk)
        caller.write(frame)
    return versions, printed, init


def main():
    if os.name == 'nt':
        print('SKIP: /consolepipes is the POSIX transport')
        return 77
    cli = sys.argv[1]

    # A script by /command: its output comes back as PRINT events.
    caller = Caller(cli, ['/command', 'echo Hello through the pipes', 'exit'])
    versions, printed, init = converse(caller)
    caller.close()
    rc, out = caller.wait()
    check(rc == 0, '/command script exits 0', f'rc {rc}\n{out}')
    check(versions and all(v == VERSION_CONFIRMED for v in versions),
          'every request carries the confirmed version', versions)
    check(init == (True, 1, 1), 'INIT reports /stdout and /stdin, binary, messages on standard error', init)
    check(any('Hello through the pipes' in p for p in printed), 'the echo arrives as a PRINT event', printed)

    # Commands read through INPUT events, as the assembly feeds them.
    caller = Caller(cli, [])
    versions, printed, init = converse(caller, ['echo first', 'echo second', 'exit'])
    caller.close()
    rc, out = caller.wait()
    check(rc == 0, 'commands given by INPUT run, and exit ends the client', f'rc {rc}\n{out}')
    text = '\n'.join(printed)
    check('first' in text and 'second' in text and text.index('first') < text.index('second'),
          'INPUT commands run in order', printed)

    # A structure of another size is refused before any request.
    caller = Caller(cli, ['/command', 'exit'], size=SIZE - 4)
    first = caller.read()
    caller.close()
    rc, out = caller.wait()
    check(first is None, 'a structure of another size gets no request')
    check(rc not in (0, None), 'and the client fails', f'rc {rc}\n{out}')

    # The caller going away mid-request: the client must notice and exit.
    caller = Caller(cli, [])
    seen = []
    while True:
        frame = caller.read()
        if frame is None:
            break
        event = struct.unpack_from('<I', frame, 8)[0]
        seen.append(event)
        if event == INPUT:
            break  # gone without answering
        if event == INIT:
            struct.pack_into('<II?', frame, 12, FILE_TYPE_PIPE, FILE_TYPE_PIPE, False)
        caller.write(frame)
    started = time.monotonic()
    caller.close()
    rc, out = caller.wait(timeout=20)
    check(INPUT in seen, 'the client asks for a command', seen)
    check(rc is not None, 'a client whose caller is gone exits', out)
    check(rc not in (0, None), 'with an error', f'rc {rc}\n{out}')
    check(time.monotonic() - started < 10, 'promptly')

    if failures == 0:
        print('OK')
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
