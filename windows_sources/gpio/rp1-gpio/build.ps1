<#
.SYNOPSIS  Build the RP1 GPIO GpioClx driver for ARM64 (KMDF + GpioClx).
.EXAMPLE   pwsh -File build.ps1 -CompileOnly   # scaffold check
           pwsh -File build.ps1                 # full build (needs ddi.c)
#>
[CmdletBinding()]
param(
    [ValidateSet("ARM64","x64")] [string]$Arch = "ARM64",
    [string]$SdkVer = "10.0.26100.0",
    [string]$KmdfVer = "1.33",
    [switch]$CompileOnly
)
$ErrorActionPreference = "Stop"
$ProjDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$Kit     = "C:\Program Files (x86)\Windows Kits\10"
$VsRoot  = "C:\Program Files\Microsoft Visual Studio\2022\Community"
$Deliver = "C:\ai_project\AprWindowsDriver\windows_driver\gpio"

$ClArch  = $Arch.ToLower(); $Machine = $Arch.ToUpper()
$ArchDefs = if ($Arch -eq "ARM64") { @("/D_ARM64_","/DARM64") } else { @("/D_AMD64_","/DAMD64") }
$KmdfMaj = $KmdfVer.Split('.')[0]; $KmdfMin = $KmdfVer.Split('.')[1]

$Vc   = Get-ChildItem "$VsRoot\VC\Tools\MSVC" -Directory | Sort-Object Name -Descending | Select-Object -First 1
$Cl   = Join-Path $Vc.FullName "bin\Hostx64\$ClArch\cl.exe"
$Link = Join-Path $Vc.FullName "bin\Hostx64\$ClArch\link.exe"

$IncKmdf="$Kit\Include\wdf\kmdf\$KmdfVer"
$IncKmCrt="$Kit\Include\$SdkVer\km\crt"; $IncKm="$Kit\Include\$SdkVer\km"; $IncShared="$Kit\Include\$SdkVer\shared"
$IncRp1Bus = (Resolve-Path (Join-Path $ProjDir "..\..\pcie-rp1\rp1bus")).Path  # rp1bus_if.h (bus interface contract)
$LibKm="$Kit\Lib\$SdkVer\km\$ClArch"; $LibKmdf="$Kit\Lib\wdf\kmdf\$ClArch\$KmdfVer"

$OutDir = Join-Path $ProjDir "build\$Arch"; New-Item -ItemType Directory -Force $OutDir | Out-Null
$Sources = Get-ChildItem $ProjDir -Filter *.c | Select-Object -ExpandProperty FullName
$Objs=@()
Write-Host "== Compiling ($Arch, KMDF $KmdfVer + GpioClx) ==" -ForegroundColor Cyan
foreach ($s in $Sources) {
    $obj = Join-Path $OutDir ([IO.Path]::GetFileNameWithoutExtension($s) + ".obj"); $Objs += $obj
    $a = @("/nologo","/c","/W4","/O2","/Gy","/Zi","/kernel","/GS","/D_WIN64","/DNDEBUG",
           "/DKMDF_VERSION_MAJOR=$KmdfMaj","/DKMDF_VERSION_MINOR=$KmdfMin") + $ArchDefs + @(
           "/I$IncKmdf","/I$IncKmCrt","/I$IncKm","/I$IncShared","/I$IncRp1Bus","/Fo$obj","/Fd$OutDir\rp1gpio.pdb",$s)
    & $Cl @a
    if ($LASTEXITCODE -ne 0) { throw "cl failed on $([IO.Path]::GetFileName($s)) (exit $LASTEXITCODE)" }
}
if ($CompileOnly) { Write-Host "== Compile-only OK: $($Objs.Count) objs ==" -ForegroundColor Green; return }

$Sys = Join-Path $OutDir "rp1gpio.sys"
Write-Host "== Linking ($Arch) ==" -ForegroundColor Cyan
$la = @("/nologo","/DRIVER","/SUBSYSTEM:NATIVE,10.0","/MACHINE:$Machine","/NODEFAULTLIB","/INTEGRITYCHECK",
        "/ENTRY:FxDriverEntry","/DEBUG","/PDB:$OutDir\rp1gpio.pdb",
        "/LIBPATH:$LibKm","/LIBPATH:$LibKmdf",
        "wdfdriverentry.lib","wdfldr.lib","msgpioclxstub.lib","libcntpr.lib","BufferOverflowFastFailK.lib","ntoskrnl.lib","hal.lib",
        "/OUT:$Sys") + $Objs
& $Link @la
if ($LASTEXITCODE -ne 0) { throw "link failed (exit $LASTEXITCODE)" }
New-Item -ItemType Directory -Force $Deliver | Out-Null; Copy-Item $Sys $Deliver -Force
Write-Host "== DELIVERED to $Deliver ==" -ForegroundColor Green
Get-ChildItem $Deliver | Select-Object Name,Length | Format-Table -AutoSize
