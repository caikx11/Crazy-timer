param([string]$WindhawkPath = 'C:\Program Files\Windhawk')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$output = Join-Path $projectRoot 'build/mod'
$compiler = Join-Path $WindhawkPath 'Compiler/bin/clang++.exe'
$version = (Get-Item -LiteralPath (Join-Path $WindhawkPath 'windhawk.exe')).VersionInfo.ProductVersion
$engine = Join-Path $WindhawkPath "Engine/$version/32/windhawk.lib"
foreach ($file in @($compiler, $engine)) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Arquivo não encontrado: $file" }
}
New-Item -ItemType Directory -Force -Path $output | Out-Null
$source = Join-Path $output 'crazy-time.wh.cpp'
$binary = Join-Path $output 'crazy-time.dll'
# The editor compiles stdin: embed our files in dependency order, then the
# official stable launcher verbatim. System includes remain intact.
$parts = @('src/scheduler.hpp', 'src/calendar.hpp', 'src/schedule_types.hpp', 'src/schedule_validation.hpp', 'src/schedule_codec.hpp', 'src/schedule_book.hpp', 'src/strings.hpp', 'src/strings_en.hpp', 'src/warning_tracker.hpp', 'src/power_executor.hpp', 'src/windows/windows_power_backend.hpp', 'src/windows/schedule_file.hpp', 'src/windows/local_calendar.hpp', 'src/windows/tray_host.hpp', 'src/windows/mod_entry.cpp')
$builder = [Text.StringBuilder]::new()
# The Windhawk metadata, readme and settings blocks must come first, so they are
# taken from the top of mod_entry.cpp; the license follows as a comment.
$entry = [IO.File]::ReadAllText((Join-Path $projectRoot 'src/windows/mod_entry.cpp'))
$settingsEnd = '// ==/WindhawkModSettings=='
$cut = $entry.IndexOf($settingsEnd)
if ($cut -lt 0) { throw 'Bloco de configurações do Windhawk não encontrado em mod_entry.cpp.' }
$cut += $settingsEnd.Length
[void]$builder.Append($entry.Substring(0, $cut))
$license = [IO.File]::ReadAllText((Join-Path $projectRoot 'LICENSE'))
[void]$builder.Append("`n/*`n" + $license + "`n*/`n")
foreach ($part in $parts) {
    $content = if ($part -eq 'src/windows/mod_entry.cpp') { $entry.Substring($cut) } else { [IO.File]::ReadAllText((Join-Path $projectRoot $part)) }
    $content = [regex]::Replace($content, '(?m)^#pragma once\r?\n', '')
    $content = [regex]::Replace($content, '(?m)^#include "[^"\r\n]+"\r?\n', '')
    [void]$builder.AppendLine("// Source: $part")
    [void]$builder.AppendLine($content)
}
[void]$builder.Append([IO.File]::ReadAllText((Join-Path $projectRoot 'src/windows/windhawk_tool_launcher.inc')))
[IO.File]::WriteAllText($source, $builder.ToString(), [Text.UTF8Encoding]::new($false))
# Quoted -D values lose their quotes through Windows PowerShell 5.1, so the mod identity
# comes from a generated prelude instead of editing the official launcher.
$prelude = Join-Path $output 'mod-identity.h'
[IO.File]::WriteAllText($prelude, '#define WH_MOD_ID L"local@crazy-time"' + "`n" + '#define WH_MOD_VERSION L"0.7.0"' + "`n", [Text.UTF8Encoding]::new($false))
$arguments = @(
    '-std=c++23', '-O2', '-shared', '-Wall', '-Wextra',
    '-DUNICODE', '-D_UNICODE', '-DWINVER=0x0A00', '-D_WIN32_WINNT=0x0A00',
    '-D_WIN32_IE=0x0A00', '-DNTDDI_VERSION=0x0A000008', '-D__USE_MINGW_ANSI_STDIO=0',
    '-DWH_MOD', '-include', $prelude,
    $engine, '-x', 'c++', $source, '-include', (Join-Path $WindhawkPath 'Compiler/include/windhawk_api.h'),
    '-target', 'i686-w64-mingw32', '-Wl,--export-all-symbols', '-o', $binary,
    '-lshell32', '-lcomctl32', '-lgdi32', '-ladvapi32'
)
$previousTmp = $env:TMP
$previousTemp = $env:TEMP
try {
    $env:TMP = $output
    $env:TEMP = $output
    & $compiler @arguments
    if ($LASTEXITCODE -ne 0) { throw "Compilação do mod falhou: $LASTEXITCODE" }
} finally {
    $env:TMP = $previousTmp
    $env:TEMP = $previousTemp
}
Write-Output "Fonte única para o editor Windhawk: $source"
Write-Output "DLL x86 compilada (ainda não instalada): $binary"
