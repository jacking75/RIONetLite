param([switch]$Fix)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$formatter = Get-Command clang-format -ErrorAction SilentlyContinue
if ($formatter) {
    $formatter = $formatter.Source
} else {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $installation = & $vswhere -latest -products * -version '[18.0,19.0)' -property installationPath
    $formatter = Join-Path $installation 'VC\Tools\Llvm\x64\bin\clang-format.exe'
}
if (-not (Test-Path -LiteralPath $formatter)) { throw 'clang-format was not found.' }
$files = Get-ChildItem -Path (Join-Path $root 'include'), (Join-Path $root 'src'), `
    (Join-Path $root 'RIONetLite'), (Join-Path $root 'tests'), `
    (Join-Path $root 'samples'), (Join-Path $root 'tools\load_client'), `
    (Join-Path $root 'tools\design_benchmark') `
    -Recurse -File -Include *.h,*.cpp |
    Where-Object { $_.FullName -notlike '*\samples\thirdparty\*' } |
    ForEach-Object { $_.FullName }

if ($Fix) {
    & $formatter -i --style=file -- $files
} else {
    & $formatter --dry-run --Werror --style=file -- $files
}
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Push-Location $root
try { & git diff --check } finally { Pop-Location }
exit $LASTEXITCODE
