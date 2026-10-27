param([switch]$Check)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$include = Join-Path $root 'include\rionetlite'
$output = Join-Path $root 'docs\interactive_book\generated\api.json'
$headers = @()
$rows = @()

foreach ($file in (Get-ChildItem -LiteralPath $include -File -Filter *.h |
        Sort-Object Name)) {
    $content = Get-Content -Raw -Encoding UTF8 -LiteralPath $file.FullName
    $headers += [ordered]@{ path=('include/rionetlite/' + $file.Name); sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $file.FullName).Hash.ToLowerInvariant() }
    $declared = @(
        [regex]::Matches($content,
            '(?m)^(?:enum class|struct|class)\s+([A-Z][A-Za-z0-9_]*)') |
            ForEach-Object { $_.Groups[1].Value }
        [regex]::Matches($content,
            '(?m)^(?:std::[A-Za-z0-9_:<>, ]+|[A-Za-z_][A-Za-z0-9_:<>]*)\s+([A-Z][A-Za-z0-9_]*)\s*\(') |
            ForEach-Object { $_.Groups[1].Value }
    ) | Where-Object { $_ -ne 'Impl' } | Sort-Object -Unique
    $annotations = @{}
    foreach ($match in [regex]::Matches($content,
            '(?m)^/// @api ([A-Za-z_][A-Za-z0-9_]*) - (.+)$')) {
        if ($annotations.ContainsKey($match.Groups[1].Value)) { throw "Duplicate API annotation: $($match.Groups[1].Value)" }
        $annotations[$match.Groups[1].Value] = $match.Groups[2].Value.Trim()
    }
    $missing = @($declared | Where-Object { -not $annotations.ContainsKey($_) })
    $extra = @($annotations.Keys | Where-Object { $_ -notin $declared })
    if ($missing.Count -ne 0 -or $extra.Count -ne 0) {
        throw "$($file.Name) API annotations differ. Missing=$($missing -join ',') Extra=$($extra -join ',')"
    }
    foreach ($symbol in $declared) {
        $rows += [pscustomobject]@{
            Header = $file.Name
            Symbol = $symbol
            Description = $annotations[$symbol]
        }
    }
}

$document = [ordered]@{
    schemaVersion=1
    headers=@($headers)
    symbols=@($rows | ForEach-Object { [ordered]@{
        header=$_.Header; symbol=$_.Symbol; description=$_.Description
    } })
}
$expected = (($document | ConvertTo-Json -Depth 6).Replace("`r`n", "`n")) + "`n"
if ($Check) {
    if (-not (Test-Path -LiteralPath $output) -or
        [IO.File]::ReadAllText($output, [Text.Encoding]::UTF8) -cne $expected) {
        throw 'Generated API JSON is stale. Run tools\generate-api-docs.ps1.'
    }
} else {
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $output) | Out-Null
    [IO.File]::WriteAllText($output, $expected, [Text.UTF8Encoding]::new($false))
}
Write-Host "Public API JSON verified: $($rows.Count) symbols"
