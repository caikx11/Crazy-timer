param([string]$Compiler = 'C:\Program Files\Windhawk\Compiler\bin\clang++.exe')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$output = Join-Path $projectRoot 'build/preview'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$source = Join-Path $projectRoot 'src/windows/preview.cpp'
$binary = Join-Path $output 'power-timer-preview.exe'
$previousTmp = $env:TMP
$previousTemp = $env:TEMP
try {
    $env:TMP = $output
    $env:TEMP = $output
    & $Compiler '-std=c++23' '-O2' '-Wall' '-Wextra' '-Werror' '-static' '-municode' '-mwindows' `
        '-DWINVER=0x0A00' '-D_WIN32_WINNT=0x0A00' $source '-o' $binary `
        '-lshell32' '-lcomctl32' '-lgdi32' '-ladvapi32'
    if ($LASTEXITCODE -ne 0) { throw "Compilação da prévia falhou: $LASTEXITCODE" }
} finally {
    $env:TMP = $previousTmp
    $env:TEMP = $previousTemp
}
Write-Output "Prévia simulada: $binary"
