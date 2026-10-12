param([string]$Compiler = 'C:\Program Files\Windhawk\Compiler\bin\clang++.exe')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build\tests'
if (-not (Test-Path -LiteralPath $Compiler -PathType Leaf)) { throw "Compilador não encontrado: $Compiler" }
New-Item -ItemType Directory -Force -Path $buildDirectory | Out-Null
$sourceFile = Join-Path $projectRoot 'tests\warning_tracker_test.cpp'
$testBinary = Join-Path $buildDirectory 'warning_tracker_test.exe'
$previousTmp = $env:TMP
$previousTemp = $env:TEMP
$runMutex = [System.Threading.Mutex]::new($false, 'Local\PowerTimer.CalendarTests')
$lockAcquired = $false
try {
    try { $lockAcquired = $runMutex.WaitOne(0) }
    catch [System.Threading.AbandonedMutexException] { $lockAcquired = $true }
    if (-not $lockAcquired) { throw 'Outra execução de testes de calendário está em andamento.' }
    $env:TMP = $buildDirectory
    $env:TEMP = $buildDirectory
    & $Compiler '-std=c++23' '-Wall' '-Wextra' '-Werror' '-pedantic' '-static' $sourceFile '-o' $testBinary
    if ($LASTEXITCODE -ne 0) { throw "Compilação dos testes de calendário falhou: $LASTEXITCODE" }
    & $testBinary
    if ($LASTEXITCODE -ne 0) { throw "Testes de calendário falharam: $LASTEXITCODE" }
} finally {
    $env:TMP = $previousTmp
    $env:TEMP = $previousTemp
    if ($lockAcquired) { $runMutex.ReleaseMutex() }
    $runMutex.Dispose()
}
