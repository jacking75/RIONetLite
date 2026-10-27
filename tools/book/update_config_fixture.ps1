param([switch]$Check)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$directory = Join-Path $root '.artifacts\book\config_fixture'
New-Item -ItemType Directory -Force -Path $directory | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products * -version '[18.0,19.0)' -requires Microsoft.Component.MSBuild -property installationPath
if (-not $installation) { throw 'Visual Studio 2026 C++ tools are required.' }
$developer = Join-Path $installation 'Common7\Tools\VsDevCmd.bat'
$configurationSource = Join-Path $root 'src\config.cpp'
$fixtureSource = Join-Path $PSScriptRoot 'config_fixture.cpp'
$configurationObject = Join-Path $directory 'configuration.obj'
$fixtureObject = Join-Path $directory 'fixture.obj'
$executable = Join-Path $directory 'config_fixture.exe'
$include = Join-Path $root 'include'
$batch = Join-Path $directory 'compile.cmd'
$lines = @(
    '@echo off',
    ('call "{0}" -no_logo -arch=x64 -host_arch=x64' -f $developer),
    'if errorlevel 1 exit /b 1',
    ('cl /nologo /EHsc /std:c++latest /utf-8 /W4 /WX /MD /DNOMINMAX /I"{0}" /c "{1}" /Fo"{2}"' -f $include, $configurationSource, $configurationObject),
    'if errorlevel 1 exit /b 1',
    ('cl /nologo /EHsc /std:c++latest /utf-8 /W4 /WX /MD /DNOMINMAX /I"{0}" /c "{1}" /Fo"{2}"' -f $include, $fixtureSource, $fixtureObject),
    'if errorlevel 1 exit /b 1',
    ('link /nologo /OUT:"{0}" "{1}" "{2}" kernel32.lib' -f $executable, $configurationObject, $fixtureObject),
    'exit /b %errorlevel%'
)
[IO.File]::WriteAllLines($batch, $lines, [Text.UTF8Encoding]::new($false))
& cmd.exe /d /c $batch
if ($LASTEXITCODE -ne 0) { throw 'Configuration fixture compilation failed.' }
$json = & $executable
if ($LASTEXITCODE -ne 0) { throw 'Configuration fixture execution failed.' }
$data = $json | ConvertFrom-Json
$data | Add-Member -NotePropertyName sourceSha256 -NotePropertyValue (Get-FileHash -LiteralPath $configurationSource -Algorithm SHA256).Hash.ToLowerInvariant()
$data | Add-Member -NotePropertyName headerSha256 -NotePropertyValue (Get-FileHash -LiteralPath (Join-Path $include 'rionetlite\config.h') -Algorithm SHA256).Hash.ToLowerInvariant()
$data | Add-Member -NotePropertyName provenance -NotePropertyValue 'Executed ValidateServerConfig from src/config.cpp with MSVC v145 x64; no allocation or RIO runtime acceptance implied.'
$expected = ($data | ConvertTo-Json -Depth 8).Replace("`r`n", "`n") + "`n"
$target = Join-Path $PSScriptRoot 'fixtures\configuration.json'
if ($Check) {
    if (-not (Test-Path -LiteralPath $target) -or [IO.File]::ReadAllText($target).Replace("`r`n", "`n") -ne $expected) {
        throw 'Configuration fixture is stale. Run tools/book/update_config_fixture.ps1.'
    }
} else {
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
    [IO.File]::WriteAllText($target, $expected, [Text.UTF8Encoding]::new($false))
}
Write-Host "Configuration fixture: $($data.cases.Count) executed C++ cases"
