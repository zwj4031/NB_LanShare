# build_ci.ps1 - CI build script that mirrors the nb_build_gui.py toolchain exactly:
#
#   VsDevCmd.bat -> rc.exe(winres/main.rc) -> cl.exe(/utf-8 /O2 /MT /EHsc /std:c++17 /GR-)
#   -> link(/SUBSYSTEM:WINDOWS,{5.01|5.02} /MANIFEST:NO) -> dist/x86, dist/x64
#
# Usage:
#   pwsh -NoProfile -File build_ci.ps1 -Version 1.2.3
#   pwsh -NoProfile -File build_ci.ps1 -Version 1.2.3 -VsDevCmd "C:\...\VsDevCmd.bat"
param(
    [string]$Version = "1.0.0",
    [string]$VsDevCmd = ""
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

# ---------------------------------------------------------------------------
# Locate VsDevCmd.bat (same logic as nb_build_gui.py's find_vsdevcmd)
# ---------------------------------------------------------------------------
if (-not $VsDevCmd) {
    $progFiles = ${env:ProgramFiles(x86)}
    if (-not $progFiles) { $progFiles = ${env:ProgramFiles} }
    $vswhere = Join-Path $progFiles "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -property installationPath
        if ($vsPath) { $VsDevCmd = Join-Path $vsPath "Common7\Tools\VsDevCmd.bat" }
    }
    if (-not $VsDevCmd -or -not (Test-Path $VsDevCmd)) {
        $VsDevCmd = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
    }
}
if (-not (Test-Path $VsDevCmd)) {
    throw "VsDevCmd.bat not found: $VsDevCmd"
}
Write-Host "=== VsDevCmd: $VsDevCmd"
Write-Host "=== Version : $Version"

if ($Version -notmatch '^\d+\.\d+\.\d+$') {
    throw "Invalid version (expect x.y.z): $Version"
}

$archs = @(
    @{ Arch = "x86"; Sub = "5.01"; Exe = "NB_LANShare_x86.exe" },
    @{ Arch = "x64"; Sub = "5.02"; Exe = "NB_LANShare_x64.exe" }
)

$sources = @(
    "src\main.cpp",
    "src\util.cpp",
    "src\lang.cpp",
    "src\qrcode.cpp",
    "src\webtemplates.cpp",
    "src\http_server.cpp",
    "src\http_routes.cpp",
    "src\sys_integration.cpp",
    "src\gui.cpp"
)

# ---------------------------------------------------------------------------
# Forced-include version header (feeds NB_VERSION into every .cpp)
# ---------------------------------------------------------------------------
$verHeader = ("#ifndef NB_VERSION`r`n#define NB_VERSION L`"$Version`"`r`n#endif`r`n")
New-Item -ItemType Directory -Force -Path (Join-Path $root "build") | Out-Null
Set-Content -Path (Join-Path $root "build\nb_version.h") -Value $verHeader -Encoding ASCII

foreach ($a in $archs) {
    $arch = $a.Arch
    $sub  = $a.Sub
    $exe  = $a.Exe
    $bd   = Join-Path $root "build\$arch"
    $dd   = Join-Path $root "dist\$arch"
    New-Item -ItemType Directory -Force -Path $bd | Out-Null
    New-Item -ItemType Directory -Force -Path $dd | Out-Null

    # ------------------------------------------------------------------ #
    # 1. Generate winres/versioninfo.rc (per-arch, English strings only) #
    # ------------------------------------------------------------------ #
    $vv = $Version.Split(".")
    $verInfo = @"
#include <windows.h>
VS_VERSION_INFO VERSIONINFO
 FILEVERSION $($vv[0]),$($vv[1]),$($vv[2]),0
 PRODUCTVERSION $($vv[0]),$($vv[1]),$($vv[2]),0
 FILEFLAGSMASK 0x3fL
 FILEFLAGS 0x0L
 FILEOS 0x40004L
 FILETYPE 0x1L
 FILESUBTYPE 0x0L
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904b0"
        BEGIN
            VALUE "CompanyName", "NB_LANShare Project"
            VALUE "FileDescription", "NB_LANShare - lightweight LAN file & text sharing tool"
            VALUE "FileVersion", "$Version"
            VALUE "InternalName", "NB_LANShare"
            VALUE "LegalCopyright", "Copyright (C) 2026 NB_LANShare Project"
            VALUE "OriginalFilename", "$exe"
            VALUE "ProductName", "NB_LANShare"
            VALUE "ProductVersion", "$Version"
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x0409, 1200
    END
END
"@
    Set-Content -Path (Join-Path $bd "versioninfo.rc") -Value $verInfo -Encoding ASCII

    # ------------------------------------------------------------------ #
    # 2. Generate & run the per-arch batch (VsDevCmd + rc + cl)          #
    #    All paths below are relative to the repo root and ASCII-safe.   #
    # ------------------------------------------------------------------ #
    $srcObj  = ($sources | ForEach-Object { "build\$arch\" + [System.IO.Path]::GetFileName($_).Replace(".cpp", ".obj") }) -join " "
    $srcList = $sources -join " "
    $libs = "Comctl32.lib Shell32.lib Shlwapi.lib User32.lib Gdi32.lib Advapi32.lib Ole32.lib Comdlg32.lib"

    $bat = Join-Path $bd "do_build.bat"
    $batContent = @(
        "@echo off",
        "call `"$VsDevCmd`" -arch=$arch",
        "if errorlevel 1 exit /b 1",
        "if not exist `"build\$arch`" mkdir `"build\$arch`"",
        "del /q `"build\$arch\*.obj`" >nul 2>nul",
        "rc.exe /nologo /i`"winres`" /fo`"build\$arch\main.res`" winres\main.rc",
        "if errorlevel 1 exit /b 1",
        "rc.exe /nologo /fo`"build\$arch\versioninfo.res`" `"build\$arch\versioninfo.rc`"",
        "if errorlevel 1 exit /b 1",
        "cl.exe /nologo /utf-8 /O2 /MT /EHsc /std:c++17 /GR- /c /Ibuild /FInb_version.h /Fobuild\$arch\ $srcList",
        "if errorlevel 1 exit /b 1",
        "cl.exe /nologo /O2 /MT $srcObj build\$arch\main.res build\$arch\versioninfo.res /link /OUT:dist\$arch\$exe /SUBSYSTEM:WINDOWS,$sub /MANIFEST:NO $libs",
        "if errorlevel 1 exit /b 1",
        "echo === BUILD_OK_$arch ==="
    ) -join "`r`n"
    Set-Content -Path $bat -Value $batContent -Encoding ASCII

    Write-Host "=== Building $arch (XP Subsystem $sub) ..."
    Push-Location $root
    try {
        & cmd.exe /c $bat
        if ($LASTEXITCODE -ne 0) { throw "Build failed for $arch (exit code $LASTEXITCODE)" }
    } finally {
        Pop-Location
    }

    $out = Join-Path $dd $exe
    if (-not (Test-Path $out)) { throw "Output not found: $out" }
    Write-Host "=== [OK] $arch -> $out ($((Get-Item $out).Length) bytes)"
}

Write-Host ""
Write-Host "=== ALL BUILDS SUCCEEDED. Version: $Version ==="