#!/usr/bin/env python3
"""
A small FTP server for the protocol tests, on the standard library alone.

Serves --root to one user (--user/--password) on 127.0.0.1:--port. Speaks RFC
959 with RFC 2389 FEAT, RFC 2428 EPSV, RFC 3659 MLSD/MLST/SIZE/MDTM/REST,
MFMT, UTF-8 paths, and, with --cert/--key, RFC 4217 FTPS: explicit (AUTH TLS,
PBSZ, PROT) or, with --implicit, implicit. Passive mode only. Paths are
confined to the root.

  ftp_server.py --port N --root DIR [--user U --password P]
                [--cert C --key K [--implicit]]
"""
import argparse, os, socket, ssl, stat, threading, time


def mlsx_facts(path, name):
    st = os.stat(path)
    kind = 'dir' if stat.S_ISDIR(st.st_mode) else 'file'
    if name in ('.', '..'):
        kind = 'cdir' if name == '.' else 'pdir'
    modify = time.strftime('%Y%m%d%H%M%S', time.gmtime(st.st_mtime))
    perm = 'flcdmpe' if stat.S_ISDIR(st.st_mode) else 'adfrw'
    return (f'type={kind};size={st.st_size};modify={modify};perm={perm};'
            f'UNIX.mode={stat.S_IMODE(st.st_mode):04o}; {name}')


def list_line(path, name):
    st = os.stat(path)
    mode = stat.filemode(st.st_mode)
    when = time.strftime('%b %d %H:%M', time.gmtime(st.st_mtime))
    return f'{mode} 1 owner group {st.st_size:>10} {when} {name}'


