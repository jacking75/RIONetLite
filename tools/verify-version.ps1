param([string]$ExpectedVersion = '', [switch]$RequireReleaseTag)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$manifest = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'vcpkg.json') | ConvertFrom-Json
$header = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'include\rionetlite\version.h')
$match = [regex]::Match($header, 'RIONETLITE_VERSION_STRING\s+"([^"]+)"')
if (-not $match.Success) { throw 'version.h has no version string.' }
$version = $match.Groups[1].Value
if ($manifest.'version-string' -ne $version) { throw 'vcpkg.json and version.h differ.' }
if ($ExpectedVersion -and $ExpectedVersion -ne $version) { throw 'Requested package version differs from source.' }
if (-not (Select-String -Quiet -Path (Join-Path $root 'CHANGELOG.md') -SimpleMatch "## $version")) {
    throw 'CHANGELOG.md has no matching version entry.'
}
$versionTags = @(& git -C $root tag --points-at HEAD) | Where-Object { $_ -like 'v*' }
if ($versionTags.Count -gt 0 -and $versionTags -notcontains "v$version") {
    throw 'Git tag and source version differ.'
}
if ($RequireReleaseTag) {
    if ($versionTags -notcontains "v$version") { throw "HEAD has no release tag v$version." }
    if (@(& git -C $root status --porcelain).Count -ne 0) { throw 'A release requires a clean tracked source tree.' }
}
Write-Host "Version verified: $version"
