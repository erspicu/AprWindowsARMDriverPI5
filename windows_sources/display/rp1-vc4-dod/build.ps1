<#
.SYNOPSIS
    Build the RP1/vc4 Display-Only Driver (WDDM DOD) for ARM64 via direct
    cl.exe/link.exe. DxgkInitializeDisplayOnlyDriver lives in displib.lib
    (verified with dumpbin; dxgkrnl.lib does NOT exist in km\arm64).
.EXAMPLE
    pwsh -File build.ps1 -CompileOnly
    pwsh -File build.ps1
#>
[CmdletBinding()]
param(
    [ValidateSet("ARM64","x64")] [string]$Arch = "ARM64",
    [string]$SdkVer = "10.0.26100.0",
    [switch]$CompileOnly
)
$ErrorActionPreference = "Stop"
$ProjDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$Kit     = "C:\Program Files (x86)\Windows Kits\10"
$VsRoot  = "C:\Program Files\Microsoft Visual Studio\2022\Community"
$Deliver = "C:\ai_project\AprWindowsDriver\windows_driver\display"

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

$Sources = Get-ChildItem $ProjDir -Filter *.c | Select-Object -ExpandProperty FullName
$Objs = @()
Write-Host "== Compiling ($Arch) ==" -ForegroundColor Cyan
foreach ($s in $Sources) {
    $obj = Join-Path $OutDir ([IO.Path]::GetFileNameWithoutExtension($s) + ".obj")
    $Objs += $obj
    $args = @("/nologo","/c","/W4","/O2","/Gy","/Zi","/kernel","/GS",
              "/D_WIN64","/DNDEBUG") + $ArchDefs + @(
              "/I$IncKmCrt","/I$IncKm","/I$IncShared","/Fo$obj","/Fd$OutDir\rp1vc4dod.pdb",$s)
    & $Cl @args
    if ($LASTEXITCODE -ne 0) { throw "cl failed on $([IO.Path]::GetFileName($s)) (exit $LASTEXITCODE)" }
}

if ($CompileOnly) {
    Write-Host "== Compile-only OK: $($Objs.Count) objs ==" -ForegroundColor Green
    $Objs | ForEach-Object { "  " + (Split-Path $_ -Leaf) }
    return
}

$Sys = Join-Path $OutDir "rp1vc4dod.sys"
Write-Host "== Linking ($Arch) ==" -ForegroundColor Cyan
$linkArgs = @("/nologo","/DRIVER","/SUBSYSTEM:NATIVE,10.0","/MACHINE:$Machine",
              "/NODEFAULTLIB","/INTEGRITYCHECK","/ENTRY:GsDriverEntry","/DEBUG",
              "/PDB:$OutDir\rp1vc4dod.pdb","/LIBPATH:$LibKm",
              "displib.lib","libcntpr.lib","BufferOverflowFastFailK.lib","ntoskrnl.lib","hal.lib",
              "/OUT:$Sys") + $Objs
& $Link @linkArgs
if ($LASTEXITCODE -ne 0) { throw "link failed (exit $LASTEXITCODE)" }

New-Item -ItemType Directory -Force $Deliver | Out-Null
Copy-Item $Sys $Deliver -Force
if (Test-Path (Join-Path $ProjDir "rp1vc4dod.inf")) { Copy-Item (Join-Path $ProjDir "rp1vc4dod.inf") $Deliver -Force }
Write-Host "== DELIVERED to $Deliver ==" -ForegroundColor Green
Get-ChildItem $Deliver | Select-Object Name,Length | Format-Table -AutoSize
