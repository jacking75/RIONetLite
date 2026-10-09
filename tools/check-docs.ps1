$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$required = @(
    'README.md', 'docs/getting-started.md', 'docs/architecture.md',
    'docs/threading-model.md', 'docs/session-lifecycle.md',
    'docs/packet-framing.md', 'docs/backpressure.md',
    'docs/graceful-shutdown.md', 'docs/configuration.md',
    'docs/performance-tuning.md', 'docs/observability.md',
    'docs/troubleshooting.md', 'docs/memory-safety.md',
    'docs/samples/echo-server.md',
    'docs/samples/chat-server.md', 'docs/benchmarking.md',
    'docs/api/README.md', 'docs/api/generated.md', 'docs/api/contracts.md',
    'docs/networking-basics.md', 'docs/tutorial/first-echo-server.md',
    'docs/tutorial/game-worker.md', 'docs/protocol/chat.md',
    'docs/package-consumption.md', 'docs/getting-started-en.md',
    'CONTRIBUTING.md', 'THIRD_PARTY_NOTICES.md'
)
foreach ($relative in $required) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $relative))) {
        throw "Missing documentation: $relative"
    }
}
$markdown = @(Get-ChildItem -LiteralPath $root -File -Filter *.md) +
    @(Get-ChildItem -LiteralPath (Join-Path $root 'docs') -File -Filter *.md -Recurse)
foreach ($file in $markdown) {
    $content = Get-Content -Raw -Encoding UTF8 $file.FullName
    foreach ($match in [regex]::Matches($content, '\[[^\]]+\]\((?!https?://|mailto:|#)([^)#]+)')) {
        $target = [Uri]::UnescapeDataString($match.Groups[1].Value)
        if (-not (Test-Path -LiteralPath (Join-Path $file.DirectoryName $target))) {
            throw "Broken link in $($file.FullName): $target"
        }
    }
}
$readme = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'README.md')
$quickStart = [regex]::Match(
    $readme, '(?s)```cpp\r?\n(?<code>.*?)\r?\n```')
if (-not $quickStart.Success) { throw 'README C++ quick start was not found.' }
$consumer = Get-Content -Raw -Encoding UTF8 `
    (Join-Path $root 'samples\package_consumer\main.cpp')
$normalize = {
    param([string]$text)
    return ($text -replace "`r`n", "`n").Trim()
}
if ((& $normalize $quickStart.Groups['code'].Value) -ne
    (& $normalize $consumer)) {
    throw 'README C++ quick start differs from package_consumer/main.cpp.'
}
& (Join-Path $PSScriptRoot 'generate-api-docs.ps1') -Check
Write-Host "Documentation verified: $($required.Count) required files"
