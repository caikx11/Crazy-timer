param([string]$WindhawkPath = 'C:\Program Files\Windhawk')
# Builds the mod and copies the single-file source to dist/ for a GitHub release.
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$header = [IO.File]::ReadAllText((Join-Path $projectRoot 'src/windows/mod_entry.cpp'))
if ($header -notmatch '(?m)^// @version\s+(\S+)') { throw 'Versão não encontrada no cabeçalho do mod.' }
$version = $Matches[1]
& powershell -NoProfile -File (Join-Path $PSScriptRoot 'build-mod.ps1') -WindhawkPath $WindhawkPath
if ($LASTEXITCODE -ne 0) { throw "build-mod.ps1 falhou: $LASTEXITCODE" }
$source = Join-Path $projectRoot 'build/mod/crazy-time.wh.cpp'
if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Arquivo não encontrado: $source" }
$dist = Join-Path $projectRoot 'dist'
New-Item -ItemType Directory -Force -Path $dist | Out-Null
$target = Join-Path $dist 'crazy-time.wh.cpp'
Copy-Item -LiteralPath $source -Destination $target -Force
$hash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText((Join-Path $dist 'crazy-time.wh.cpp.sha256'), "$hash  crazy-time.wh.cpp`n", [Text.UTF8Encoding]::new($false))
Write-Output "Versão: $version"
Write-Output "Arquivo do release: $target"
Write-Output "SHA256: $hash"
Write-Output "Tag sugerida: v$version (veja docs/releases/README.md)"
