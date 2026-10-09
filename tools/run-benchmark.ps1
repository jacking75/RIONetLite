param(
    [ValidateSet('matrix', 'suite')][string]$Profile = 'matrix',
    [int]$Runs = 5,
    [int]$DurationSeconds = 5,
    [int]$Connections = 32,
    [int]$Port = 32472,
    [string]$WaitModes = 'adaptive',
    [string]$BatchSizes = '64',
    [string]$ObservabilityModes = 'off',
    [string]$AffinityModes = 'off',
    [int]$WarmupSeconds = 1,
    [int]$CooldownMilliseconds = 250,
    [switch]$CaptureEtw,
    [string]$BaselineCsv = '',
    [double]$MaxThroughputRegressionPercent = 5.0,
    [double]$MaxP99RegressionPercent = 10.0,
    [string]$OutputDirectory = ''
)

$ErrorActionPreference = 'Stop'
if ($Runs -lt 1 -or $DurationSeconds -lt 1 -or $Connections -lt 1 -or
    $WarmupSeconds -lt 0 -or $CooldownMilliseconds -lt 0 -or
    $MaxThroughputRegressionPercent -lt 0 -or $MaxP99RegressionPercent -lt 0 -or
    $Port -lt 1 -or $Port -gt 65535) {
    throw 'A benchmark argument is outside its valid range.'
}

