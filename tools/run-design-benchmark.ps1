param(
    [int]$Sessions = 256,
    [int]$BytesPerBuffer = 65536,
    [int]$Runs = 5,
    [int]$SendIterations = 200000,
    [string]$OutputDirectory = ''
)

$ErrorActionPreference = 'Stop'
if ($Sessions -lt 1 -or $BytesPerBuffer -lt 1 -or $Runs -lt 1 -or
    $SendIterations -lt 1 -or
    2L * $Sessions * $BytesPerBuffer -gt [uint32]::MaxValue) {
    throw 'A design benchmark argument is outside its valid range.'
}
$root = Split-Path -Parent $PSScriptRoot
$commit = (& git -C $root rev-parse --short HEAD).Trim()
if (-not $OutputDirectory) {
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $OutputDirectory = Join-Path $root "benchmarks\results\$stamp-$commit-design"
}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} `
    'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products * -version '[18.0,19.0)' `
    -requires Microsoft.Component.MSBuild -property installationPath
if (-not $installation) { throw 'Visual Studio 2026 was not found.' }
$msbuild = Join-Path $installation 'MSBuild\Current\Bin\amd64\MSBuild.exe'
$project = Join-Path $root 'tools\design_benchmark\DesignBenchmark.vcxproj'
& $msbuild $project /nologo /m /t:Rebuild /p:Configuration=Release `
    /p:Platform=x64
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$result = Join-Path $OutputDirectory 'design.json'
& (Join-Path $root 'bin\x64\Release\DesignBenchmark.exe') `
    --sessions $Sessions --bytes $BytesPerBuffer --runs $Runs `
    --send-iterations $SendIterations --json $result
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
@{
    Commit = $commit
    Timestamp = (Get-Date).ToUniversalTime().ToString('o')
    Computer = $env:COMPUTERNAME
    Processor = $env:PROCESSOR_IDENTIFIER
    LogicalProcessors = [Environment]::ProcessorCount
    Configuration = 'Release'
} | ConvertTo-Json | Set-Content -Encoding UTF8 `
    (Join-Path $OutputDirectory 'environment.json')
Write-Host "Design results: $OutputDirectory"
