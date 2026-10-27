param([string]$Version = '', [switch]$RequireReleaseTag)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$header = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'include\rionetlite\version.h')
if (-not $Version) { $Version = [regex]::Match($header, 'RIONETLITE_VERSION_STRING\s+"([^"]+)"').Groups[1].Value }
if ($Version -notmatch '^\d+\.\d+\.\d+(?:-[A-Za-z0-9.-]+)?$') { throw 'Invalid package version.' }
& (Join-Path $PSScriptRoot 'verify-version.ps1') -ExpectedVersion $Version -RequireReleaseTag:$RequireReleaseTag
if ($LASTEXITCODE -ne 0) { throw 'Version verification failed.' }
$artifactsRoot = [IO.Path]::GetFullPath((Join-Path $root 'artifacts'))
$stage = [IO.Path]::GetFullPath((Join-Path $artifactsRoot "RIONetLite-$Version"))
$verification = [IO.Path]::GetFullPath((Join-Path $artifactsRoot "package-verification\$Version"))
$allowedPrefix = $artifactsRoot.TrimEnd('\') + '\'
foreach ($target in @($stage, $verification)) {
    if (-not $target.StartsWith($allowedPrefix, [StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid package path.' }
    if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target -Recurse -Force }
}
New-Item -ItemType Directory -Force -Path (Join-Path $stage 'include'),
    (Join-Path $stage 'lib'), (Join-Path $stage 'source'),
    (Join-Path $stage 'samples\package_consumer'), $verification | Out-Null

function Get-SourceSnapshot {
    $paths = @(& git -C $root ls-files --cached --others --exclude-standard) | Sort-Object -Unique
    if ($LASTEXITCODE -ne 0) { throw 'Source inventory failed.' }
    foreach ($relative in $paths) {
        $path = Join-Path $root $relative
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            [pscustomobject]@{ Path=$relative.Replace('\', '/'); SHA256=(Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash }
        }
    }
}
function Get-SnapshotHash($snapshot) {
    $serialized = $snapshot | ConvertTo-Json -Compress -Depth 4
    $hasher = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($hasher.ComputeHash([Text.Encoding]::UTF8.GetBytes($serialized)))).Replace('-', '') }
    finally { $hasher.Dispose() }
}
$snapshot = @(Get-SourceSnapshot)
$sourceHash = Get-SnapshotHash $snapshot
$commit = & git -C $root rev-parse HEAD
$sourceStatus = @(& git -C $root status --porcelain)
$tags = @(& git -C $root tag --points-at HEAD)
foreach ($file in $snapshot) {
    $destination = Join-Path (Join-Path $stage 'source') $file.Path
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
    Copy-Item -LiteralPath (Join-Path $root $file.Path) -Destination $destination
    if ((Get-FileHash -Algorithm SHA256 -LiteralPath $destination).Hash -ne $file.SHA256) {
        throw "Source changed while copying: $($file.Path)"
    }
}
if ((Get-SnapshotHash @(Get-SourceSnapshot)) -ne $sourceHash) { throw 'Source changed during snapshot creation.' }
$snapshot | ConvertTo-Json -Depth 4 | Set-Content -Encoding UTF8 (Join-Path $stage 'source-files.json')

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products * -version '[18.0,19.0)' -requires Microsoft.Component.MSBuild -property installationPath
if (-not $installation) { throw 'Visual Studio 2026 was not found.' }
$msbuild = Join-Path $installation 'MSBuild\Current\Bin\amd64\MSBuild.exe'
$taskPath = $env:Path
Remove-Item Env:Path -ErrorAction SilentlyContinue
Remove-Item Env:PATH -ErrorAction SilentlyContinue
$env:Path = $taskPath
$library = Join-Path $stage 'source\build\RIONetLite.vcxproj'
$propertiesText = & $msbuild $library /nologo /p:Configuration=Release /p:Platform=x64 `
    /p:VcpkgEnabled=false /p:VcpkgEnableManifest=false `
    '-getProperty:VCToolsVersion,WindowsTargetPlatformVersion,PlatformToolset,WindowsSdkDir,WindowsSDK_IncludePath'
if ($LASTEXITCODE -ne 0) { throw 'Toolchain property query failed.' }
$properties = ($propertiesText -join "`n" | ConvertFrom-Json).Properties
$sdkVersion = [regex]::Match($properties.WindowsSDK_IncludePath,
    'Include\\(10\.\d+\.\d+\.\d+)\\').Groups[1].Value
if (-not $sdkVersion) { throw 'The resolved Windows SDK version was not found.' }
$compiler = Join-Path $installation "VC\Tools\MSVC\$($properties.VCToolsVersion)\bin\Hostx64\x64\cl.exe"
$buildLog = Join-Path $verification 'library-build.log'
$intRoot = Join-Path $verification 'obj\'
& $msbuild $library /nologo /m /t:Rebuild /p:Configuration=Release /p:Platform=x64 `
    "/p:WindowsTargetPlatformVersion=$sdkVersion" `
    "/p:OutDir=$(Join-Path $stage 'lib\')" "/p:RIONetLiteIsolatedIntRoot=$intRoot" `
    /p:WholeProgramOptimization=false /p:UseMimalloc=false /p:VcpkgEnabled=false `
    /p:VcpkgEnableManifest=false /p:VcpkgApplocalDeps=false *> $buildLog
if ($LASTEXITCODE -ne 0) { throw "Fresh package build failed: $buildLog" }
foreach ($binary in @('RIONetLite.lib', 'RIONetLite.pdb')) {
    if (-not (Test-Path -LiteralPath (Join-Path $stage "lib\$binary"))) { throw "Missing fresh binary: $binary" }
}
Copy-Item -Recurse (Join-Path $stage 'source\include\rionetlite') (Join-Path $stage 'include')
Copy-Item -LiteralPath (Join-Path $stage 'source\LICENSE'), (Join-Path $stage 'source\CHANGELOG.md'),
    (Join-Path $stage 'source\SECURITY.md'), (Join-Path $stage 'source\THIRD_PARTY_NOTICES.md') -Destination $stage
$packageReadme = @'
# RIONetLite package

This x64 Release static library uses MSVC v145, /MD and /std:c++latest.
Use include/ and lib/RIONetLite.lib with Ws2_32.lib, Mswsock.lib and Mincore.lib.
Read package-manifest.json for the exact compiler, SDK, source hash and flags.
Build samples/package_consumer/PackageConsumer.vcxproj to check this ZIP alone.

Open [the offline book](source/docs/interactive_book/index.html#chapter=packaging-and-release)
for ABI, consumer settings, provenance, limitations and packaging instructions.
The book and its source snapshots are included under source/docs/interactive_book/.
This candidate is not an approved or tagged release unless its manifest says so.
'@
[IO.File]::WriteAllText((Join-Path $stage 'README.md'), $packageReadme.Replace("`r`n", "`n") + "`n", [Text.UTF8Encoding]::new($false))
Copy-Item -LiteralPath (Join-Path $stage 'source\samples\package_consumer\main.cpp'),
    (Join-Path $stage 'source\samples\package_consumer\PackageConsumer.vcxproj') `
    -Destination (Join-Path $stage 'samples\package_consumer')
Set-Content -Encoding ASCII -LiteralPath (Join-Path $stage 'VERSION') -Value $Version
$manifest = [ordered]@{
    PackageVersion=$Version; CreatedAtUtc=(Get-Date).ToUniversalTime().ToString('o')
    SourceCommit=$commit; SourceTreeSHA256=$sourceHash; SourceDirty=($sourceStatus.Count -ne 0)
    SourceStatus=$sourceStatus; TagsAtCommit=$tags
    ExcludedSourceFiles=@()
    ReleaseTagRequired=$RequireReleaseTag.IsPresent
    Toolchain=$properties; CompilerFileVersion=(Get-Item -LiteralPath $compiler).VersionInfo.FileVersion
    ResolvedWindowsSDKVersion=$sdkVersion
    Platform='x64'; Configuration='Release'; Runtime='/MD'; Language='/std:c++latest'
    UTF8=$true; Optimization='/O2'; WholeProgramOptimization=$false; Mimalloc=$false
    SourceFiles='source-files.json'; FileHashes='SHA256SUMS.txt'
    Verification='ZIP-only consumer build and RIO start/stop on the packaging host'
}
$manifest | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 (Join-Path $stage 'package-manifest.json')
$hashLines = Get-ChildItem -LiteralPath $stage -Recurse -File | Sort-Object FullName | ForEach-Object {
    $relative = $_.FullName.Substring($stage.Length + 1).Replace('\', '/')
    "$( (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash )  $relative"
}
$hashLines | Set-Content -Encoding ASCII (Join-Path $stage 'SHA256SUMS.txt')
$zip = "$stage.zip"
Compress-Archive -Path "$stage\*" -DestinationPath $zip -Force
$zipHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $zip).Hash
Set-Content -Encoding ASCII -LiteralPath "$zip.sha256" -Value "$zipHash  $([IO.Path]::GetFileName($zip))"

# Verify the final ZIP, not just the staging directory. No repository or vcpkg
# include/lib paths are allowed in the consumer build.
Expand-Archive -LiteralPath $zip -DestinationPath $verification -Force
$consumer = Join-Path $verification 'samples\package_consumer\PackageConsumer.vcxproj'
$consumerLog = Join-Path $verification 'consumer-build.log'
& $msbuild $consumer /nologo /m /t:Rebuild /p:Configuration=Release /p:Platform=x64 `
    "/p:WindowsTargetPlatformVersion=$sdkVersion" `
    /p:VcpkgEnabled=false /p:VcpkgEnableManifest=false /p:VcpkgApplocalDeps=false *> $consumerLog
if ($LASTEXITCODE -ne 0) { throw "ZIP-only consumer build failed: $consumerLog" }
$consumerExe = Join-Path $verification 'samples\package_consumer\bin\PackageConsumer.exe'
& $consumerExe
if ($LASTEXITCODE -ne 0) { throw 'ZIP-only consumer RIO start/stop failed.' }
if ((Get-SnapshotHash @(Get-SourceSnapshot)) -ne $sourceHash) { throw 'Working source changed during packaging; candidate is stale.' }
[pscustomobject]@{ Passed=$true; Zip=$zip; ZipSHA256=$zipHash; SourceTreeSHA256=$sourceHash; ConsumerExitCode=0 } |
    ConvertTo-Json | Set-Content -Encoding UTF8 (Join-Path $verification 'verification.json')
Write-Host "Package verified: $zip"
Write-Host "Source tree SHA256: $sourceHash"
Write-Host "ZIP SHA256: $zipHash"
