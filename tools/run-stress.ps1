param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [int]$DurationSeconds = 600,
    [int]$Connections = 64,
    [int]$Port = 32462,
    [int]$Seed = 1511506142,
    [int]$ScenarioSeconds = 30,
    [int]$ResourceSampleSeconds = 10,
    [int]$MaxSessions = 1024,
    [int]$MaxHandleGrowth = 8,
    [int]$MaxThreadGrowth = 4,
    [int64]$MaxPrivateByteGrowth = 16777216,
    [ValidateSet('on', 'off')][string]$Observability = 'off',
    [string]$OutputDirectory = ''
)

$ErrorActionPreference = 'Stop'
if ($DurationSeconds -lt 1 -or $Connections -lt 1 -or $Port -lt 1 -or
    $Port -gt 65535 -or $ScenarioSeconds -lt 1 -or
    $ResourceSampleSeconds -lt 1 -or $MaxSessions -lt $Connections -or
    $MaxHandleGrowth -lt 0 -or $MaxThreadGrowth -lt 0 -or
    $MaxPrivateByteGrowth -lt 0) {
    throw 'A stress argument is outside its valid range.'
}

# Some automation hosts inject both Path and PATH. Start-Process rejects the
# duplicate environment keys, so normalize them before launching helpers.
$taskPath = $env:Path
Remove-Item Env:Path -ErrorAction SilentlyContinue
Remove-Item Env:PATH -ErrorAction SilentlyContinue
$env:Path = $taskPath

function Get-Median([object[]]$Values) {
    if ($Values.Count -eq 0) { return 0 }
    $ordered = @($Values | Sort-Object)
    return [double]$ordered[[Math]::Floor(($ordered.Count - 1) / 2)]
}

