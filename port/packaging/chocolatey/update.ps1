# Moves the package to linscp's latest release (chocolatey-au):
#
#   Install-Module Chocolatey-AU -Scope CurrentUser   # once
#   cd port/packaging/chocolatey; ./update.ps1
#
# The checksum comes from the release's SHA256SUMS (published from 0.7.2 on);
# for a release without one, AU downloads the installer and computes it.
Import-Module Chocolatey-AU

$repo = '99ecarvalho/linscp'
$release = Invoke-RestMethod "https://api.github.com/repos/$repo/releases/latest" `
  -Headers @{ 'User-Agent' = 'linscp-chocolatey-au' }

function global:au_SearchReplace {
  @{
    'tools\chocolateyInstall.ps1' = @{
      "(?i)(^\s*url64bit\s*=\s*)'.*'"   = "`$1'$($Latest.URL64)'"
      "(?i)(^\s*checksum64\s*=\s*)'.*'" = "`$1'$($Latest.Checksum64)'"
    }
    'linscp.nuspec' = @{
      '(<releaseNotes>).*(</releaseNotes>)' = "`${1}$($Latest.ReleaseNotes)`$2"
      '(linscp@v)[^/]+(/assets/)'           = "`${1}$($Latest.Version)`$2"
      '(/blob/v)[^/]+(/LICENSE)'            = "`${1}$($Latest.Version)`$2"
    }
  }
}

function global:au_GetLatest {
  $version = $release.tag_name -replace '^v', ''
  $setup = "linscp-$version-setup.exe"
  $latest = @{
    Version      = $version
    URL64        = "https://github.com/$repo/releases/download/v$version/$setup"
    ReleaseNotes = $release.html_url
  }
  $sums = $release.assets | Where-Object name -eq 'SHA256SUMS'
  if ($sums) {
    $text = [Text.Encoding]::UTF8.GetString(
      (Invoke-WebRequest $sums.browser_download_url -UseBasicParsing).RawContentStream.ToArray())
    $pattern = "^([0-9a-fA-F]{64})\s+\*?$([regex]::Escape($setup))\s*$"
    $line = $text -split "`n" | Where-Object { $_ -match $pattern } | Select-Object -First 1
    if (-not $line) { throw "SHA256SUMS of v$version has no line for $setup" }
    $latest.Checksum64 = ([regex]::Match($line, $pattern)).Groups[1].Value.ToLower()
    $latest.ChecksumType64 = 'sha256'
  }
  $latest
}

$hasSums = [bool]($release.assets | Where-Object name -eq 'SHA256SUMS')
update -ChecksumFor $(if ($hasSums) { 'none' } else { '64' })
