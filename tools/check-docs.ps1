$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (-not (Get-Command python -ErrorAction SilentlyContinue)) {
    throw 'Install Python 3.10+ and rerun tools\check-docs.ps1.'
}
& python -c 'import sys; sys.exit(0 if sys.version_info >= (3,10) else 1)'
if ($LASTEXITCODE -ne 0) { throw 'Python 3.10+ is required. Rerun tools\check-docs.ps1 after installation.' }
if (-not (Get-Command node -ErrorAction SilentlyContinue)) {
    throw 'Install Node.js 20+ and run npm.cmd --prefix tools/book ci; rerun tools\check-docs.ps1.'
}
$nodeVersion = & node --version
if ($LASTEXITCODE -ne 0 -or [int]($nodeVersion.TrimStart('v').Split('.')[0]) -lt 20) {
    throw 'Node.js 20+ is required. Rerun tools\check-docs.ps1 after installation.'
}
& (Join-Path $PSScriptRoot 'generate-api-docs.ps1') -Check
& python (Join-Path $PSScriptRoot 'book\verify_documents.py')
if ($LASTEXITCODE -ne 0) { throw 'Documentation verification failed.' }
Write-Host 'Documentation verification passed; native, model and browser execution are separate checks.'
