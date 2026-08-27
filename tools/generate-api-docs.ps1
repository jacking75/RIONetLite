param([switch]$Check)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$include = Join-Path $root 'include\rionetlite'
$output = Join-Path $root 'docs\api\generated.md'
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
    '# 생성된 공개 API 목록',
    '',
    '> `tools\generate-api-docs.ps1`이 공개 헤더의 `@api` 주석에서 생성한다.',
    '',
    '| 헤더 | symbol | 설명 |',
    '|---|---|---|'
)
$lines += $rows | ForEach-Object {
    '| `{0}` | `{1}` | {2} |' -f $_.Header, $_.Symbol, $_.Description
}
$expected = ($lines -join "`r`n") + "`r`n"
if ($Check) {
    if (-not (Test-Path -LiteralPath $output)) {
        throw 'Generated API reference is missing.'
    }
    $actual = Get-Content -Raw -Encoding UTF8 -LiteralPath $output
    if ($actual -ne $expected) {
        throw 'Generated API reference is stale. Run tools\generate-api-docs.ps1.'
    }
} else {
    Set-Content -Encoding UTF8 -LiteralPath $output -Value $expected -NoNewline
}
Write-Host "Public API documented: $($rows.Count) symbols"
