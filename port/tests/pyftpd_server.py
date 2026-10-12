#!/usr/bin/env python3
"""
pyftpdlib serving a directory, for the protocol tests: an FTP server the
suite did not write. --cert/--key make it FTPS (explicit, RFC 4217).

  pyftpd_server.py --port N --root DIR [--user U --password P] [--cert C --key K]
"""
import argparse, logging

from pyftpdlib.authorizers import DummyAuthorizer
from pyftpdlib.handlers import FTPHandler, TLS_FTPHandler
from pyftpdlib.servers import ThreadedFTPServer


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--port', type=int, required=True)
    ap.add_argument('--root', required=True)
    ap.add_argument('--user', default='linscp')
    ap.add_argument('--password', default='secret')
    ap.add_argument('--cert')
    ap.add_argument('--key')
    a = ap.parse_args()
    logging.basicConfig(level=logging.WARNING)
    authorizer = DummyAuthorizer()
    authorizer.add_user(a.user, a.password, a.root, perm='elradfmwMT')
    if a.cert:
        handler = TLS_FTPHandler
        handler.certfile = a.cert
        handler.keyfile = a.key
        handler.tls_control_required = True
        handler.tls_data_required = True
    else:
        handler = FTPHandler
    handler.authorizer = authorizer
    handler.passive_ports = None
    ThreadedFTPServer(('127.0.0.1', a.port), handler).serve_forever()


if __name__ == '__main__':
    main()
