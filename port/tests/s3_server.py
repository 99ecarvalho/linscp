#!/usr/bin/env python3
"""
A small S3 server for the protocol tests, on the standard library alone.

Serves --root over plain HTTP on 127.0.0.1:--port, path-style: each
directory under the root is a bucket, files are objects and directories
below a bucket are the "key/" markers S3 clients create for folders. Covers
what WinSCP's libs3 asks for: listing buckets, a bucket's location, listing
objects (v1 and v2, prefix and delimiter), HEAD, GET (with Range), PUT
(including copies with x-amz-copy-source), DELETE, an object's ACL (the
owner's full control until one is set) and its tags (?tagging, kept in memory). Signatures are
accepted, not checked: the test is of the client, not of AWS's auth.

  s3_server.py --port N --root DIR
"""
import argparse, email.utils, hashlib, http.server, os, shutil, time, urllib.parse
from xml.etree import ElementTree
from xml.sax.saxutils import escape

NS = 'http://s3.amazonaws.com/doc/2006-03-01/'


def iso(t):
    return time.strftime('%Y-%m-%dT%H:%M:%S.000Z', time.gmtime(t))


def etag(path):
    if os.path.isdir(path):
        return '"d41d8cd98f00b204e9800998ecf8427e"'
    h = hashlib.md5()
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(65536), b''):
            h.update(block)
    return f'"{h.hexdigest()}"'


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'
    root = '.'
    # (bucket, key) -> the tags last put, as (key, value).
    tags = {}
    acls = {}

    def log_message(self, *args):
        pass

    # --- helpers ----------------------------------------------------------
    def split(self):
        url = urllib.parse.urlsplit(self.path)
        query = urllib.parse.parse_qs(url.query, keep_blank_values=True)
        parts = urllib.parse.unquote(url.path).lstrip('/').split('/', 1)
        bucket = parts[0]
        key = parts[1] if len(parts) > 1 else ''
        return bucket, key, query

    def disk(self, bucket, key=''):
        path = os.path.normpath(os.path.join(self.root, bucket, key))
        if not path.startswith(os.path.abspath(self.root)):
            raise PermissionError(path)
        return path

    def send(self, code, body=b'', headers=None, content_type='application/xml'):
        self.send_response(code)
        if body or code not in (204, 304):
            self.send_header('Content-Type', content_type)
        self.send_header('Content-Length', str(len(body)))
        for k, v in (headers or {}).items():
            self.send_header(k, v)
        self.end_headers()
        if self.command != 'HEAD':
            self.wfile.write(body)

    def xml(self, code, inner):
        self.send(code, ('<?xml version="1.0" encoding="UTF-8"?>\n' + inner).encode())

    def error(self, code, s3code, message):
        self.xml(code, f'<Error><Code>{s3code}</Code><Message>{escape(message)}</Message></Error>')

    def body(self):
        length = int(self.headers.get('Content-Length', '0') or '0')
        return self.rfile.read(length) if length else b''

    # --- requests -----------------------------------------------------------
    def do_GET(self):
        bucket, key, query = self.split()
        if not bucket:
            buckets = ''.join(
                f'<Bucket><Name>{escape(b)}</Name><CreationDate>{iso(os.path.getmtime(self.disk(b)))}</CreationDate></Bucket>'
                for b in sorted(os.listdir(self.root)) if os.path.isdir(self.disk(b)))
            return self.xml(200, f'<ListAllMyBucketsResult xmlns="{NS}"><Owner><ID>linscp</ID>'
                                 f'<DisplayName>linscp</DisplayName></Owner><Buckets>{buckets}</Buckets>'
                                 '</ListAllMyBucketsResult>')
        if not os.path.isdir(self.disk(bucket)):
            return self.error(404, 'NoSuchBucket', bucket)
        if not key:
            if 'location' in query:
                return self.xml(200, f'<LocationConstraint xmlns="{NS}"></LocationConstraint>')
            return self.list_objects(bucket, query)
        if 'acl' in query:
            # One set (PUT ?acl), kept in memory; else the owner's full
            # control, the one grant: what Properties reads.
            if (bucket, key) in self.acls:
                return self.xml(200, self.acls[(bucket, key)])
            return self.xml(200, f'<AccessControlPolicy xmlns="{NS}"><Owner><ID>linscp</ID><DisplayName>linscp</DisplayName>'
                                 '</Owner><AccessControlList><Grant><Grantee xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" '
                                 'xsi:type="CanonicalUser"><ID>linscp</ID><DisplayName>linscp</DisplayName></Grantee>'
                                 '<Permission>FULL_CONTROL</Permission></Grant></AccessControlList></AccessControlPolicy>')
        if 'tagging' in query:
            if not os.path.exists(self.disk(bucket, key)):
                return self.error(404, 'NoSuchKey', key)
            tags = ''.join(f'<Tag><Key>{escape(k)}</Key><Value>{escape(v)}</Value></Tag>'
                           for k, v in self.tags.get((bucket, key), []))
            return self.xml(200, f'<Tagging xmlns="{NS}"><TagSet>{tags}</TagSet></Tagging>')
        return self.get_object(bucket, key, head=False)

    def do_HEAD(self):
        bucket, key, _ = self.split()
        if not key:
            return self.send(200 if os.path.isdir(self.disk(bucket)) else 404)
        return self.get_object(bucket, key, head=True)

    def list_objects(self, bucket, query):
        prefix = query.get('prefix', [''])[0]
        delimiter = query.get('delimiter', [''])[0]
        v2 = query.get('list-type', [''])[0] == '2'
        base = self.disk(bucket)
        contents, prefixes = [], []
        if delimiter == '/':
            directory = prefix.rpartition('/')[0]
            start = prefix.rpartition('/')[2]
            here = self.disk(bucket, directory)
            if os.path.isdir(here):
                if prefix.endswith('/') and prefix:
                    contents.append(prefix)  # the folder's own marker
                for name in sorted(os.listdir(here)):
                    if not name.startswith(start):
                        continue
                    k = (directory + '/' if directory else '') + name
                    if os.path.isdir(os.path.join(here, name)):
                        prefixes.append(k + '/')
                    else:
                        contents.append(k)
        else:
            for dirpath, dirs, files in os.walk(base):
                rel = os.path.relpath(dirpath, base).replace(os.sep, '/')
                rel = '' if rel == '.' else rel + '/'
                if rel and rel.startswith(prefix):
                    contents.append(rel)
                contents += [rel + f for f in files if (rel + f).startswith(prefix)]
            contents.sort()

        def item(k):
            path = self.disk(bucket, k)
            size = 0 if os.path.isdir(path) else os.path.getsize(path)
            return (f'<Contents><Key>{escape(k)}</Key><LastModified>{iso(os.path.getmtime(path))}</LastModified>'
                    f'<ETag>{escape(etag(path))}</ETag><Size>{size}</Size><StorageClass>STANDARD</StorageClass>'
                    '<Owner><ID>linscp</ID><DisplayName>linscp</DisplayName></Owner></Contents>')

        body = ''.join(item(k) for k in contents)
        body += ''.join(f'<CommonPrefixes><Prefix>{escape(p)}</Prefix></CommonPrefixes>' for p in prefixes)
        counts = f'<KeyCount>{len(contents) + len(prefixes)}</KeyCount>' if v2 else '<Marker></Marker>'
        self.xml(200, f'<ListBucketResult xmlns="{NS}"><Name>{escape(bucket)}</Name>'
                      f'<Prefix>{escape(prefix)}</Prefix>{counts}<MaxKeys>1000</MaxKeys>'
                      f'<Delimiter>{escape(delimiter)}</Delimiter><IsTruncated>false</IsTruncated>'
                      f'{body}</ListBucketResult>')

    def get_object(self, bucket, key, head):
        path = self.disk(bucket, key)
        if key.endswith('/'):
            if not os.path.isdir(path):
                return self.error(404, 'NoSuchKey', key)
            data = b''
        elif not os.path.isfile(path):
            return self.error(404, 'NoSuchKey', key)
        else:
            with open(path, 'rb') as f:
                data = f.read()
        headers = {'ETag': etag(path), 'Last-Modified': email.utils.formatdate(os.path.getmtime(path), usegmt=True)}
        code = 200
        ranged = self.headers.get('Range', '')
        if ranged.startswith('bytes=') and data:
            first, _, last = ranged[6:].partition('-')
            first = int(first or 0)
            last = int(last) if last else len(data) - 1
            headers['Content-Range'] = f'bytes {first}-{last}/{len(data)}'
            data = data[first:last + 1]
            code = 206
        if head:
            self.send_response(code)
            self.send_header('Content-Length', str(len(data)))
            self.send_header('Content-Type', 'application/octet-stream')
            for k, v in headers.items():
                self.send_header(k, v)
            self.end_headers()
            return
        self.send(code, data, headers, 'application/octet-stream')

    def do_PUT(self):
        bucket, key, query = self.split()
        data = self.body()
        if not key:
            os.makedirs(self.disk(bucket), exist_ok=True)
            return self.send(200)
        path = self.disk(bucket, key)
        if 'acl' in query:
            self.acls[(bucket, key)] = data.decode('utf-8')
            return self.send(200)
        if 'tagging' in query:
            # As S3: read as tags, given back in its own namespace.
            document = ElementTree.fromstring(data)
            self.tags[(bucket, key)] = [(t.findtext('{*}Key') or '', t.findtext('{*}Value') or '')
                                        for t in document.iter() if t.tag.rpartition('}')[2] == 'Tag']
            return self.send(200)
        source = self.headers.get('x-amz-copy-source')
        if source:
            sbucket, _, skey = urllib.parse.unquote(source).lstrip('/').partition('/')
            spath = self.disk(sbucket, skey)
            if not os.path.isfile(spath):
                return self.error(404, 'NoSuchKey', skey)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            shutil.copyfile(spath, path)
            return self.xml(200, f'<CopyObjectResult><LastModified>{iso(time.time())}</LastModified>'
                                 f'<ETag>{escape(etag(path))}</ETag></CopyObjectResult>')
        if key.endswith('/'):
            os.makedirs(path, exist_ok=True)
        else:
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'wb') as f:
                f.write(data)
        self.send(200, b'', {'ETag': etag(path)})

    def do_DELETE(self):
        bucket, key, _ = self.split()
        path = self.disk(bucket, key)
        try:
            if key.endswith('/'):
                os.rmdir(path)
            elif os.path.isfile(path):
                os.remove(path)
        except OSError:
            return self.error(409, 'BucketNotEmpty' if key.endswith('/') else 'InternalError', key)
        self.send(204)

    def do_POST(self):
        self.body()
        self.error(501, 'NotImplemented', 'POST')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--port', type=int, required=True)
    ap.add_argument('--root', required=True)
    a = ap.parse_args()
    Handler.root = os.path.abspath(a.root)
    http.server.ThreadingHTTPServer(('127.0.0.1', a.port), Handler).serve_forever()


if __name__ == '__main__':
    main()
