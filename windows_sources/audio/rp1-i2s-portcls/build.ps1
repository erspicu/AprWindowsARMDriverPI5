<#
.SYNOPSIS
    Build the RP1 I2S PortCls/WaveRT audio driver (.sys) for ARM64 via direct
    cl.exe/link.exe (no MSBuild). Reuses the ported HAL from ..\rp1-i2s\.

.PARAMETER CompileOnly
    Compile all sources to .obj but skip linking (useful while miniports are
    still being filled in - Phase 2 is incremental).

.EXAMPLE
    pwsh -File build.ps1 -CompileOnly      # compile check
    pwsh -File build.ps1                    # full build + deliver
#>
[CmdletBinding()]
param(
    [ValidateSet("ARM64","x64")] [string]$Arch = "ARM64",
    [string]$SdkVer = "10.0.26100.0",
    [switch]$CompileOnly,
    [switch]$Sign
)
$ErrorActionPreference = "Stop"
$ProjDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$HalDir  = Join-Path (Split-Path -Parent $ProjDir) "rp1-i2s"
$Kit     = "C:\Program Files (x86)\Windows Kits\10"
$VsRoot  = "C:\Program Files\Microsoft Visual Studio\2022\Community"
$Deliver = "C:\ai_project\AprWindowsDriver\windows_driver\audio"

$ClArch  = $Arch.ToLower()
$Machine = $Arch.ToUpper()
$ArchDefs = if ($Arch -eq "ARM64") { @("/D_ARM64_","/DARM64") } else { @("/D_AMD64_","/DAMD64") }

$Vc   = Get-ChildItem "$VsRoot\VC\Tools\MSVC" -Directory | Sort-Object Name -Descending | Select-Object -First 1
$Cl   = Join-Path $Vc.FullName "bin\Hostx64\$ClArch\cl.exe"
$Link = Join-Path $Vc.FullName "bin\Hostx64\$ClArch\link.exe"

$IncKmCrt  = "$Kit\Include\$SdkVer\km\crt"
$IncKm     = "$Kit\Include\$SdkVer\km"
$IncShared = "$Kit\Include\$SdkVer\shared"
$LibKm     = "$Kit\Lib\$SdkVer\km\$ClArch"

$OutDir = Join-Path $ProjDir "build\$Arch"
New-Item -ItemType Directory -Force $OutDir | Out-Null

# C++ kernel sources in this project + the C HAL from the Phase 1 project.
$CppSources = Get-ChildItem $ProjDir -Filter *.cpp | Select-Object -ExpandProperty FullName
$CSources   = @(Join-Path $HalDir "rp1_i2s_hw.c")

$CommonInc = @("/I$IncKmCrt","/I$IncKm","/I$IncShared")
$Objs = @()

Write-Host "== Compiling C++ ($Arch) ==" -ForegroundColor Cyan
foreach ($s in $CppSources) {
    $obj = Join-Path $OutDir ([IO.Path]::GetFileNameWithoutExtension($s) + ".obj")
    $Objs += $obj
    $args = @("/nologo","/c","/W4","/Zi","/kernel","/GS","/Zc:wchar_t",
              "/DNDEBUG") + $ArchDefs + $CommonInc + @("/Fo$obj","/Fd$OutDir\rp1i2saud.pdb",$s)
    & $Cl @args
    if ($LASTEXITCODE -ne 0) { throw "cl failed on $([IO.Path]::GetFileName($s)) (exit $LASTEXITCODE)" }
}

Write-Host "== Compiling C HAL ($Arch) ==" -ForegroundColor Cyan
foreach ($s in $CSources) {
    $obj = Join-Path $OutDir ([IO.Path]::GetFileNameWithoutExtension($s) + ".obj")
    $Objs += $obj
    $args = @("/nologo","/c","/W4","/Zi","/kernel","/GS",
              "/DNDEBUG") + $ArchDefs + $CommonInc + @("/Fo$obj",$s)
    & $Cl @args
    if ($LASTEXITCODE -ne 0) { throw "cl failed on $([IO.Path]::GetFileName($s)) (exit $LASTEXITCODE)" }
}

if ($CompileOnly) {
    Write-Host "== Compile-only OK: $($Objs.Count) objs ==" -ForegroundColor Green
    $Objs | ForEach-Object { "  " + (Split-Path $_ -Leaf) }
    return
}

$Sys = Join-Path $OutDir "rp1i2saud.sys"
Write-Host "== Linking ($Arch) ==" -ForegroundColor Cyan
$linkArgs = @("/nologo","/DRIVER","/SUBSYSTEM:NATIVE,10.0","/MACHINE:$Machine",
              "/NODEFAULTLIB","/INTEGRITYCHECK","/ENTRY:GsDriverEntry","/DEBUG",
              "/PDB:$OutDir\rp1i2saud.pdb","/LIBPATH:$LibKm",
              "portcls.lib","stdunk.lib","ksguid.lib","libcntpr.lib",
              "BufferOverflowFastFailK.lib","ntoskrnl.lib","hal.lib",
              "/OUT:$Sys") + $Objs
& $Link @linkArgs
if ($LASTEXITCODE -ne 0) { throw "link failed (exit $LASTEXITCODE)" }

Copy-Item (Join-Path $ProjDir "rp1i2saud.inf") $OutDir -Force -EA SilentlyContinue
$Stampinf = Get-ChildItem "$Kit\bin\$SdkVer\x64\stampinf.exe" -EA SilentlyContinue | Select-Object -First 1
if ($Stampinf -and (Test-Path (Join-Path $OutDir "rp1i2saud.inf"))) {
    & $Stampinf.FullName -f (Join-Path $OutDir "rp1i2saud.inf") -d "*" -a $(if($Arch -eq "ARM64"){"arm64"}else{"amd64"}) -v "1.0.0.0" | Out-Null
}

New-Item -ItemType Directory -Force $Deliver | Out-Null
Copy-Item $Sys $Deliver -Force
if (Test-Path (Join-Path $OutDir "rp1i2saud.inf")) { Copy-Item (Join-Path $OutDir "rp1i2saud.inf") $Deliver -Force }
Write-Host "== DELIVERED to $Deliver ==" -ForegroundColor Green
Get-ChildItem $Deliver | Select-Object Name,Length | Format-Table -AutoSize
