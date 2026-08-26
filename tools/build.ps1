param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [switch]$Rebuild,
    [switch]$SkipTests
)

$ErrorActionPreference = 'Stop'

$vswhere = Join-Path ${env:ProgramFiles(x86)} `
    'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw 'vswhere.exe was not found. Install Visual Studio 2026.'
}

$installation = & $vswhere -latest -products * -version '[18.0,19.0)' `
    -requires Microsoft.Component.MSBuild -property installationPath
if (-not $installation) {
    throw 'Visual Studio 2026 with MSBuild was not found.'
}

$msbuild = Join-Path $installation 'MSBuild\Current\Bin\amd64\MSBuild.exe'
if (-not (Test-Path -LiteralPath $msbuild)) {
    throw "MSBuild was not found: $msbuild"
}

# Some automation hosts inject both Path and PATH. MSBuild treats those names
# as duplicate keys when it creates cl.exe, so normalize the key first.
$taskPath = $env:Path
Remove-Item Env:Path -ErrorAction SilentlyContinue
Remove-Item Env:PATH -ErrorAction SilentlyContinue
$env:Path = $taskPath

$target = if ($Rebuild) { 'Rebuild' } else { 'Build' }
& $msbuild (Join-Path $PSScriptRoot '..\RIONetLite.sln') /m "/t:$target" `
    "/p:Configuration=$Configuration" /p:Platform=x64 /nologo
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if (-not $SkipTests) {
    $testDirectory = Join-Path $PSScriptRoot `
        "..\bin\tests\x64\$Configuration"
    $tests = @(
        'RIONetLite.UnitTests.exe',
        'RIONetLite.IntegrationTests.exe'
    )
    foreach ($test in $tests) {
        & (Join-Path $testDirectory $test) --gtest_color=no
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }
}
