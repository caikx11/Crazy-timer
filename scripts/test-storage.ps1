param([string]$Compiler = 'C:\Program Files\Windhawk\Compiler\bin\clang++.exe')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build\tests'
$fixture = Join-Path $buildDirectory 'storage-fixture'
if (-not (Test-Path -LiteralPath $Compiler -PathType Leaf)) { throw "Compilador não encontrado: $Compiler" }
New-Item -ItemType Directory -Force -Path $buildDirectory | Out-Null
$sourceFile = Join-Path $projectRoot 'tests\schedule_storage_test.cpp'
$testBinary = Join-Path $buildDirectory 'schedule_storage_test.exe'
$previousTmp = $env:TMP
$previousTemp = $env:TEMP
$runMutex = [System.Threading.Mutex]::new($false, 'Local\PowerTimer.StorageTests')
$lockAcquired = $false
try {
    try { $lockAcquired = $runMutex.WaitOne(0) }
    catch [System.Threading.AbandonedMutexException] { $lockAcquired = $true }
    if (-not $lockAcquired) { throw 'Outra execução de testes de armazenamento está em andamento.' }
    $env:TMP = $buildDirectory
    $env:TEMP = $buildDirectory
    & $Compiler '-std=c++23' '-Wall' '-Wextra' '-Werror' '-pedantic' '-static' $sourceFile '-o' $testBinary
    if ($LASTEXITCODE -ne 0) { throw "Compilação dos testes de armazenamento falhou: $LASTEXITCODE" }
    & $testBinary $fixture
    if ($LASTEXITCODE -ne 0) { throw "Testes de armazenamento falharam: $LASTEXITCODE" }
} finally {
    $env:TMP = $previousTmp
    $env:TEMP = $previousTemp
    if ($lockAcquired) { $runMutex.ReleaseMutex() }
    $runMutex.Dispose()
}
