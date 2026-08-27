param(
    [int]$Port = 32578,
    [int]$Connections = 16,
    [int]$ScenarioSeconds = 1,
    [int]$Seed = 1511506142,
    [string]$OutputDirectory = ''
)

$ErrorActionPreference = 'Stop'
if ($Port -lt 1 -or $Port -gt 65535 -or $Connections -lt 1 -or
    $ScenarioSeconds -lt 1) {
    throw 'A memory-safety argument is outside its valid range.'
}

$root = Split-Path -Parent $PSScriptRoot
$artifacts_root = [IO.Path]::GetFullPath((Join-Path $root 'artifacts'))
if (-not $OutputDirectory) {
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $OutputDirectory = Join-Path $artifacts_root "memory-safety\$stamp-asan-rio"
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (-not $OutputDirectory.StartsWith(
        $artifacts_root, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Memory-safety output must stay under artifacts.'
}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null

$vswhere = Join-Path ${env:ProgramFiles(x86)} `
    'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products * -version '[18.0,19.0)' `
    -requires Microsoft.Component.MSBuild -property installationPath
if (-not $installation) { throw 'Visual Studio 2026 was not found.' }
$msbuild = Join-Path $installation 'MSBuild\Current\Bin\amd64\MSBuild.exe'
$runtime = Get-ChildItem -Path (Join-Path $installation 'VC\Tools\MSVC') `
    -Recurse -Filter 'clang_rt.asan_dynamic-x86_64.dll' |
    Where-Object FullName -Like '*\Hostx64\x64\*' |
    Sort-Object FullName -Descending | Select-Object -First 1
if (-not $runtime) { throw 'The MSVC x64 AddressSanitizer runtime was not found.' }

$build_root = Join-Path $OutputDirectory 'build'
$out_dir = Join-Path $build_root 'bin\'
$int_dir = Join-Path $build_root 'obj\'
$library_project = Join-Path $root 'build\RIONetLite.vcxproj'
$project = Join-Path $root 'samples\echo_server\EchoServer.vcxproj'
& $msbuild $library_project /nologo /m:1 /t:Rebuild /p:Configuration=Debug `
    /p:Platform=x64 /p:EnableASAN=true /p:UseMimalloc=false `
    /p:BasicRuntimeChecks=Default /p:SupportJustMyCode=false `
    "/p:OutDir=$out_dir" "/p:RIONetLiteIsolatedIntRoot=$int_dir" 2>&1 |
    Tee-Object -FilePath (Join-Path $OutputDirectory 'build.log')
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $msbuild $project /nologo /m:1 /t:Rebuild /p:Configuration=Debug `
    /p:Platform=x64 /p:BuildProjectReferences=false /p:EnableASAN=true `
    /p:UseMimalloc=false /p:BasicRuntimeChecks=Default `
    /p:SupportJustMyCode=false "/p:OutDir=$out_dir" `
    "/p:RIONetLiteIsolatedIntRoot=$int_dir" 2>&1 |
    Tee-Object -Append -FilePath (Join-Path $OutputDirectory 'build.log')
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# Start-Process rejects environments containing both Path and PATH.
$task_path = $env:Path
Remove-Item Env:Path -ErrorAction SilentlyContinue
Remove-Item Env:PATH -ErrorAction SilentlyContinue
$env:Path = "$($runtime.DirectoryName);$task_path"
$env:ASAN_OPTIONS = 'halt_on_error=1:abort_on_error=1:print_stats=1'

$echo = Join-Path $out_dir 'EchoServer.exe'
$client = Join-Path $root 'bin\x64\Release\LoadClient.exe'
if (-not (Test-Path $client)) { throw 'Build Release before memory-safety tests.' }
$stop_file = Join-Path $OutputDirectory 'stop-server.signal'
$server_stdout = Join-Path $OutputDirectory 'server.stdout.log'
$server_stderr = Join-Path $OutputDirectory 'server.stderr.log'
$server = Start-Process -FilePath $echo -ArgumentList '--port', $Port,
    '--duration', 0, '--observability', 'off', '--max-sessions', 128,
    '--stop-file', $stop_file -RedirectStandardOutput $server_stdout `
    -RedirectStandardError $server_stderr -WindowStyle Hidden -PassThru
$null = $server.Handle

$scenarios = @('echo', 'split', 'coalesce', 'half-close', 'slow-read',
    'abrupt-close', 'invalid-length', 'no-read')
$summary = @()
$server_exit = $null
try {
    Start-Sleep -Milliseconds 750
    foreach ($scenario in $scenarios) {
        $json = Join-Path $OutputDirectory "$scenario.json"
        $stdout = Join-Path $OutputDirectory "$scenario.stdout.log"
        $stderr = Join-Path $OutputDirectory "$scenario.stderr.log"
        $arguments = @('--port', $Port, '--connections', $Connections,
            '--duration', $ScenarioSeconds, '--scenario', $scenario,
            '--seed', $Seed, '--json', $json)
        $process = Start-Process -FilePath $client -ArgumentList $arguments `
            -RedirectStandardOutput $stdout -RedirectStandardError $stderr `
            -WindowStyle Hidden -Wait -PassThru
        if ($process.ExitCode -ne 0) {
            throw "$scenario failed with exit code $($process.ExitCode)."
        }
        $result = Get-Content -Raw -Encoding UTF8 $json | ConvertFrom-Json
        if ($result.connection_failures -ne 0 -or
            $result.data_errors -ne 0 -or $result.dropped_packets -ne 0 -or
            $result.timeouts -ne 0) {
            throw "$scenario reported a correctness failure."
        }
        $summary += [pscustomobject]@{
            Scenario=$scenario; ExitCode=$process.ExitCode
            Sent=$result.sent_packets; Received=$result.received_packets
            DataErrors=$result.data_errors; Timeouts=$result.timeouts
        }
    }
    if ($server.HasExited) {
        throw "AddressSanitizer server exited early: $($server.ExitCode)."
    }
} finally {
    Set-Content -Encoding ASCII -Path $stop_file -Value 'stop'
    if (-not $server.WaitForExit(30000)) {
        Stop-Process -Id $server.Id
    }
    $server.WaitForExit()
    $server_exit = $server.ExitCode
}

$asan_text = if (Test-Path $server_stderr) {
    Get-Content -Raw -Encoding UTF8 $server_stderr
} else { '' }
if ($server_exit -ne 0) { throw "ASan server exit code was $server_exit." }
if ($asan_text -match 'ERROR: AddressSanitizer') {
    throw 'AddressSanitizer reported an error.'
}
$summary | Export-Csv -NoTypeInformation -Encoding UTF8 `
    (Join-Path $OutputDirectory 'summary.csv')
@(
    'PASS'
    'Tool=MSVC AddressSanitizer x64'
    "Scenarios=$($scenarios -join ',')"
    "ServerExit=$server_exit"
    'AddressSanitizerErrors=0'
) | Set-Content -Encoding UTF8 (Join-Path $OutputDirectory 'result.txt')
Write-Host "AddressSanitizer RIO results: $OutputDirectory"
