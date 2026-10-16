param([string]$Compiler = 'C:\Program Files\Windhawk\Compiler\bin\clang++.exe')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$output = Join-Path $projectRoot 'build\tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$binary = Join-Path $output 'windows_startup_test.exe'
$previousTmp = $env:TMP
$previousTemp = $env:TEMP
try {
    $env:TMP = $output
    $env:TEMP = $output
    & $Compiler '-std=c++23' '-Wall' '-Wextra' '-Werror' '-static' `
        '-DWINVER=0x0A00' '-D_WIN32_WINNT=0x0A00' `
        (Join-Path $projectRoot 'tests\windows_startup_test.cpp') '-o' $binary `
        '-lshell32' '-lcomctl32' '-lgdi32' '-ladvapi32'
    if ($LASTEXITCODE -ne 0) { throw "Compilação do teste de início oculto falhou: $LASTEXITCODE" }
    & $binary
    if ($LASTEXITCODE -ne 0) { throw "Teste de início oculto falhou: $LASTEXITCODE" }
} finally {
    $env:TMP = $previousTmp
    $env:TEMP = $previousTemp
}
