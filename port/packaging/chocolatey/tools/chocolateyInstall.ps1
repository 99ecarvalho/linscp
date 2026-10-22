$ErrorActionPreference = 'Stop'

# The release's Inno Setup installer (port/packaging/windows/linscp.iss),
# installed for all users. Chocolatey's automatic uninstaller removes it
# through its Add/Remove Programs entry.
$packageArgs = @{
  packageName    = $env:ChocolateyPackageName
  fileType       = 'exe'
  url64bit       = 'https://github.com/99ecarvalho/linscp/releases/download/v0.7.2/linscp-0.7.2-setup.exe'
  checksum64     = '8a8f556f2f7b87e019ec45a186683a6fd3b4669cdd45381792e060da21cf33b4'
  checksumType64 = 'sha256'
  softwareName   = 'linscp*'
  silentArgs     = '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP-'
  validExitCodes = @(0)
}

Install-ChocolateyPackage @packageArgs