function Wait-ForQuiescence {
    $idleDeadline = (Get-Date).AddSeconds(5)
    do {
        $connectionsLeft = @([Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveTcpConnections() |
            Where-Object { $_.LocalEndPoint.Port -eq $Port -and
                $_.State -notin @([Net.NetworkInformation.TcpState]::TimeWait,
                    [Net.NetworkInformation.TcpState]::Closed,
                    [Net.NetworkInformation.TcpState]::Listen) }).Count
        if ($connectionsLeft -eq 0) {
            # Allow the owner workers to retire completions after socket close.
            Start-Sleep -Milliseconds 500
            return
        }
        Start-Sleep -Milliseconds 100
    } while ((Get-Date) -lt $idleDeadline)
    throw 'Server did not return to the quiescent state after a scenario.'
}

$root = Split-Path -Parent $PSScriptRoot
if (-not $OutputDirectory) {
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $OutputDirectory = Join-Path $root "artifacts\stress\$stamp"
}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$echo = Join-Path $root "bin\x64\$Configuration\EchoServer.exe"
$client = Join-Path $root "bin\x64\$Configuration\LoadClient.exe"
if (-not (Test-Path $echo) -or -not (Test-Path $client)) {
    throw "Build $Configuration before running stress tests."
}
[pscustomobject]@{
    OS=[Environment]::OSVersion.VersionString
    ServerSHA256=(Get-FileHash -Algorithm SHA256 -LiteralPath $echo).Hash
    ClientSHA256=(Get-FileHash -Algorithm SHA256 -LiteralPath $client).Hash
    SourceCommit=(& git -C $root rev-parse HEAD)
    SourceStatus=@(& git -C $root status --porcelain)
    Configuration=$Configuration; Observability=$Observability
    MaxSessions=$MaxSessions; Connections=$Connections; Seed=$Seed
} | ConvertTo-Json -Depth 4 | Set-Content -Encoding UTF8 `
    (Join-Path $OutputDirectory 'environment.json')

$scenarios = @('echo', 'split', 'coalesce', 'half-close', 'slow-read',
    'abrupt-close', 'invalid-length', 'no-read')
$scenarioDuration = [Math]::Max(1, [Math]::Min($ScenarioSeconds,
    [Math]::Floor($DurationSeconds / $scenarios.Count)))
$stopFile = Join-Path $OutputDirectory 'stop-server.signal'
$serverStdout = Join-Path $OutputDirectory 'server.stdout.log'
$serverStderr = Join-Path $OutputDirectory 'server.stderr.log'
if (Test-Path -LiteralPath $stopFile) { Remove-Item -LiteralPath $stopFile -Force }
$server = Start-Process -FilePath $echo -ArgumentList '--port', $Port,
    '--duration', 0, '--observability', $Observability, '--max-sessions', $MaxSessions,
    '--stop-file', $stopFile -RedirectStandardOutput $serverStdout `
    -RedirectStandardError $serverStderr -WindowStyle Hidden -PassThru
$null = $server.Handle

$samples = @()
$results = @()
$idleSamples = @()
$warmupResults = @()
$startedAt = Get-Date
$deadline = [DateTime]::MaxValue
$iteration = 0
$completed = $false
$serverForcedTermination = $false
$serverExitCode = $null
try {
    Start-Sleep -Milliseconds 750
    # Warm every provider path before the measured interval. Warmup results are
    # retained separately and never count as scenario coverage or soak time.
    foreach ($scenario in $scenarios) {
        $json = Join-Path $OutputDirectory "warmup-$scenario.json"
        & $client --port $Port --connections $Connections --duration 1 `
            --scenario $scenario --seed $Seed --json $json *> `
            (Join-Path $OutputDirectory "warmup-$scenario.log")
        if ($LASTEXITCODE -ne 0) { throw "Warmup $scenario failed." }
        $result = Get-Content -Raw -Encoding UTF8 $json | ConvertFrom-Json
        if ($result.connection_failures -ne 0 -or $result.data_errors -ne 0 -or
            $result.dropped_packets -ne 0 -or $result.timeouts -ne 0) {
            throw "Warmup $scenario reported a correctness failure."
        }
        $warmupResults += $result
        Wait-ForQuiescence
    }
    $startedAt = Get-Date
    $deadline = $startedAt.AddSeconds($DurationSeconds)
    while ((Get-Date) -lt $deadline) {
        foreach ($scenario in $scenarios) {
            $remaining = [Math]::Floor(($deadline - (Get-Date)).TotalSeconds)
            if ($remaining -lt 1) { break }
            ++$iteration
            $runDuration = [Math]::Max(1, [Math]::Min($scenarioDuration, $remaining))
            $prefix = '{0:D4}-{1}' -f $iteration, $scenario
            $json = Join-Path $OutputDirectory "$prefix.json"
            $stdout = Join-Path $OutputDirectory "$prefix.stdout.log"
            $stderr = Join-Path $OutputDirectory "$prefix.stderr.log"
            $arguments = @('--port', $Port, '--connections', $Connections,
                '--duration', $runDuration, '--scenario', $scenario,
                '--seed', $Seed, '--json', $json)
            $clientProcess = Start-Process -FilePath $client -ArgumentList $arguments `
                -RedirectStandardOutput $stdout -RedirectStandardError $stderr `
                -WindowStyle Hidden -PassThru
            $null = $clientProcess.Handle
            do {
                $process = Get-Process -Id $server.Id -ErrorAction Stop
                $samples += [pscustomobject]@{
                    Timestamp=(Get-Date).ToUniversalTime().ToString('o')
                    ElapsedSeconds=[Math]::Round(((Get-Date) - $startedAt).TotalSeconds, 3)
                    Iteration=$iteration; Scenario=$scenario
                    Handles=$process.HandleCount; Threads=$process.Threads.Count
                    PrivateBytes=$process.PrivateMemorySize64
                    CommittedBytes=$process.PrivateMemorySize64
                    WorkingSetBytes=$process.WorkingSet64
                    ExpectedRegisteredBuffers=2 * $MaxSessions
                }
            } while (-not $clientProcess.WaitForExit($ResourceSampleSeconds * 1000))
            $clientProcess.WaitForExit()
            $clientProcess.Refresh()
            if (Test-Path $stdout) { Get-Content -Encoding UTF8 $stdout | Write-Host }
            if ($clientProcess.ExitCode -ne 0) {
                $errorText = if (Test-Path $stderr) {
                    Get-Content -Raw -Encoding UTF8 $stderr
                } else { '' }
                throw "$scenario failed with exit code $($clientProcess.ExitCode): $errorText"
            }
            $result = Get-Content -Raw -Encoding UTF8 $json | ConvertFrom-Json
            $invalid = $result.connection_failures -ne 0 -or
                $result.data_errors -ne 0 -or $result.dropped_packets -ne 0 -or
                $result.timeouts -ne 0
            if ($invalid) { throw "$scenario reported a correctness failure" }
            $results += [pscustomobject]@{
                Iteration=$iteration; Scenario=$scenario; DurationSeconds=$runDuration
                Connected=$result.connected; SentPackets=$result.sent_packets
                ReceivedPackets=$result.received_packets
                ConnectionFailures=$result.connection_failures
                DataErrors=$result.data_errors; DroppedPackets=$result.dropped_packets
                Timeouts=$result.timeouts; Seed=$result.seed
            }
            Wait-ForQuiescence
            $process = Get-Process -Id $server.Id -ErrorAction Stop
            $idleSamples += [pscustomobject]@{
                ElapsedSeconds=[Math]::Round(((Get-Date) - $startedAt).TotalSeconds, 3)
                Iteration=$iteration; Scenario=$scenario
                Handles=$process.HandleCount; Threads=$process.Threads.Count
                PrivateBytes=$process.PrivateMemorySize64
            }
        }
    }
    $completed = $true
} finally {
    Set-Content -Encoding ASCII -Path $stopFile -Value 'stop'
    if (-not $server.HasExited -and -not $server.WaitForExit(30000)) {
        $serverForcedTermination = $true
        Stop-Process -Id $server.Id -Force
        $server.WaitForExit()
    }
    $server.Refresh()
    $serverExitCode = $server.ExitCode
    [pscustomobject]@{
        ForcedTermination=$serverForcedTermination; ExitCode=$serverExitCode
        Passed=(-not $serverForcedTermination -and $serverExitCode -eq 0)
    } | ConvertTo-Json | Set-Content -Encoding UTF8 `
        (Join-Path $OutputDirectory 'server-exit.json')
    $samples | Export-Csv -NoTypeInformation -Encoding UTF8 `
        (Join-Path $OutputDirectory 'resources.csv')
    $results | Export-Csv -NoTypeInformation -Encoding UTF8 `
        (Join-Path $OutputDirectory 'summary.csv')
    $idleSamples | Export-Csv -NoTypeInformation -Encoding UTF8 `
        (Join-Path $OutputDirectory 'quiescent-resources.csv')
    $warmupResults | ConvertTo-Json -Depth 4 | Set-Content -Encoding UTF8 `
        (Join-Path $OutputDirectory 'warmup-results.json')
}

if (-not $completed) { throw 'Stress run did not complete.' }
$orderedSamples = @($idleSamples | Sort-Object ElapsedSeconds)
$quartileCount = [Math]::Max(1, [Math]::Floor($orderedSamples.Count / 4))
$first = @($orderedSamples | Select-Object -First $quartileCount)
$last = @($orderedSamples | Select-Object -Last $quartileCount)
$firstHandles = Get-Median @($first.Handles)
$lastHandles = Get-Median @($last.Handles)
$firstThreads = Get-Median @($first.Threads)
$lastThreads = Get-Median @($last.Threads)
$firstPrivate = Get-Median @($first.PrivateBytes)
$lastPrivate = Get-Median @($last.PrivateBytes)
$uniqueScenarios = @($results | Select-Object -ExpandProperty Scenario -Unique)
$minimumCoverage = @($results | Group-Object Scenario | Where-Object Count -lt 2).Count -eq 0
$sufficientSamples = $samples.Count -ge 32 -and $orderedSamples.Count -ge 16 -and
    $uniqueScenarios.Count -eq $scenarios.Count -and $minimumCoverage
$trendPassed = $sufficientSamples -and
    (($lastHandles - $firstHandles) -le $MaxHandleGrowth -and
     ($lastThreads - $firstThreads) -le $MaxThreadGrowth -and
     ($lastPrivate - $firstPrivate) -le $MaxPrivateByteGrowth)
$correctnessFailures = @($results | Where-Object {
    $_.ConnectionFailures -ne 0 -or $_.DataErrors -ne 0 -or
    $_.DroppedPackets -ne 0 -or $_.Timeouts -ne 0
}).Count
$analysis = [pscustomobject]@{
    StartedAt=$startedAt.ToUniversalTime().ToString('o')
    FinishedAt=(Get-Date).ToUniversalTime().ToString('o')
    RequestedDurationSeconds=$DurationSeconds; ScenarioSeconds=$scenarioDuration
    Connections=$Connections; Seed=$Seed; Iterations=$results.Count
    ResourceSamples=$samples.Count; QuiescentSamples=$orderedSamples.Count
    WarmupScenarios=$warmupResults.Count; TrendBasis='post-warmup quiescent samples'
    SufficientSamples=$sufficientSamples
    FirstQuartileMedianHandles=$firstHandles; LastQuartileMedianHandles=$lastHandles
    HandleGrowth=$lastHandles - $firstHandles; MaxHandleGrowth=$MaxHandleGrowth
    FirstQuartileMedianThreads=$firstThreads; LastQuartileMedianThreads=$lastThreads
    ThreadGrowth=$lastThreads - $firstThreads; MaxThreadGrowth=$MaxThreadGrowth
    FirstQuartileMedianPrivateBytes=$firstPrivate
    LastQuartileMedianPrivateBytes=$lastPrivate
    PrivateByteGrowth=$lastPrivate - $firstPrivate
    MaxPrivateByteGrowth=$MaxPrivateByteGrowth
    ExpectedRegisteredBuffers=2 * $MaxSessions
    ServerExitCode=$serverExitCode; ServerForcedTermination=$serverForcedTermination
    CorrectnessFailures=$correctnessFailures; TrendPassed=$trendPassed
}
$analysis | ConvertTo-Json | Set-Content -Encoding UTF8 `
    (Join-Path $OutputDirectory 'resource-analysis.json')
if ($serverForcedTermination -or $serverExitCode -ne 0) { throw 'Server shutdown did not pass.' }
if (-not $sufficientSamples) { throw 'Insufficient samples/scenario coverage for the resource gate.' }
if (-not $trendPassed) { throw 'Server resource growth exceeded the configured limit.' }
Write-Host "Stress results: $OutputDirectory"