$root = Split-Path -Parent $PSScriptRoot
$commit = (& git -C $root rev-parse --short HEAD).Trim()
if ($BaselineCsv) {
    $BaselineCsv = [IO.Path]::GetFullPath($BaselineCsv)
    if (-not (Test-Path -LiteralPath $BaselineCsv)) {
        throw "Baseline CSV does not exist: $BaselineCsv"
    }
    $baseline_directory = Split-Path -Parent $BaselineCsv
    $baseline_environment_path = Join-Path $baseline_directory 'environment.json'
    $approval_path = Join-Path $baseline_directory 'approval.json'
    if (-not (Test-Path -LiteralPath $baseline_environment_path) -or
        -not (Test-Path -LiteralPath $approval_path)) {
        throw 'A fixed-machine baseline requires environment.json and approval.json.'
    }
    $baseline_environment = Get-Content -Raw -Encoding UTF8 `
        $baseline_environment_path | ConvertFrom-Json
    $approval = Get-Content -Raw -Encoding UTF8 $approval_path | ConvertFrom-Json
    $machine_matches = $approval.Status -eq 'approved' -and
        $approval.Machine -eq $env:COMPUTERNAME -and
        $approval.Processor -eq $env:PROCESSOR_IDENTIFIER -and
        [int]$approval.LogicalProcessors -eq [Environment]::ProcessorCount -and
        $baseline_environment.Computer -eq $env:COMPUTERNAME -and
        $baseline_environment.Processor -eq $env:PROCESSOR_IDENTIFIER -and
        [int]$baseline_environment.LogicalProcessors -eq
            [Environment]::ProcessorCount
    if (-not $machine_matches) {
        throw 'The current host does not match the approved fixed-machine baseline.'
    }
    $settings_match = $baseline_environment.Profile -eq $Profile -and
        [int]$baseline_environment.Runs -eq $Runs -and
        [int]$baseline_environment.DurationSeconds -eq $DurationSeconds -and
        [int]$baseline_environment.WarmupSeconds -eq $WarmupSeconds -and
        [int]$baseline_environment.CooldownMilliseconds -eq
            $CooldownMilliseconds
    if (-not $settings_match) {
        throw 'Benchmark settings do not match the approved baseline.'
    }
}
if (-not $OutputDirectory) {
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $OutputDirectory = Join-Path $root "artifacts\benchmarks\$stamp-$commit"
}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null

$cases = @()
if ($Profile -eq 'matrix') {
    foreach ($waitMode in $WaitModes.Split(',')) {
        foreach ($batchText in $BatchSizes.Split(',')) {
            foreach ($observability in $ObservabilityModes.Split(',')) {
                $mode = $observability.Trim()
                if ($mode -ne 'on' -and $mode -ne 'off') {
                    throw 'ObservabilityModes accepts only on or off.'
                }
                foreach ($affinityText in $AffinityModes.Split(',')) {
                    $affinity = $affinityText.Trim()
                    if ($affinity -ne 'on' -and $affinity -ne 'off') {
                        throw 'AffinityModes accepts only on or off.'
                    }
                    $cases += [pscustomobject]@{
                        Name = "wait-batch-affinity-$affinity"
                        WaitMode = $waitMode.Trim(); Batch = [int]$batchText
                        Observability = $mode; Affinity = $affinity
                        ClientScenario = 'echo'; Payload = 128
                        Connections = $Connections; Rate = 0; Reconnect = 0
                    }
                }
            }
        }
    }
} else {
    $cases = @(
        [pscustomobject]@{ Name='latency-32'; WaitMode='adaptive'; Batch=64; Observability='off'; Affinity='off'; ClientScenario='echo'; Payload=32; Connections=4; Rate=0; Reconnect=0 },
        [pscustomobject]@{ Name='latency-128'; WaitMode='adaptive'; Batch=64; Observability='off'; Affinity='off'; ClientScenario='echo'; Payload=128; Connections=4; Rate=0; Reconnect=0 },
        [pscustomobject]@{ Name='throughput-1k'; WaitMode='adaptive'; Batch=64; Observability='off'; Affinity='off'; ClientScenario='echo'; Payload=1008; Connections=$Connections; Rate=0; Reconnect=0 },
        [pscustomobject]@{ Name='throughput-4k'; WaitMode='adaptive'; Batch=64; Observability='off'; Affinity='off'; ClientScenario='echo'; Payload=4080; Connections=$Connections; Rate=0; Reconnect=0 },
        [pscustomobject]@{ Name='throughput-16k'; WaitMode='adaptive'; Batch=64; Observability='off'; Affinity='off'; ClientScenario='echo'; Payload=16368; Connections=$Connections; Rate=0; Reconnect=0 },
        [pscustomobject]@{ Name='fan-in'; WaitMode='adaptive'; Batch=64; Observability='off'; Affinity='off'; ClientScenario='echo'; Payload=32; Connections=[Math]::Max(128, $Connections); Rate=10; Reconnect=0 },
        [pscustomobject]@{ Name='burst'; WaitMode='adaptive'; Batch=64; Observability='off'; Affinity='off'; ClientScenario='coalesce'; Payload=128; Connections=$Connections; Rate=0; Reconnect=0 },
        [pscustomobject]@{ Name='slow-consumer'; WaitMode='adaptive'; Batch=64; Observability='off'; Affinity='off'; ClientScenario='slow-read'; Payload=128; Connections=$Connections; Rate=0; Reconnect=0 },
        [pscustomobject]@{ Name='churn'; WaitMode='adaptive'; Batch=64; Observability='off'; Affinity='off'; ClientScenario='echo'; Payload=32; Connections=$Connections; Rate=0; Reconnect=9 }
    )
}

$echo = Join-Path $root 'bin\x64\Release\EchoServer.exe'
$client = Join-Path $root 'bin\x64\Release\LoadClient.exe'
if (-not (Test-Path $echo) -or -not (Test-Path $client)) {
    throw 'Build Release before running benchmarks.'
}

$rows = @()
$etwStarted = $false
try {
    if ($CaptureEtw) {
        & wpr.exe -start CPU -filemode
        if ($LASTEXITCODE -ne 0) { throw 'WPR CPU trace could not be started.' }
        $etwStarted = $true
    }

    foreach ($case in $cases) {
        for ($run = 1; $run -le $Runs; ++$run) {
            $serverDuration = $WarmupSeconds + $DurationSeconds + 10
            $server = Start-Process -FilePath $echo -ArgumentList '--port', $Port,
                '--duration', $serverDuration, '--wait-mode', $case.WaitMode,
                '--batch', $case.Batch, '--observability', $case.Observability,
                '--affinity', $case.Affinity,
                '--max-sessions', ([Math]::Max(1024, $case.Connections + 16)) `
                -WindowStyle Hidden -PassThru
            try {
                Start-Sleep -Milliseconds 750
                if ($WarmupSeconds -gt 0) {
                    $warmupJson = Join-Path $OutputDirectory 'warmup.json'
                    & $client --port $Port --connections ([Math]::Min(4, $case.Connections)) `
                        --duration $WarmupSeconds --scenario echo --payload 32 `
                        --seed 1511506142 --json $warmupJson | Out-Null
                    if ($LASTEXITCODE -ne 0) { throw "warm-up for $($case.Name) failed" }
                }

                $before = Get-Process -Id $server.Id -ErrorAction Stop
                $beforeCpuMs = $before.TotalProcessorTime.TotalMilliseconds
                $contextSwitchJob = Start-Job -ScriptBlock {
                    param([int]$sample_count)
                    $counter_sets = Get-Counter `
                        '\Thread(EchoServer*)\Context Switches/sec' `
                        -SampleInterval 1 -MaxSamples $sample_count -ErrorAction Stop
                    $rates = foreach ($counter_set in $counter_sets) {
                        ($counter_set.CounterSamples | Measure-Object CookedValue -Sum).Sum
                    }
                    [pscustomobject]@{
                        Average = ($rates | Measure-Object -Average).Average
                        Maximum = ($rates | Measure-Object -Maximum).Maximum
                        Samples = $rates.Count
                    }
                } -ArgumentList ([Math]::Max(1, $DurationSeconds))
                $json = Join-Path $OutputDirectory "$($case.Name)-$($case.WaitMode)-batch$($case.Batch)-obs$($case.Observability)-$run.json"
                $arguments = @('--port', $Port, '--connections', $case.Connections,
                    '--duration', $DurationSeconds, '--scenario', $case.ClientScenario,
                    '--payload', $case.Payload, '--seed', 1511506142,
                    '--reconnect', $case.Reconnect, '--json', $json)
                if ($case.Rate -gt 0) { $arguments += @('--rate', $case.Rate) }
                & $client @arguments
                if ($LASTEXITCODE -ne 0) { throw "$($case.Name) run $run failed" }

                $contextSwitches = Receive-Job -Job $contextSwitchJob -Wait -AutoRemoveJob
                $contextSwitchJob = $null

                $result = Get-Content -Raw -Encoding UTF8 $json | ConvertFrom-Json
                $invalid = $result.connection_failures -ne 0 -or
                    $result.data_errors -ne 0 -or $result.dropped_packets -ne 0 -or
                    $result.timeouts -ne 0
                if ($invalid) { throw "$($case.Name) run $run reported correctness errors" }

                $after = Get-Process -Id $server.Id -ErrorAction Stop
                $serverCpuMs = [Math]::Max(0, $after.TotalProcessorTime.TotalMilliseconds - $beforeCpuMs)
                $serverCpuPercent = 100.0 * $serverCpuMs /
                    ($DurationSeconds * 1000.0 * [Math]::Max(1, [Environment]::ProcessorCount))
                $rows += [pscustomobject]@{
                    Case=$case.Name; WaitMode=$case.WaitMode; Batch=$case.Batch
                    Observability=$case.Observability; Affinity=$case.Affinity; Run=$run
                    ClientScenario=$case.ClientScenario; Payload=$case.Payload
                    Connections=$case.Connections; Rate=$case.Rate; Reconnect=$case.Reconnect
                    Packets=$result.received_packets; PacketsPerSecond=$result.packets_per_second
                    BytesPerSecond=$result.bytes_per_second; P50Us=$result.p50_us; P99Us=$result.p99_us
                    ClientCpuMs=$result.kernel_cpu_ms + $result.user_cpu_ms
                    ServerCpuMs=[Math]::Round($serverCpuMs, 3)
                    ServerCpuPercent=[Math]::Round($serverCpuPercent, 3)
                    ServerContextSwitchesPerSecond=[Math]::Round($contextSwitches.Average, 3)
                    ServerMaxContextSwitchesPerSecond=[Math]::Round($contextSwitches.Maximum, 3)
                    ContextSwitchSamples=$contextSwitches.Samples
                    ServerHandles=$after.HandleCount; ServerThreads=$after.Threads.Count
                    ServerPrivateBytes=$after.PrivateMemorySize64; ServerWorkingSetBytes=$after.WorkingSet64
                    ConnectionFailures=$result.connection_failures; DataErrors=$result.data_errors
                    DroppedPackets=$result.dropped_packets; Timeouts=$result.timeouts
                }
            } finally {
                if ($contextSwitchJob) {
                    Stop-Job -Job $contextSwitchJob -ErrorAction SilentlyContinue
                    Remove-Job -Job $contextSwitchJob -Force -ErrorAction SilentlyContinue
                }
                if (-not $server.HasExited) { Stop-Process -Id $server.Id }
            }
            if ($CooldownMilliseconds -gt 0) {
                Start-Sleep -Milliseconds $CooldownMilliseconds
            }
        }
    }
} finally {
    if ($etwStarted) {
        & wpr.exe -stop (Join-Path $OutputDirectory 'cpu-context-switches.etl')
    }
}

