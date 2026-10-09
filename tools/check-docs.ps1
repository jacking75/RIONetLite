$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$rootDocuments = @('README.md', 'README_kr.md', 'CHANGELOG.md',
    'CONTRIBUTING.md', 'SECURITY.md', 'THIRD_PARTY_NOTICES.md')
$required = $rootDocuments + @(
    'docs/getting-started.md', 'docs/getting-started-en.md',
    'docs/architecture.md', 'docs/operations.md', 'docs/verification.md',
    'docs/api/README.md', 'docs/samples/chat-server.md',
    'docs/package-consumption.md', 'docs/protocol/chat-fixtures.tsv',
    'docs/rionetlite_book.html'
)
foreach ($relative in $required) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf)) {
        throw "Missing documentation: $relative"
    }
}
$markdown = @($rootDocuments | ForEach-Object { Get-Item -LiteralPath (Join-Path $root $_) }) +
    @(Get-ChildItem -LiteralPath (Join-Path $root 'docs') -File -Filter *.md -Recurse)
$contents = @{}
foreach ($file in $markdown) {
    $contents[$file.FullName] = Get-Content -Raw -Encoding UTF8 -LiteralPath $file.FullName
}
function Get-MarkdownAnchors([string]$content) {
    $insideFence = $false
    $seen = @{}
    foreach ($line in ($content -split "`n")) {
        if ($line -match '^\s*(```|~~~)') { $insideFence = -not $insideFence; continue }
        if ($insideFence -or $line -notmatch '^#{1,6}\s+(.+?)\s*$') { continue }
        $heading = $Matches[1] -replace '\s+#+$', ''
        $slug = ($heading.Trim().ToLowerInvariant() -replace '[^\p{L}\p{Nd}_\- ]', '') -replace ' ', '-'
        if ($seen.ContainsKey($slug)) {
            $seen[$slug]++
            "${slug}-$($seen[$slug])"
        } else {
            $seen[$slug] = 0
            $slug
        }
    }
}
foreach ($file in $markdown) {
    foreach ($match in [regex]::Matches($contents[$file.FullName],
            '\[[^\]]+\]\((?!https?://|mailto:)([^)]+)\)')) {
        $parts = $match.Groups[1].Value -split '#', 2
        $relative = [Uri]::UnescapeDataString($parts[0])
        $target = if ($relative) {
            [IO.Path]::GetFullPath((Join-Path $file.DirectoryName $relative))
        } else { $file.FullName }
        if (-not (Test-Path -LiteralPath $target)) {
            throw "Broken link in $($file.FullName): $relative"
        }
        if ($parts.Count -eq 2 -and $parts[1] -and [IO.Path]::GetExtension($target) -eq '.md') {
            if (-not $contents.ContainsKey($target)) {
                $contents[$target] = Get-Content -Raw -Encoding UTF8 -LiteralPath $target
            }
            $fragment = [Uri]::UnescapeDataString($parts[1])
            if ($fragment -notin @(Get-MarkdownAnchors $contents[$target])) {
                throw "Broken anchor in $($file.FullName): $($match.Groups[1].Value)"
            }
        }
    }
}
$readme = $contents[(Join-Path $root 'README.md')]
$readmeKr = $contents[(Join-Path $root 'README_kr.md')]
$docFiles = @(Get-ChildItem -LiteralPath (Join-Path $root 'docs') -File -Recurse)
foreach ($file in $docFiles) {
    $relative = $file.FullName.Substring($root.Length + 1).Replace('\', '/')
    $linkedEn = $readme.Contains("($relative)")
    $linkedKr = $readmeKr.Contains("($relative)")
    if (-not ($linkedEn -or $linkedKr)) { throw "Documentation is not indexed: $relative" }
    if ($file.Extension -eq '.md') {
        $heading = [regex]::Match($contents[$file.FullName], '(?m)^# (.+)$').Groups[1].Value
        if ($heading -match '[\uAC00-\uD7A3]') {
            if (-not $linkedKr) { throw "Korean document is missing from README_kr.md: $relative" }
        } elseif (-not $linkedEn) { throw "English document is missing from README.md: $relative" }
    } elseif ($file.Extension -eq '.html' -and -not $linkedKr) {
        throw "Korean HTML book is missing from README_kr.md: $relative"
    }
}
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
Write-Host "Documentation verified: $($docFiles.Count) indexed files, links and anchors"
