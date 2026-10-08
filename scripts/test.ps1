param(
    [string]$Compiler = 'C:\Program Files\Windhawk\Compiler\bin\clang++.exe'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path (Join-Path $projectRoot 'build') 'tests'
if (-not (Test-Path -LiteralPath $Compiler -PathType Leaf)) {
    throw "Compilador não encontrado: $Compiler"
}
New-Item -ItemType Directory -Force -Path $buildDirectory | Out-Null
# Serialize this project's runs without relying on a persistent lock file.
$hasher = [System.Security.Cryptography.SHA256]::Create()
try {
    $projectHash = [BitConverter]::ToString($hasher.ComputeHash(
        [Text.Encoding]::UTF8.GetBytes($projectRoot.ToLowerInvariant()))).Replace('-', '')
} finally {
    $hasher.Dispose()
}
$runMutex = [System.Threading.Mutex]::new($false, "Local\PowerTimer.Tests.$projectHash")
$lockAcquired = $false
$sourceFile = Join-Path $projectRoot 'tests\scheduler_test.cpp'
$testBinary = Join-Path $buildDirectory 'scheduler_test.exe'
# This target contains only the portable scheduler, never Windows power actions.
$previousTmp = $env:TMP
$previousTemp = $env:TEMP
try {
    try {
        $lockAcquired = $runMutex.WaitOne(0)
    } catch [System.Threading.AbandonedMutexException] {
        # The previous runner exited unexpectedly; ownership is granted to us.
        $lockAcquired = $true
    }
    if (-not $lockAcquired) { throw 'Outra execução de testes está em andamento neste projeto.' }
    # Keep compiler temporary files inside the writable project directory.
    $env:TMP = $buildDirectory
    $env:TEMP = $buildDirectory
    & $Compiler '-std=c++23' '-Wall' '-Wextra' '-Werror' '-pedantic' '-static' $sourceFile '-o' $testBinary
    if ($LASTEXITCODE -ne 0) { throw "Compilação dos testes falhou: $LASTEXITCODE" }
    Write-Host "Compilação concluída. Executando testes: $testBinary"
    # A launch failure is not a failed assertion (and LASTEXITCODE may be stale).
    try {
        & $testBinary
    } catch {
        throw [System.InvalidOperationException]::new(
            "Não foi possível iniciar os testes; nenhum resultado foi validado. " +
            "Binário: $testBinary. Consulte o erro original e, se houver bloqueio, " +
            "o histórico de proteção do Windows/antivírus. O script não altera proteções. " +
            "Erro original: $($_.Exception.Message)", $_.Exception)
    }
    if ($LASTEXITCODE -ne 0) { throw "Testes falharam: $LASTEXITCODE" }
} finally {
    $env:TMP = $previousTmp
    $env:TEMP = $previousTemp
    if ($lockAcquired) { $runMutex.ReleaseMutex() }
    $runMutex.Dispose()
}

& (Join-Path $PSScriptRoot 'test-book.ps1') -Compiler $Compiler
& (Join-Path $PSScriptRoot 'test-storage.ps1') -Compiler $Compiler
& (Join-Path $PSScriptRoot 'test-calendar.ps1') -Compiler $Compiler
