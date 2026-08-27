param([string]$Version = '0.2.0')

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'verify-version.ps1') -ExpectedVersion $Version
$artifactsRoot = [IO.Path]::GetFullPath((Join-Path $root 'artifacts'))
$stage = [IO.Path]::GetFullPath((Join-Path $artifactsRoot "RIONetLite-$Version"))
if (-not $stage.StartsWith($artifactsRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid package path.' }
if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path (Join-Path $stage 'include'), `
    (Join-Path $stage 'lib'), (Join-Path $stage 'samples\package_consumer') | Out-Null
Copy-Item -Recurse (Join-Path $root 'include\rionetlite') (Join-Path $stage 'include')
Copy-Item (Join-Path $root 'bin\lib\x64\Release\RIONetLite.lib') (Join-Path $stage 'lib')
Copy-Item (Join-Path $root 'bin\lib\x64\Release\RIONetLite.pdb') (Join-Path $stage 'lib')
Copy-Item (Join-Path $root 'LICENSE'), (Join-Path $root 'CHANGELOG.md') $stage
Copy-Item (Join-Path $root 'samples\package_consumer\main.cpp'), `
    (Join-Path $root 'samples\package_consumer\PackageConsumer.vcxproj') `
    (Join-Path $stage 'samples\package_consumer')
Set-Content -Encoding ASCII -Path (Join-Path $stage 'VERSION') -Value $Version

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products * -version '[18.0,19.0)' -requires Microsoft.Component.MSBuild -property installationPath
$msbuild = Join-Path $installation 'MSBuild\Current\Bin\amd64\MSBuild.exe'
& $msbuild (Join-Path $stage 'samples\package_consumer\PackageConsumer.vcxproj') `
    /nologo /m /t:Rebuild /p:Configuration=Release /p:Platform=x64
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Remove-Item -LiteralPath (Join-Path $stage 'samples\package_consumer\bin'), `
    (Join-Path $stage 'samples\package_consumer\obj') -Recurse -Force
Compress-Archive -Path "$stage\*" -DestinationPath "$stage.zip" -Force
Write-Host "Package: $stage.zip"
