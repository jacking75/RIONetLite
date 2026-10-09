param([switch]$Check)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$include = Join-Path $root 'include\rionetlite'
$output = Join-Path $root 'docs\api\README.md'
$rows = @()

foreach ($file in (Get-ChildItem -LiteralPath $include -File -Filter *.h |
        Sort-Object Name)) {
    $content = Get-Content -Raw -Encoding UTF8 -LiteralPath $file.FullName
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

$lines = @(
    '<!-- BEGIN GENERATED API -->',
    '## 공개 symbol 목록',
    '',
    '> `tools\generate-api-docs.ps1`이 공개 헤더의 `@api` 주석에서 생성한다.',
    '',
    '| 헤더 | symbol | 설명 |',
    '|---|---|---|'
)
$lines += $rows | ForEach-Object {
    '| `{0}` | `{1}` | {2} |' -f $_.Header, $_.Symbol, $_.Description
}
$lines += '<!-- END GENERATED API -->'
$expected = $lines -join "`n"
if (-not (Test-Path -LiteralPath $output)) {
    throw 'Public API reference is missing.'
}
$actual = (Get-Content -Raw -Encoding UTF8 -LiteralPath $output).Replace("`r`n", "`n")
$regions = [regex]::Matches($actual,
    '(?s)<!-- BEGIN GENERATED API -->.*?<!-- END GENERATED API -->')
if ($regions.Count -ne 1) {
    throw 'Public API reference must contain exactly one generated region.'
}
$region = $regions[0]
if ($Check) {
    if ($region.Value -ne $expected) {
        throw 'Generated API reference is stale. Run tools\generate-api-docs.ps1.'
    }
} else {
    $updated = $actual.Substring(0, $region.Index) + $expected +
        $actual.Substring($region.Index + $region.Length)
    [IO.File]::WriteAllText($output, $updated, [Text.UTF8Encoding]::new($false))
}
Write-Host "Public API documented: $($rows.Count) symbols"
