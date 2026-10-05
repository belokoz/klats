# Builds Klats for Windows from scratch and runs the core tests.
#
#   .\build.ps1            release build in build\release
#   .\build.ps1 -Debug     debug build in build\debug
#   .\build.ps1 -Fixtures  also regenerate tests\fixtures\layouts.h
#   .\build.ps1 -Installer also build build\release\Klats-X.Y.Z-windows-x64.exe
#
# Needs Visual Studio Build Tools 2022 (MSVC, Windows SDK, CMake and Ninja are part of it), and
# Inno Setup 6 for the installer. Every build is clean: with only the Russian language pack of the
# Build Tools installed, Ninja loses track of header and resource dependencies, and a full build
# takes seconds anyway.
# Keep this file ASCII: Windows PowerShell 5.1 reads scripts without a BOM in the ANSI code page.
param(
    [switch]$Debug,
    [switch]$Fixtures,
    [switch]$Installer
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
$vsInstaller = Split-Path -Parent $vswhere
$command = "set `"PATH=$vsInstaller;%PATH%`" && call `"$vcvars`" >nul && " + ($steps -join ' && ')
cmd /c $command
if ($LASTEXITCODE -ne 0) { throw "build failed with exit code $LASTEXITCODE" }

if ($Installer) {
    if ($Debug) { throw 'the installer is built from the release build only' }
    $cmake = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'CMakeLists.txt') -Raw
    if ($cmake -notmatch 'project\(Klats VERSION (\d+\.\d+\.\d+)') { throw 'no version in CMakeLists.txt' }
    $version = $Matches[1]
    $iscc = @(
        (Get-Command iscc -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source),
        (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'),
        (Join-Path $env:ProgramFiles 'Inno Setup 6\ISCC.exe'),
        (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe')
    ) | Where-Object { $_ -and (Test-Path -LiteralPath $_) } | Select-Object -First 1
    if (-not $iscc) { throw 'Inno Setup 6 not found: winget install JRSoftware.InnoSetup' }
    & $iscc /Q "/DAppVersion=$version" "/DBuildDir=$buildDir" (Join-Path $PSScriptRoot 'installer\klats.iss')
    if ($LASTEXITCODE -ne 0) { throw "installer build failed with exit code $LASTEXITCODE" }
    Write-Host "==> installer: $(Join-Path $buildDir "Klats-$version-windows-x64.exe")"
}
Write-Host "==> done: $buildDir"