$rows | Export-Csv -NoTypeInformation -Encoding UTF8 (Join-Path $OutputDirectory 'summary.csv')
$aggregate = foreach ($group in ($rows | Group-Object Case,WaitMode,Batch,Observability,Affinity)) {
    $values = @($group.Group.PacketsPerSecond | Sort-Object)
    $p99Values = @($group.Group.P99Us | Sort-Object)
    $cpuValues = @($group.Group.ServerCpuPercent | Sort-Object)
    $contextSwitchValues = @($group.Group.ServerContextSwitchesPerSecond | Sort-Object)
    $middle = [Math]::Floor(($values.Count - 1) / 2)
    $mean = ($values | Measure-Object -Average).Average
    $variance = (($values | ForEach-Object { [Math]::Pow($_ - $mean, 2) }) |
        Measure-Object -Average).Average
    [pscustomobject]@{
        Case=$group.Group[0].Case; WaitMode=$group.Group[0].WaitMode
        Batch=$group.Group[0].Batch; Observability=$group.Group[0].Observability
        Affinity=$group.Group[0].Affinity
        Runs=$values.Count
        MedianPacketsPerSecond=$values[$middle]
        StdDevPacketsPerSecond=[Math]::Round([Math]::Sqrt($variance), 3)
        MedianP99Us=$p99Values[$middle]; MedianServerCpuPercent=$cpuValues[$middle]
        MedianServerContextSwitchesPerSecond=$contextSwitchValues[$middle]
        MaxHandles=($group.Group.ServerHandles | Measure-Object -Maximum).Maximum
        MaxPrivateBytes=($group.Group.ServerPrivateBytes | Measure-Object -Maximum).Maximum
    }
}
$aggregate | Export-Csv -NoTypeInformation -Encoding UTF8 (Join-Path $OutputDirectory 'aggregate.csv')

