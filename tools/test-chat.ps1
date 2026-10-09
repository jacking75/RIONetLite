param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [int]$Port = 32620,
    [string]$OutputDirectory = ''
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if ($Port -lt 1 -or $Port -gt 65532) { throw 'Port is outside its valid range.' }
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $root ('artifacts\chat\' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$taskPath = $env:Path
Remove-Item Env:Path -ErrorAction SilentlyContinue
Remove-Item Env:PATH -ErrorAction SilentlyContinue
$env:Path = $taskPath
$project = Join-Path $root 'samples\chat_client\csharp_test_client.csproj'
& dotnet build $project -c $Configuration --nologo
if ($LASTEXITCODE -ne 0) { throw 'ChatClient build failed.' }
$client = Join-Path $root "samples\chat_client\bin\$Configuration\net10.0-windows\ChatClient.dll"
$fixtures = Join-Path $root 'docs\protocol\chat-fixtures.tsv'
& dotnet $client --fixtures $fixtures
if ($LASTEXITCODE -ne 0) { throw 'C# golden fixture check failed.' }
$serverPath = Join-Path $root "bin\x64\$Configuration\ChatServer.exe"
if (-not (Test-Path -LiteralPath $serverPath)) { throw "Build $Configuration first." }
$results = @()
foreach ($shards in @(1, 2, 4)) {
    $testPort = $Port + $shards - 1
    $prefix = Join-Path $OutputDirectory "shards-$shards"
    $stopFile = "$prefix.stop"
    if (Test-Path -LiteralPath $stopFile) { Remove-Item -LiteralPath $stopFile -Force }
    $server = Start-Process -FilePath $serverPath -ArgumentList '--port', $testPort,
        '--logic-workers', $shards, '--io-workers', 8, '--max-sessions', 8,
        '--idle-seconds', 30, '--duration', 40, '--stop-file', ('"' + $stopFile + '"') `
        -WindowStyle Hidden -RedirectStandardOutput "$prefix.server.stdout.log" `
        -RedirectStandardError "$prefix.server.stderr.log" -PassThru
    $null = $server.Handle
    $clientProcess = $null
    $forced = $false
    try {
        $readyDeadline = (Get-Date).AddSeconds(5)
        $ready = $false
        do {
            $server.Refresh()
            if ($server.HasExited) { throw "ChatServer exited before listening: $($server.ExitCode)" }
            $listeners = [Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveTcpListeners()
            $ready = @($listeners | Where-Object Port -eq $testPort).Count -ne 0
            if (-not $ready) { Start-Sleep -Milliseconds 50 }
        } while (-not $ready -and (Get-Date) -lt $readyDeadline)
        if (-not $ready) { throw 'ChatServer listen deadline expired.' }
        $json = "$prefix.client.json"
        $arguments = @(('"' + $client + '"'), '--self-test', '--port', $testPort,
            '--fixtures', ('"' + $fixtures + '"'), '--json', ('"' + $json + '"'))
        $clientProcess = Start-Process -FilePath (Get-Command dotnet).Source -ArgumentList $arguments `
            -WindowStyle Hidden -RedirectStandardOutput "$prefix.client.stdout.log" `
            -RedirectStandardError "$prefix.client.stderr.log" -PassThru
        $null = $clientProcess.Handle
        if (-not $clientProcess.WaitForExit(35000)) { throw 'ChatClient check deadline expired.' }
        $clientProcess.Refresh()
        if ($clientProcess.ExitCode -ne 0) {
            throw ("ChatClient failed: " + (Get-Content -Raw -Encoding UTF8 "$prefix.client.stderr.log"))
        }
        $result = Get-Content -Raw -Encoding UTF8 $json | ConvertFrom-Json
        if (-not $result.passed -or $result.broadcast_receipts -ne 21 -or $result.malformed_closes -ne 5) {
            throw 'ChatClient result is incomplete.'
        }
        $results += [pscustomobject]@{ LogicShards=$shards; Port=$testPort; Result=$result }
    } finally {
        Set-Content -LiteralPath $stopFile -Encoding ASCII -Value 'stop'
        if ($clientProcess -and -not $clientProcess.HasExited) { Stop-Process -Id $clientProcess.Id -Force }
        if (-not $server.WaitForExit(10000)) { $forced = $true; Stop-Process -Id $server.Id -Force }
        $server.Refresh()
    }
    if ($forced -or $server.ExitCode -ne 0) { throw "ChatServer shutdown failed: forced=$forced exit=$($server.ExitCode)" }
    if (-not (Select-String -Path "$prefix.server.stdout.log" -SimpleMatch -Quiet 'Logic drain: active_players=0 queued_events=0 pending_broadcasts=0')) {
        throw 'Chat logic did not drain fully.'
    }
    Write-Host "Chat $shards shards: pass; 21 broadcast receipts, 5 malformed closes, full logic drain."
}
$results | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 (Join-Path $OutputDirectory 'summary.json')
Write-Host "Chat results: $OutputDirectory"
