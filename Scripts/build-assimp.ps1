#Requires -Version 5.1
<#
.SYNOPSIS
    Download and build Assimp 5.4.3 static libs (Debug + Release, Windows x64).
    Installs to External/assimp/ so cmake auto-detects it — no -DASSIMP_ROOT needed.

.DESCRIPTION
    Run once from the project root or from within Scripts/.
    Requires CMake 3.20+ and Visual Studio 2022 to be installed.

.EXAMPLE
    .\Scripts\build-assimp.ps1
#>
[CmdletBinding()]
param(
    [switch]$Force   # Re-build even if External/assimp already exists
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# ── Configuration ─────────────────────────────────────────────────────────────
$AssimpVersion = "5.4.3"
$SourceUrl     = "https://github.com/assimp/assimp/archive/refs/tags/v$AssimpVersion.zip"

# ── Paths ────────────────────────────────────────────────────────────────────
$ScriptDir   = $PSScriptRoot
$ProjectRoot = Split-Path -Parent $ScriptDir
$InstallDir  = Join-Path $ProjectRoot "External\assimp"
$TempDir     = Join-Path ([System.IO.Path]::GetTempPath()) "xcel3d-assimp-$AssimpVersion"
$ZipPath     = Join-Path $TempDir "source.zip"
$SrcDir      = Join-Path $TempDir "assimp-$AssimpVersion"
$BuildDir    = Join-Path $TempDir "build"
$LibDir      = Join-Path $InstallDir "lib"

Write-Host ""
Write-Host "=== Assimp $AssimpVersion  |  Windows x64 Static (Debug + Release) ===" -ForegroundColor Cyan
Write-Host "  Project root : $ProjectRoot"
Write-Host "  Install to   : $InstallDir"
Write-Host "  Temp dir     : $TempDir"
Write-Host ""

# ── Guard: skip if already installed ─────────────────────────────────────────
$sentinel = Join-Path $InstallDir "include\assimp\Importer.hpp"
if ((Test-Path $sentinel) -and -not $Force) {
    Write-Host "Assimp already installed at:" -ForegroundColor Yellow
    Write-Host "  $InstallDir"
    Write-Host ""
    Write-Host "Pass -Force to rebuild, or delete External\assimp\ manually."
    exit 0
}

# ── Verify cmake is available ─────────────────────────────────────────────────
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "cmake not found on PATH.  Install CMake 3.20+ and retry."
}

# ── Prepare temp directory ────────────────────────────────────────────────────
if (Test-Path $TempDir) { Remove-Item $TempDir -Recurse -Force }
New-Item -ItemType Directory -Path $TempDir -Force | Out-Null

# ── Download ──────────────────────────────────────────────────────────────────
Write-Host "Downloading assimp $AssimpVersion..." -ForegroundColor Cyan
Invoke-WebRequest -Uri $SourceUrl -OutFile $ZipPath -UseBasicParsing

Write-Host "Extracting..."
Expand-Archive -Path $ZipPath -DestinationPath $TempDir -Force

if (-not (Test-Path $SrcDir)) {
    throw "Expected source directory not found: $SrcDir"
}

# ── CMake configure ───────────────────────────────────────────────────────────
# Use the Visual Studio 17 2022 multi-config generator — no need for vcvarsall.
# Both Debug and Release are built from a single configure step.
$CmakeOpts = @(
    "-S", $SrcDir,
    "-B", $BuildDir,
    "-G", "Visual Studio 17 2022",
    "-A", "x64",
    "-DCMAKE_INSTALL_PREFIX=$InstallDir",
    "-DBUILD_SHARED_LIBS=OFF",           # static lib — plugin DLL embeds assimp
    "-DASSIMP_BUILD_TESTS=OFF",
    "-DASSIMP_BUILD_ASSIMP_TOOLS=OFF",
    "-DASSIMP_WARNINGS_AS_ERRORS=OFF",
    "-DASSIMP_BUILD_ZLIB=ON",            # bundled zlib for zip-based formats
    "-DASSIMP_INSTALL=ON",
    "-DASSIMP_INSTALL_PDB=OFF"
)

Write-Host ""
Write-Host "Configuring..." -ForegroundColor Cyan
& cmake @CmakeOpts
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed (exit $LASTEXITCODE)" }

# ── Build + install Release ───────────────────────────────────────────────────
Write-Host ""
Write-Host "Building Release..." -ForegroundColor Cyan
& cmake --build $BuildDir --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw "Build Release failed (exit $LASTEXITCODE)" }

& cmake --install $BuildDir --config Release
if ($LASTEXITCODE -ne 0) { throw "Install Release failed (exit $LASTEXITCODE)" }

# ── Build + install Debug ─────────────────────────────────────────────────────
Write-Host ""
Write-Host "Building Debug..." -ForegroundColor Cyan
& cmake --build $BuildDir --config Debug --parallel
if ($LASTEXITCODE -ne 0) { throw "Build Debug failed (exit $LASTEXITCODE)" }

& cmake --install $BuildDir --config Debug
if ($LASTEXITCODE -ne 0) { throw "Install Debug failed (exit $LASTEXITCODE)" }

# ── Ensure bundled zlibstatic is available for the plugin link ────────────────
# Assimp does not always install its internal zlib; copy it explicitly so
# AssimpPlugin/CMakeLists.txt can resolve it via find_library.
$zlibCandidates = @(
    @{ Src = "$BuildDir\contrib\zlib\Release\zlibstatic.lib";  Dst = "zlibstatic.lib"  },
    @{ Src = "$BuildDir\contrib\zlib\Debug\zlibstaticd.lib";   Dst = "zlibstaticd.lib" },
    @{ Src = "$BuildDir\contrib\zlib\Debug\zlibstatic.lib";    Dst = "zlibstatic.lib"  }
)
foreach ($entry in $zlibCandidates) {
    $src = $entry.Src
    $dst = Join-Path $LibDir $entry.Dst
    if ((Test-Path $src) -and -not (Test-Path $dst)) {
        Write-Host "Copying bundled zlib: $($entry.Dst)"
        Copy-Item $src $dst -Force
    }
}

# ── Cleanup ───────────────────────────────────────────────────────────────────
Write-Host ""
Write-Host "Cleaning up temp directory..."
Remove-Item $TempDir -Recurse -Force

# ── Summary ───────────────────────────────────────────────────────────────────
Write-Host ""
Write-Host "=== Done ===" -ForegroundColor Green
Write-Host ""
Write-Host "Installed: $InstallDir"
Write-Host ""
if (Test-Path $LibDir) {
    Write-Host "Libraries:"
    Get-ChildItem $LibDir -Filter "*.lib" | Sort-Object Name | ForEach-Object {
        $mb = [math]::Round($_.Length / 1MB, 1)
        Write-Host "  lib\$($_.Name)  ($mb MB)"
    }
}
Write-Host ""
Write-Host "Configure the project - assimp will be found automatically:"
Write-Host "  cmake -S . -B out\build\x64-Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug"
Write-Host "  cmake --build out\build\x64-Debug"
