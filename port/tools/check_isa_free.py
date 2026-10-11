#!/usr/bin/env python3
"""
A function compiled in a file built with extra ISA flags must not itself use
those instructions when it is what decides whether they are available.

aes-ni.c is compiled with -msse4.1 -maes (see port/putty/CMakeLists.txt), but
aes_ni_available() runs on every CPU, before support is known. This
disassembles it and fails if the compiler used any SSE4.1 or AES-NI
instruction there.

  check_isa_free.py OBJDUMP ARCHIVE OBJECT FUNCTION
"""
import re, subprocess, sys

SSE41 = ('blendpd blendps blendvpd blendvps dppd dpps extractps insertps movntdqa '
         'mpsadbw packusdw pblendvb pblendw pcmpeqq pextrb pextrd pextrq phminposuw '
         'pinsrb pinsrd pinsrq pmaxsb pmaxsd pmaxud pmaxuw pminsb pminsd pminud '
         'pminuw pmuldq pmulld ptest roundpd roundps roundsd roundss').split()
AES = 'aesenc aesenclast aesdec aesdeclast aesimc aeskeygenassist pclmulqdq'.split()
FORBIDDEN = set(SSE41 + AES)


def main(objdump, archive, obj, function):
    out = subprocess.run([objdump, '-d', '--no-show-raw-insn', archive],
                         capture_output=True, text=True, check=True).stdout
    # Sections per member: find the member, then the function's block in it.
    # GNU objdump heads one "member:", llvm-objdump "archive(member):".
    member = re.split(rf'(?:^|\(){re.escape(obj)}\)?:', out, maxsplit=1, flags=re.M)
    if len(member) < 2:
        sys.exit(f'{obj} not found in {archive}')
    body = re.search(rf'<{re.escape(function)}>:\n(.*?)(?:\n\n|\Z)', member[1], re.S)
    if not body:
        sys.exit(f'{function} not found in {obj}')
    used = sorted({m for m in re.findall(r'^\s*[0-9a-f]+:\s+(\w+)', body.group(1), re.M)
                   if m.lower().lstrip('v') in FORBIDDEN})
    n = len(body.group(1).strip().splitlines())
    print(f'{function}: {n} instructions')
    if used:
        print(f'FAIL: uses {", ".join(used)} before support is known')
        return 1
    print('OK: no SSE4.1 or AES-NI instructions in the availability check')
    return 0


if __name__ == '__main__':
    sys.exit(main(*sys.argv[1:5]))
