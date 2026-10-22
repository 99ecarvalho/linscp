#!/usr/bin/env python3
"""
The executables' version resource presents linscp (port/tools/gen_versioninfo.py):
linscp's company, product, description, product version and web address; no
internal or original file name of another program; the file version core
reads kept as the pinned project's.

  check_versioninfo.py --project WinSCP.cbproj --product-version X.Y.Z
"""
import argparse, os, re, subprocess, sys, tempfile

ap = argparse.ArgumentParser()
ap.add_argument('--project', required=True)
ap.add_argument('--product-version', required=True)
a = ap.parse_args()
tool = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'tools', 'gen_versioninfo.py')
with tempfile.TemporaryDirectory() as temp:
    out = os.path.join(temp, 'v.rc')
    subprocess.run([sys.executable, tool, '--project', a.project, '--out', out,
                    '--product-version', a.product_version], check=True)
    rc = open(out, encoding='utf-8').read()
values = dict(re.findall(r'VALUE "(\w+)", "([^"]*)\\0"', rc))
failures = []
def expect(ok, what):
    if not ok:
        failures.append(what)
expect(values.get('ProductName') == 'linscp', 'ProductName is linscp')
expect(values.get('CompanyName') == 'Apliant', 'CompanyName is Apliant')
expect(values.get('ProductVersion') == a.product_version, 'ProductVersion is linscp\'s')
expect(values.get('FileDescription', '').startswith('linscp'), 'FileDescription names linscp')
expect(values.get('WWW') == 'https://linscp.org', 'WWW is linscp.org')
expect('InternalName' not in values and 'OriginalFilename' not in values, 'no internal or original file name')
for key in ('ProductName', 'CompanyName', 'FileDescription', 'WWW'):
    expect(not re.search(r'winscp', values.get(key, ''), re.I), f'{key} does not name another program')
expect(re.search(r'^FILEVERSION 6,', rc, re.M) is not None, 'FILEVERSION is the pinned project\'s (core reads it)')
expect(re.search(r'^PRODUCTVERSION ' + a.product_version.replace('.', ','), rc, re.M) is not None, 'PRODUCTVERSION is linscp\'s')
for f in failures:
    print('FAIL:', f)
print('version resource: ' + ('ok' if not failures else f'{len(failures)} failure(s)'))
sys.exit(1 if failures else 0)
