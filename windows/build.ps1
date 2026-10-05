# Builds Klats for Windows from scratch and runs the core tests.
#
#   .\build.ps1            release build in build\release
#   .\build.ps1 -Debug     debug build in build\debug
#   .\build.ps1 -Fixtures  also regenerate tests\fixtures\layouts.h
#
# Needs Visual Studio Build Tools 2022 (MSVC, Windows SDK, CMake and Ninja are part of it).
# Every build is clean: with only the Russian language pack of the Build Tools installed, Ninja
# loses track of header and resource dependencies, and a full build takes seconds anyway.
# Keep this file ASCII: Windows PowerShell 5.1 reads scripts without a BOM in the ANSI code page.
param(
    [switch]$Debug,
    [switch]$Fixtures
)
$ErrorActionPreference = 'Stop'
$preset = if ($Debug) { 'debug' } else { 'release' }
Set-Location -LiteralPath $PSScriptRoot

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'vswhere.exe not found: install Visual Studio Build Tools 2022' }
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'MSVC x64 tools not found: add "Desktop development with C++" to the Build Tools' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'

$buildDir = Join-Path $PSScriptRoot "build\$preset"
if (Test-Path -LiteralPath $buildDir) { Remove-Item -LiteralPath $buildDir -Recurse -Force }

$steps = @(
    "cmake --preset $preset",
    "cmake --build --preset $preset"
)
if ($Fixtures) { $steps += "cmake --build --preset $preset --target fixtures" }
$steps += "ctest --preset $preset"

# vcvars looks for vswhere on PATH and complains when it is not there.
$installer = Split-Path -Parent $vswhere
$command = "set `"PATH=$installer;%PATH%`" && call `"$vcvars`" >nul && " + ($steps -join ' && ')
cmd /c $command
if ($LASTEXITCODE -ne 0) { throw "build failed with exit code $LASTEXITCODE" }
Write-Host "==> done: $buildDir"
