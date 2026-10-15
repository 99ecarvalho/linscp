# Windows test VM: setting up SSH access

How the Windows 11 VM that linscp's Windows builds are tested on natively is
set up, so it can be rebuilt. The build host reaches it over SSH, copies the
deploy folder and test programs over SFTP, and runs them there.
`windows-toolchain.md` covers the builds themselves.

What the VM has today (checked 2026-10-14):

- Windows 11 Enterprise Evaluation 25H2.
- The OpenSSH Server optional capability, `sshd` running and starting
  automatically.
- Windows' stock `sshd_config`, so password logins work.
- Two local accounts:
  - one standard user;
  - one administrator, used for everything below.
- Everything installed or copied for linscp under `C:\temp`.

The commands are for an elevated PowerShell on the VM (run as the
administrator) unless they say otherwise.

## 1. Accounts

A standard user and an administrator, with passwords. Pick your own; the
repository does not record them.

```powershell
$Password = Read-Host -AsSecureString "Password for the standard user"
New-LocalUser -Name x -Password $Password -PasswordNeverExpires
Add-LocalGroupMember -Group Users -Member x

$Password = Read-Host -AsSecureString "Password for the administrator"
New-LocalUser -Name user -Password $Password -PasswordNeverExpires
Add-LocalGroupMember -Group Administrators -Member user
```

## 2. OpenSSH Server

```powershell
# Is it there? Both should say Installed after the next line.
Get-WindowsCapability -Online -Name OpenSSH*
Add-WindowsCapability -Online -Name OpenSSH.Server~~~~0.0.1.0

Start-Service sshd
Set-Service -Name sshd -StartupType Automatic
Get-Service sshd                # Running, Automatic
```

Installing the capability creates the `OpenSSH-Server-In-TCP` firewall rule,
but only for the Private network profile. A VM's network often comes up as
Public, so a second rule opens port 22 on every profile:

```powershell
New-NetFirewallRule -Name sshd-linscp -DisplayName "OpenSSH Server (linscp)" `
  -Enabled True -Direction Inbound -Protocol TCP -Action Allow -LocalPort 22 -Profile Any
Get-NetFirewallRule -DisplayName *SSH* | Select Name, DisplayName, Enabled, Profile
```

The stock `C:\ProgramData\ssh\sshd_config` is kept. Password authentication
is on by default.

The default shell is left as `cmd.exe`. Its single-command quirk matters
when scripting: `cmd` runs only the first line of a multi-line command, so
send one command per exec, or join them with `&` or `&&`.

### Optional: key instead of password

For the administrator, Windows reads keys from one shared file, not from the
user's profile (`Match Group administrators` in `sshd_config`):

```powershell
# The public key, as one line, from the build host.
Add-Content -Path C:\ProgramData\ssh\administrators_authorized_keys -Value "ssh-ed25519 AAAA... you@host"
icacls C:\ProgramData\ssh\administrators_authorized_keys /inheritance:r /grant "Administrators:F" /grant "SYSTEM:F"
```

For the standard user, the key goes in `C:\Users\x\.ssh\authorized_keys`.

## 3. Check from the build host

The VM's address on the test network was `192.168.0.33`.

```sh
ssh user@192.168.0.33 "ver & whoami"
```

The project's scripts use Python's paramiko with password logins: no agent,
no key files. Its `exec_command` runs through `cmd.exe`, as above.

## 4. What the tests need under `C:\temp`

All of it is portable: nothing is installed system-wide. In PowerShell, as
the administrator:

```powershell
New-Item -ItemType Directory -Force C:\temp | Out-Null
Set-Location C:\temp

# Python 3.12, the embeddable package (no installer, no pip): enough for the
# test wrappers (port/tests/run_qt_session.py and its fixtures).
curl.exe -L -o py.zip https://www.python.org/ftp/python/3.12.7/python-3.12.7-embed-amd64.zip
Expand-Archive py.zip -DestinationPath C:\temp\python

# rclone, the SFTP server the tests talk to (the golden corpus's version).
curl.exe -L -o rclone.zip https://downloads.rclone.org/v1.75.2/rclone-v1.75.2-windows-amd64.zip
Expand-Archive rclone.zip -DestinationPath C:\temp
Copy-Item C:\temp\rclone-v1.75.2-windows-amd64\rclone.exe C:\temp\rclone.exe

# Only for the .NET assembly's tests: the .NET 8 SDK, user-local.
curl.exe -L -o dotnet-install.ps1 https://dot.net/v1/dotnet-install.ps1
powershell -ExecutionPolicy Bypass -File .\dotnet-install.ps1 -Channel 8.0 -InstallDir C:\temp\dotnet
```

WinSCP itself, for comparing against, is installed normally (6.5.7, in
`C:\Program Files (x86)\WinSCP`).

The test runs put these on `PATH` themselves (`set PATH=C:\temp;%PATH%`).

## 5. The folders the build host copies

| Folder | What | From |
|---|---|---|
| `C:\temp\linscp-gui` | the shell, as a user runs it: `build/win/deploy` plus `linscp.cmd` (its own `linscp.ini`, so WinSCP's registry is left alone) | `cmake --build build/win --target deploy` |
| `C:\temp\linscp-test` | the deploy folder plus `test_qt_gui.exe`, `test_qt_session.exe` and `Qt6Test.dll` | the same, plus `build/win/port/tests` |
| `C:\temp\linscp\port\tests` | the test wrappers and fixtures (`run_qt_session.py`, `fixtures.py`, `ftp_server.py`, the golden corpus) | `port/tests` |

A test then runs as, for example:

```bat
set PATH=C:\temp;%PATH% && C:\temp\python\python.exe C:\temp\linscp\port\tests\run_qt_session.py C:\temp\linscp-test\test_qt_gui.exe sync /ini=nul {port} {tmp}\local -platform windows
```

Notes:

- **Use `-platform windows`.** The deploy folder has only Qt's `windows`
  platform plugin. With `offscreen`, Qt's "no platform plugin" message box
  waits for a click nobody can give over SSH.
- **Close the shell before redeploying `linscp-gui`.** A running `linscp.exe`
  holds its DLLs, and a half-deleted folder is the result. `tasklist /FI
  "IMAGENAME eq linscp.exe"` shows whether one is running.

## 6. Running the whole suite there

`port/tools/vm_tests.py` does all of the above for every test ctest knows
for the cross build: it copies the deploy folder, the test programs and
`port/tests` (skipping unchanged files), turns each ctest command into a
native one (Wine dropped, the VM's paths, `-platform windows`), and runs
them over SSH, reporting passed, failed and skipped (exit 77) as ctest does.
The password comes from the environment, never the repository:

```sh
LINSCP_VM_PASSWORD=... python3 port/tools/vm_tests.py            # everything
LINSCP_VM_PASSWORD=... python3 port/tools/vm_tests.py -R qt_gui   # some
```

Tests that check the source tree, and the POSIX layer's own test, are not
run there; those needing sshd or a container engine skip, as the VM has
neither.
