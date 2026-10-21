$ErrorActionPreference = 'Stop'

# The release's Inno Setup installer (port/packaging/windows/linscp.iss),
# installed for all users. Chocolatey's automatic uninstaller removes it
# through its Add/Remove Programs entry.
$packageArgs = @{
  packageName    = $env:ChocolateyPackageName
  fileType       = 'exe'
  url64bit       = 'https://github.com/99ecarvalho/linscp/releases/download/v0.7.1/linscp-0.7.1-setup.exe'
  checksum64     = '27b7ea17014fc062deffc9d71dd73eb9688e5d625a2fed596c12a09f9d6cfdfc'
  checksumType64 = 'sha256'
  softwareName   = 'linscp*'
  silentArgs     = '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP-'
  validExitCodes = @(0)
}

Install-ChocolateyPackage @packageArgs
