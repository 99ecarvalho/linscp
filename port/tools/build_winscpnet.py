#!/usr/bin/env python3
"""
Build WinSCP's .NET assembly (WinSCPnet.dll, netstandard2.0, or net40 as
WinSCP ships it for Windows PowerShell) from the pinned submodule, with
linscp's assembly patches applied to a copy of its dotnet/.

  build_winscpnet.py --dotnet DOTNET --upstream 3rd-party/winscp --dest DIR
                     [--framework netstandard2.0|net40]

The patches are port/patches/dotnet/*.patch, in name order, applied with
`git apply` (no fuzz: a patch upstream has moved past fails the build and
must be refreshed). The submodule is not touched. Prints the assembly's
path; exits 77 when DOTNET does not exist, for ctest to skip.
"""
import argparse, glob, os, shutil, subprocess, sys

PATCHES = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'patches', 'dotnet')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dotnet', required=True)
    ap.add_argument('--upstream', required=True)
    ap.add_argument('--dest', required=True)
    ap.add_argument('--framework', default='netstandard2.0', choices=('netstandard2.0', 'net40'))
    a = ap.parse_args()
    if not os.path.isfile(a.dotnet):
        print(f'SKIP: no dotnet at {a.dotnet} (port/tools/fetch_dotnet.py fetches one)')
        return 77
    source = os.path.join(a.dest, 'src')
    shutil.rmtree(source, ignore_errors=True)
    shutil.copytree(os.path.join(a.upstream, 'dotnet'), os.path.join(source, 'dotnet'),
                    ignore=shutil.ignore_patterns('bin', 'obj'))
    for patch in sorted(glob.glob(os.path.join(PATCHES, '*.patch'))):
        # Outside a repository git apply is a plain, strict patch tool. The
        # copy may be inside one (a build tree in the checkout), where git apply
        # would take the paths from that repository's top and skip them all
        # without a word: the ceiling stops it looking.
        env = dict(os.environ, GIT_CEILING_DIRECTORIES=os.path.dirname(os.path.abspath(source)))
        p = subprocess.run(['git', 'apply', '--verbose', '--whitespace=nowarn', os.path.abspath(patch)],
                           cwd=source, env=env, capture_output=True, text=True)
        if p.returncode != 0 or 'Applied patch' not in p.stderr:
            print(f'{os.path.basename(patch)} does not apply:\n{p.stdout}{p.stderr}')
            return 1
    out = os.path.join(a.dest, 'bin')
    build = subprocess.run([a.dotnet, 'build', os.path.join(source, 'dotnet', 'WinSCPnet.csproj'),
                            '-f', a.framework, '-c', 'Release', '-o', out, '-nologo'],
                           capture_output=True, text=True)
    if build.returncode != 0:
        print(build.stdout[-4000:] + build.stderr[-2000:])
        return 1
    print(os.path.join(out, 'WinSCPnet.dll'))
    return 0


if __name__ == '__main__':
    sys.exit(main())
