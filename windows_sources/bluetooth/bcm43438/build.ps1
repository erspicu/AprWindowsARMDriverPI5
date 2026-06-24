<#
.SYNOPSIS  Build the BCM43438 Bluetooth UART HCI transport (KMDF) for ARM64.
.EXAMPLE   pwsh -File build.ps1 [-CompileOnly]
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
$Deliver = "C:\ai_project\AprWindowsDriver\windows_driver\bluetooth"

$ClArch  = $Arch.ToLower(); $Machine = $Arch.ToUpper()
$ArchDefs = if ($Arch -eq "ARM64") { @("/D_ARM64_","/DARM64") } else { @("/D_AMD64_","/DAMD64") }
$KmdfMaj = $KmdfVer.Split('.')[0]; $KmdfMin = $KmdfVer.Split('.')[1]

$Vc   = Get-ChildItem "$VsRoot\VC\Tools\MSVC" -Directory | Sort-Object Name -Descending | Select-Object -First 1
$Cl   = Join-Path $Vc.FullName "bin\Hostx64\$ClArch\cl.exe"
$Link = Join-Path $Vc.FullName "bin\Hostx64\$ClArch\link.exe"
$IncKmdf="$Kit\Include\wdf\kmdf\$KmdfVer"
$IncKmCrt="$Kit\Include\$SdkVer\km\crt"; $IncKm="$Kit\Include\$SdkVer\km"; $IncShared="$Kit\Include\$SdkVer\shared"
$LibKm="$Kit\Lib\$SdkVer\km\$ClArch"; $LibKmdf="$Kit\Lib\wdf\kmdf\$ClArch\$KmdfVer"

$OutDir = Join-Path $ProjDir "build\$Arch"; New-Item -ItemType Directory -Force $OutDir | Out-Null
$Sources = Get-ChildItem $ProjDir -Filter *.c | Select-Object -ExpandProperty FullName
$Objs=@()
Write-Host "== Compiling ($Arch, KMDF $KmdfVer) ==" -ForegroundColor Cyan
foreach ($s in $Sources) {
    $obj = Join-Path $OutDir ([IO.Path]::GetFileNameWithoutExtension($s) + ".obj"); $Objs += $obj
    $a = @("/nologo","/c","/W4","/O2","/Gy","/Zi","/kernel","/GS","/D_WIN64","/DNDEBUG",
           "/DNTDDI_VERSION=0x0A00000C","/D_WIN32_WINNT=0x0A00",   # Win11 (ExAllocatePool2 etc.)
           "/DKMDF_VERSION_MAJOR=$KmdfMaj","/DKMDF_VERSION_MINOR=$KmdfMin") + $ArchDefs + @(
           "/I$IncKmdf","/I$IncKmCrt","/I$IncKm","/I$IncShared","/Fo$obj","/Fd$OutDir\btbcm.pdb",$s)
    & $Cl @a
    if ($LASTEXITCODE -ne 0) { throw "cl failed on $([IO.Path]::GetFileName($s)) (exit $LASTEXITCODE)" }
}
if ($CompileOnly) { Write-Host "== Compile-only OK: $($Objs.Count) objs ==" -ForegroundColor Green; return }

$Sys = Join-Path $OutDir "btbcm.sys"
Write-Host "== Linking ($Arch) ==" -ForegroundColor Cyan
$la = @("/nologo","/DRIVER","/SUBSYSTEM:NATIVE,10.0","/MACHINE:$Machine","/NODEFAULTLIB","/INTEGRITYCHECK",
        "/ENTRY:FxDriverEntry","/DEBUG","/PDB:$OutDir\btbcm.pdb","/LIBPATH:$LibKm","/LIBPATH:$LibKmdf",
        "wdfdriverentry.lib","wdfldr.lib","libcntpr.lib","BufferOverflowFastFailK.lib","ntoskrnl.lib","hal.lib",
        "/OUT:$Sys") + $Objs
& $Link @la
if ($LASTEXITCODE -ne 0) { throw "link failed (exit $LASTEXITCODE)" }
New-Item -ItemType Directory -Force $Deliver | Out-Null; Copy-Item $Sys $Deliver -Force
Write-Host "== DELIVERED to $Deliver ==" -ForegroundColor Green
Get-ChildItem $Deliver | Select-Object Name,Length | Format-Table -AutoSize