class Session(threading.Thread):
    def __init__(self, server, sock):
        super().__init__(daemon=True)
        self.server = server
        self.sock = sock
        self.cwd = '/'
        self.user = None
        self.logged_in = False
        self.passive = None
        self.protected = False
        self.rest = 0
        self.rename_from = None
        self.reader = None

    # --- plumbing -------------------------------------------------------
    def reply(self, text):
        self.sock.sendall((text + '\r\n').encode('utf-8'))

    def readline(self):
        line = self.reader.readline()
        if not line:
            return None
        return line.decode('utf-8', 'replace').rstrip('\r\n')

    def real(self, path):
        virtual = os.path.normpath(os.path.join(self.cwd, path or '.')).replace('\\', '/')
        if not virtual.startswith('/'):
            virtual = '/' + virtual
        return os.path.join(self.server.root, virtual.lstrip('/')), virtual

    def open_passive(self):
        if self.passive is not None:
            self.passive.close()
        self.passive = socket.socket()
        self.passive.bind(('127.0.0.1', 0))
        self.passive.listen(1)
        self.passive.settimeout(20)
        return self.passive.getsockname()[1]

    def data_connection(self):
        if self.passive is None:
            self.reply('425 Use PASV or EPSV first')
            return None
        try:
            conn, _ = self.passive.accept()
        finally:
            self.passive.close()
            self.passive = None
        return conn

    def secure(self, conn):
        # After the 150, as servers do: clients start TLS on the data
        # connection once they have it.
        if self.protected:
            conn = self.server.tls.wrap_socket(conn, server_side=True)
        return conn

    def close_data(self, conn):
        try:
            if isinstance(conn, ssl.SSLSocket):
                # Send close_notify, but do not wait long for the client's:
                # RFC 4217 does not require it, and blocking here would
                # hold back the 226 the client is waiting for.
                conn.settimeout(2)
                try:
                    conn = conn.unwrap()
                except (ssl.SSLError, OSError):
                    pass
            conn.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        conn.close()

    def send_listing(self, lines):
        conn = self.data_connection()
        if conn is None:
            return
        self.reply('150 Here comes the listing')
        conn = self.secure(conn)
        conn.sendall(''.join(l + '\r\n' for l in lines).encode('utf-8'))
        self.close_data(conn)
        self.reply('226 Listing sent')

    # --- the session ------------------------------------------------------
    def run(self):
        try:
            if self.server.implicit:
                self.sock = self.server.tls.wrap_socket(self.sock, server_side=True)
                self.protected = True
            self.reader = self.sock.makefile('rb')
            self.reply('220 linscp test FTP server')
            while True:
                line = self.readline()
                if line is None:
                    break
                verb, _, arg = line.partition(' ')
                handler = getattr(self, 'cmd_' + verb.upper(), None)
                if handler is None:
                    self.reply('502 Command not implemented')
                elif not self.logged_in and verb.upper() not in ('USER', 'PASS', 'AUTH', 'FEAT', 'SYST',
                                                                 'QUIT', 'PBSZ', 'PROT', 'OPTS', 'CLNT'):
                    self.reply('530 Please log in')
                elif handler(arg) == 'quit':
                    break
        except (OSError, ssl.SSLError):
            pass
        finally:
            try:
                self.sock.close()
            except OSError:
                pass

    def cmd_AUTH(self, arg):
        if self.server.tls is None or arg.upper() not in ('TLS', 'SSL'):
            self.reply('504 AUTH not supported')
            return
        self.reply('234 Proceed with negotiation')
        self.sock = self.server.tls.wrap_socket(self.sock, server_side=True)
        self.reader = self.sock.makefile('rb')

    def cmd_PBSZ(self, arg):
        self.reply('200 PBSZ=0')

    def cmd_PROT(self, arg):
        self.protected = arg.upper() == 'P'
        self.reply('200 Protection level set')

    def cmd_USER(self, arg):
        self.user = arg
        self.reply('331 Password required')

    def cmd_PASS(self, arg):
        if self.server.tls is not None and not self.server.implicit and not isinstance(self.sock, ssl.SSLSocket):
            self.reply('530 TLS required')
            return
        if self.user == self.server.user and arg == self.server.password:
            self.logged_in = True
            self.reply('230 Logged in')
        else:
            self.reply('530 Login incorrect')

    def cmd_SYST(self, arg):
        self.reply('215 UNIX Type: L8')

    def cmd_FEAT(self, arg):
        features = ['EPSV', 'MDTM', 'MFMT', 'MLST type*;size*;modify*;perm*;UNIX.mode*;', 'REST STREAM',
                    'SIZE', 'UTF8']
        if self.server.tls is not None:
            features += ['AUTH TLS', 'PBSZ', 'PROT']
        self.reply('211-Features:')
        for f in features:
            self.reply(' ' + f)
        self.reply('211 End')

    def cmd_OPTS(self, arg):
        self.reply('200 OK')

    def cmd_CLNT(self, arg):
        self.reply('200 Noted')

    def cmd_NOOP(self, arg):
        self.reply('200 OK')

    def cmd_QUIT(self, arg):
        self.reply('221 Bye')
        return 'quit'

    def cmd_PWD(self, arg):
        self.reply(f'257 "{self.cwd}" is the current directory')

    def cmd_CWD(self, arg):
        path, virtual = self.real(arg)
        if os.path.isdir(path):
            self.cwd = virtual
            self.reply('250 Directory changed')
        else:
            self.reply('550 No such directory')

    def cmd_CDUP(self, arg):
        return self.cmd_CWD('..')

    def cmd_TYPE(self, arg):
        self.reply('200 Type set')

    def cmd_MODE(self, arg):
        self.reply('200 Mode set' if arg.upper() == 'S' else '504 Only stream mode')

    def cmd_STRU(self, arg):
        self.reply('200 Structure set' if arg.upper() == 'F' else '504 Only file structure')

    def cmd_PASV(self, arg):
        port = self.open_passive()
        self.reply(f'227 Entering Passive Mode (127,0,0,1,{port >> 8},{port & 0xFF})')

    def cmd_EPSV(self, arg):
        port = self.open_passive()
        self.reply(f'229 Entering Extended Passive Mode (|||{port}|)')

    def listing_target(self, arg):
        # Options such as -a are not paths.
        args = [a for a in arg.split(' ') if a and not a.startswith('-')] if arg else []
        return self.real(' '.join(args))[0]

    def cmd_LIST(self, arg):
        path = self.listing_target(arg)
        if os.path.isdir(path):
            names = sorted(os.listdir(path))
            self.send_listing([list_line(os.path.join(path, n), n) for n in names])
        elif os.path.exists(path):
            self.send_listing([list_line(path, os.path.basename(path))])
        else:
            self.reply('550 No such file or directory')

    def cmd_NLST(self, arg):
        path = self.listing_target(arg)
        if os.path.isdir(path):
            self.send_listing(sorted(os.listdir(path)))
        else:
            self.reply('550 No such directory')

    def cmd_MLSD(self, arg):
        path, _ = self.real(arg)
        if not os.path.isdir(path):
            self.reply('550 Not a directory')
            return
        lines = [mlsx_facts(path, '.')]
        lines += [mlsx_facts(os.path.join(path, n), n) for n in sorted(os.listdir(path))]
        self.send_listing(lines)

    def cmd_MLST(self, arg):
        path, virtual = self.real(arg)
        if not os.path.exists(path):
            self.reply('550 No such file or directory')
            return
        self.reply('250-Listing')
        self.reply(' ' + mlsx_facts(path, virtual).replace('; ' + virtual, '; ' + virtual))
        self.reply('250 End')

    def cmd_SIZE(self, arg):
        path, _ = self.real(arg)
        if os.path.isfile(path):
            self.reply(f'213 {os.path.getsize(path)}')
        else:
            self.reply('550 Not a file')

    def cmd_MDTM(self, arg):
        path, _ = self.real(arg)
        if os.path.exists(path):
            self.reply('213 ' + time.strftime('%Y%m%d%H%M%S', time.gmtime(os.path.getmtime(path))))
        else:
            self.reply('550 No such file')

    def cmd_MFMT(self, arg):
        stamp, _, name = arg.partition(' ')
        path, _ = self.real(name)
        try:
            when = time.mktime(time.strptime(stamp[:14], '%Y%m%d%H%M%S')) - time.timezone
            os.utime(path, (when, when))
            self.reply(f'213 Modify={stamp}; {name}')
        except (ValueError, OSError):
            self.reply('550 Cannot set time')

    def cmd_REST(self, arg):
        try:
            self.rest = int(arg)
            self.reply('350 Restarting')
        except ValueError:
            self.reply('501 Bad offset')

    def cmd_RETR(self, arg):
        path, _ = self.real(arg)
        if not os.path.isfile(path):
            self.reply('550 No such file')
            return
        conn = self.data_connection()
        if conn is None:
            return
        self.reply('150 Opening data connection')
        conn = self.secure(conn)
        with open(path, 'rb') as f:
            f.seek(self.rest)
            self.rest = 0
            while True:
                block = f.read(65536)
                if not block:
                    break
                conn.sendall(block)
        self.close_data(conn)
        self.reply('226 Transfer complete')

    def store(self, arg, mode):
        path, _ = self.real(arg)
        conn = self.data_connection()
        if conn is None:
            return
        self.reply('150 Ready to receive')
        conn = self.secure(conn)
        with open(path, mode) as f:
            if self.rest:
                f.seek(self.rest)
                f.truncate()
                self.rest = 0
            while True:
                data = conn.recv(65536)
                if not data:
                    break
                f.write(data)
        self.close_data(conn)
        self.reply('226 Transfer complete')

    def cmd_STOR(self, arg):
        self.store(arg, 'r+b' if self.rest else 'wb')

    def cmd_APPE(self, arg):
        self.store(arg, 'ab')

    def cmd_ALLO(self, arg):
        self.reply('202 No storage allocation necessary')

    def cmd_DELE(self, arg):
        path, _ = self.real(arg)
        try:
            os.remove(path)
            self.reply('250 Deleted')
        except OSError:
            self.reply('550 Cannot delete')

    def cmd_MKD(self, arg):
        path, virtual = self.real(arg)
        try:
            os.mkdir(path)
            self.reply(f'257 "{virtual}" created')
        except OSError:
            self.reply('550 Cannot create')

    def cmd_RMD(self, arg):
        path, _ = self.real(arg)
        try:
            os.rmdir(path)
            self.reply('250 Removed')
        except OSError:
            self.reply('550 Cannot remove')

    def cmd_RNFR(self, arg):
        path, _ = self.real(arg)
        if os.path.exists(path):
            self.rename_from = path
            self.reply('350 Ready for RNTO')
        else:
            self.reply('550 No such file')

    def cmd_RNTO(self, arg):
        path, _ = self.real(arg)
        try:
            os.rename(self.rename_from, path)
            self.reply('250 Renamed')
        except (OSError, TypeError):
            self.reply('550 Cannot rename')
        self.rename_from = None

    def cmd_SITE(self, arg):
        verb, _, rest = arg.partition(' ')
        if verb.upper() == 'CHMOD':
            mode, _, name = rest.partition(' ')
            try:
                os.chmod(self.real(name)[0], int(mode, 8))
                self.reply('200 Mode changed')
            except (ValueError, OSError):
                self.reply('550 Cannot change mode')
        else:
            self.reply('502 Unknown SITE command')


class Server:
    def __init__(self, a):
        self.root = os.path.abspath(a.root)
        self.user = a.user
        self.password = a.password
        self.implicit = a.implicit
        self.tls = None
        if a.cert:
            self.tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            self.tls.load_cert_chain(a.cert, a.key)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--port', type=int, required=True)
    ap.add_argument('--root', required=True)
    ap.add_argument('--user', default='linscp')
    ap.add_argument('--password', default='secret')
    ap.add_argument('--cert')
    ap.add_argument('--key')
    ap.add_argument('--implicit', action='store_true')
    a = ap.parse_args()
    server = Server(a)
    listener = socket.socket()
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(('127.0.0.1', a.port))
    listener.listen(8)
    while True:
        sock, _ = listener.accept()
        Session(server, sock).start()


if __name__ == '__main__':
    main()
