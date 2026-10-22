#!/usr/bin/env python3
"""
Build WinSCP's .NET assembly (WinSCPnet.dll, netstandard2.0, or net40 as
WinSCP ships it for Windows PowerShell) from the pinned submodule, with
linscp's assembly patches applied to a copy of its dotnet/.

  build_winscpnet.py --dotnet DOTNET --upstream 3rd-party/winscp --dest DIR
                     --product-version X.Y.Z [--framework netstandard2.0|net40]

The patches are port/patches/dotnet/*.patch, in name order, applied with
`git apply` (no fuzz: a patch upstream has moved past fails the build and
must be refreshed). Then linscp's identity (IDENTITY): the assembly starts
linscp-cli, of linscp's product version (the version resource
gen_versioninfo.py writes, which the assembly checks), looks for it where
linscp's installer puts it, and names linscp as its product. The
submodule is not touched. Prints the assembly's path; exits 77 when DOTNET
does not exist, for ctest to skip.
"""
import argparse, glob, os, shutil, subprocess, sys

PATCHES = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'patches', 'dotnet')

# The Inno Setup AppId of port/packaging/windows/linscp.iss, whose uninstall
# key records the installation folder.
INSTALLER_APP_ID = '{6C1E36A4-5B9F-4B8C-9C2E-3F0A7D1E8B52}'

# (file, upstream text, linscp's), each upstream text found exactly once.
IDENTITY = [
    ('internal/ExeSessionProcess.cs',
     'private const string ExeExecutableFileName = "winscp.exe";',
     'private static readonly string ExeExecutableFileName =\n'
     '            (Path.DirectorySeparatorChar == \'\\\\\') ? "linscp-cli.exe" : "linscp-cli";'),
    ('internal/ExeSessionProcess.cs',
     'return Path.Combine(programFiles, "WinSCP");',
     'return Path.Combine(programFiles, "linscp");'),
    ('internal/ExeSessionProcess.cs',
     'programFiles = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86);',
     'programFiles = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles);'),
    ('internal/ExeSessionProcess.cs',
     'RegistryKey baseKey = RegistryKey.OpenBaseKey(hive, RegistryView.Registry32);',
     'RegistryKey baseKey = RegistryKey.OpenBaseKey(hive, RegistryView.Registry64);'),
    ('internal/ExeSessionProcess.cs',
     r'OpenSubKey(@"Software\Microsoft\Windows\CurrentVersion\Uninstall\winscp3_is1")',
     r'OpenSubKey(@"Software\Microsoft\Windows\CurrentVersion\Uninstall\%s_is1")' % INSTALLER_APP_ID),
    ('properties/AssemblyInfo.cs',
     '[assembly: AssemblyTitle("WinSCPnet")]',
     '[assembly: AssemblyTitle("linscp scripting interface for .NET")]'),
    ('properties/AssemblyInfo.cs',
     '[assembly: AssemblyDescription("WinSCP scripting interface .NET wrapper")]',
     '[assembly: AssemblyDescription("linscp scripting interface for .NET")]'),
    ('properties/AssemblyInfo.cs',
     '[assembly: AssemblyCompany("Martin Prikryl")]',
     '[assembly: AssemblyCompany("Apliant")]'),
    ('properties/AssemblyInfo.cs',
     '[assembly: AssemblyProduct("WinSCP")]',
     '[assembly: AssemblyProduct("linscp")]'),
]


def apply_identity(source, product_version):
    for name, old, new in IDENTITY + [
            ('properties/AssemblyInfo.cs', None, f'public const string ProductVersion = "{product_version}";')]:
        path = os.path.join(source, 'dotnet', name)
        with open(path, encoding='utf-8-sig') as f:
            text = f.read()
        if old is None:
            lines = [l for l in text.splitlines() if 'public const string ProductVersion = ' in l]
            old = lines[0].strip() if len(lines) == 1 else ''
        if not old or text.count(old) != 1:
            raise SystemExit(f'{name}: the text linscp replaces has moved: {old or "ProductVersion"}')
        with open(path, 'w', encoding='utf-8-sig') as f:
            f.write(text.replace(old, new))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dotnet', required=True)
    ap.add_argument('--upstream', required=True)
    ap.add_argument('--dest', required=True)
    ap.add_argument('--product-version', required=True)
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
    apply_identity(source, a.product_version)
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