$regressions = @()
if ($BaselineCsv) {
    $baseline = Import-Csv -Encoding UTF8 $BaselineCsv
    foreach ($current in $aggregate) {
        $previous = $baseline | Where-Object {
            $_.Case -eq $current.Case -and $_.WaitMode -eq $current.WaitMode -and
            [int]$_.Batch -eq $current.Batch -and $_.Observability -eq $current.Observability
        } | Select-Object -First 1
        if (-not $previous) {
            throw "Baseline is missing $($current.Case)/$($current.WaitMode)/$($current.Batch)/$($current.Observability)."
        }
        $previousThroughput = [double]$previous.MedianPacketsPerSecond
        $previousP99 = [double]$previous.MedianP99Us
        $throughputRegression = if ($previousThroughput -gt 0) {
            100.0 * ($previousThroughput - [double]$current.MedianPacketsPerSecond) / $previousThroughput
        } else { 0.0 }
        $p99Regression = if ($previousP99 -gt 0) {
            100.0 * ([double]$current.MedianP99Us - $previousP99) / $previousP99
        } else { 0.0 }
        $passed = $throughputRegression -le $MaxThroughputRegressionPercent -and
            $p99Regression -le $MaxP99RegressionPercent
        $regressions += [pscustomobject]@{
            Case=$current.Case; WaitMode=$current.WaitMode; Batch=$current.Batch
            Observability=$current.Observability; Affinity=$current.Affinity
            ThroughputRegressionPercent=[Math]::Round($throughputRegression, 3)
            P99RegressionPercent=[Math]::Round($p99Regression, 3); Passed=$passed
        }
    }
    $regressions | Export-Csv -NoTypeInformation -Encoding UTF8 `
        (Join-Path $OutputDirectory 'regression.csv')
}

$environment = [pscustomobject]@{
    Commit=$commit; Timestamp=(Get-Date).ToUniversalTime().ToString('o')
    Computer=$env:COMPUTERNAME; Processor=$env:PROCESSOR_IDENTIFIER
    LogicalProcessors=[Environment]::ProcessorCount; Configuration='Release'
    Profile=$Profile; Runs=$Runs; DurationSeconds=$DurationSeconds
    WarmupSeconds=$WarmupSeconds; CooldownMilliseconds=$CooldownMilliseconds
    WaitModes=$WaitModes; BatchSizes=$BatchSizes; ObservabilityModes=$ObservabilityModes
    AffinityModes=$AffinityModes
    CaptureEtw=$CaptureEtw.IsPresent; BaselineCsv=$BaselineCsv
    MaxThroughputRegressionPercent=$MaxThroughputRegressionPercent
    MaxP99RegressionPercent=$MaxP99RegressionPercent
}
$environment | ConvertTo-Json | Set-Content -Encoding UTF8 (Join-Path $OutputDirectory 'environment.json')
@('# Benchmark summary', '', "- Commit: ``$commit``", "- Profile: $Profile",
    "- Runs per case: $Runs", "- Duration per run: $DurationSeconds seconds",
    '- Every correctness counter is zero.',
    '- `aggregate.csv` contains medians, throughput standard deviation, server CPU, context switches and resource peaks.',
    '- `-BaselineCsv` writes `regression.csv` and fails above the configured throughput/P99 limits.',
    '- `summary.csv` samples EchoServer thread context switches once per second.',
    '- Optional `cpu-context-switches.etl` contains detailed CPU scheduling evidence.') |
    Set-Content -Encoding UTF8 (Join-Path $OutputDirectory 'README.md')
Write-Host "Results: $OutputDirectory"
if ($regressions | Where-Object { -not $_.Passed }) {
    throw 'Benchmark regression threshold was exceeded.'
}
