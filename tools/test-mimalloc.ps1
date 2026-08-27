param(
    [int]$Port = 32482,
    [int]$Runs = 3,
    [int]$DurationSeconds = 2,
    [int]$Connections = 32,
    [string]$OutputDirectory = ''
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (-not $OutputDirectory) {
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $OutputDirectory = Join-Path $root "benchmarks\results\$stamp-mimalloc"
}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$rows = @()
foreach ($enabled in @($false, $true)) {
    $arguments = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
        (Join-Path $PSScriptRoot 'build.ps1'), '-Configuration', 'Release', '-Rebuild', '-SkipTests')
    if ($enabled) { $arguments += '-UseMimalloc' }
    & powershell.exe @arguments
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & (Join-Path $root 'bin\x64\Release\ChatServer.exe') --port $Port --duration 1
    if ($LASTEXITCODE -ne 0) { throw "ChatServer failed with UseMimalloc=$enabled" }
    for ($run = 1; $run -le $Runs; ++$run) {
        $server = Start-Process -FilePath (Join-Path $root 'bin\x64\Release\EchoServer.exe') `
            -ArgumentList '--port', $Port, '--duration', ($DurationSeconds + 5) -WindowStyle Hidden -PassThru
        try {
            Start-Sleep -Milliseconds 750
            $jsonPath = Join-Path $OutputDirectory "mimalloc-$enabled-$run.json"
            & (Join-Path $root 'bin\x64\Release\LoadClient.exe') --port $Port `
                --connections $Connections --duration $DurationSeconds --scenario echo `
                --payload 128 --seed 1511506142 --json $jsonPath
            if ($LASTEXITCODE -ne 0) { throw "mimalloc=$enabled run $run failed" }
            $result = Get-Content -Raw -Encoding UTF8 $jsonPath | ConvertFrom-Json
            if ($result.data_errors -ne 0 -or $result.dropped_packets -ne 0) {
                throw "mimalloc=$enabled run $run reported correctness errors"
            }
            $rows += [pscustomobject]@{
                UseMimalloc = $enabled
                Run = $run
                PacketsPerSecond = $result.packets_per_second
                P50Us = $result.p50_us
                P99Us = $result.p99_us
                CpuMs = $result.kernel_cpu_ms + $result.user_cpu_ms
                DataErrors = $result.data_errors
                DroppedPackets = $result.dropped_packets
            }
        } finally {
            if (-not $server.HasExited) { Stop-Process -Id $server.Id }
        }
    }
}
$rows | Export-Csv -NoTypeInformation -Encoding UTF8 (Join-Path $OutputDirectory 'summary.csv')
$rows | Format-Table -AutoSize
Write-Host "mimalloc off/on lifecycle and benchmark results: $OutputDirectory"
