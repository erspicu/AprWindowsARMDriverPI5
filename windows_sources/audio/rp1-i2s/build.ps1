<#
.SYNOPSIS
    Build the RP1 I2S WDM driver (.sys) for ARM64 (or x64) by invoking the
    MSVC cross compiler / linker directly against the WDK headers and libs.

    We do NOT use an MSBuild "Driver" (.vcxproj) project because the winget
    WDK package does not install the Visual Studio WDK MSBuild integration
    (no WDK.vsix / WindowsKernelModeDriver10.0 toolset). A direct cl.exe /
    link.exe build is self-contained and deterministic.

.EXAMPLE
    pwsh -File build.ps1                # ARM64 (default)
    pwsh -File build.ps1 -Arch x64      # x64 sanity build
    pwsh -File build.ps1 -Sign          # also stamp + inf2cat + self-signed test cert
#>
[CmdletBinding()]
param(
    [ValidateSet("ARM64","x64")] [string]$Arch = "ARM64",
    [string]$SdkVer = "10.0.26100.0",
    [switch]$Sign
)
$ErrorActionPreference = "Stop"
$ProjDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$Kit     = "C:\Program Files (x86)\Windows Kits\10"
$VsRoot  = "C:\Program Files\Microsoft Visual Studio\2022\Community"
$Deliver = "C:\ai_project\AprWindowsDriver\windows_driver\audio"

$ClArch    = $Arch.ToLower()                               # arm64 | x64  (tool dir)
$Machine   = $Arch.ToUpper()                               # ARM64 | X64  (/MACHINE)
$StampArch = if ($Arch -eq "ARM64") { "arm64" } else { "amd64" }
$ArchDefs  = if ($Arch -eq "ARM64") { @("/D_ARM64_","/DARM64") } else { @("/D_AMD64_","/DAMD64") }

$Vc   = Get-ChildItem "$VsRoot\VC\Tools\MSVC" -Directory | Sort-Object Name -Descending | Select-Object -First 1
$Cl   = Join-Path $Vc.FullName "bin\Hostx64\$ClArch\cl.exe"
$Link = Join-Path $Vc.FullName "bin\Hostx64\$ClArch\link.exe"
foreach ($t in @($Cl,$Link)) { if (-not (Test-Path $t)) { throw "missing tool: $t (install MSVC $Arch build tools)" } }

$IncVc     = Join-Path $Vc.FullName "include"
$IncKmCrt  = "$Kit\Include\$SdkVer\km\crt"
$IncKm     = "$Kit\Include\$SdkVer\km"
$IncShared = "$Kit\Include\$SdkVer\shared"
$LibKm     = "$Kit\Lib\$SdkVer\km\$ClArch"

$OutDir = Join-Path $ProjDir "build\$Arch"
New-Item -ItemType Directory -Force $OutDir | Out-Null

$Sources = @("rp1_i2s_hw.c","driver.c","pnp.c")
$Objs = @()
Write-Host "== Compiling ($Arch) with $Cl ==" -ForegroundColor Cyan
foreach ($s in $Sources) {
    $obj = Join-Path $OutDir ([IO.Path]::GetFileNameWithoutExtension($s) + ".obj")
    $Objs += $obj
    # Kernel CRT (km\crt) is self-contained and must precede everything; do NOT
    # add the VC user-mode include (it pulls corecrt.h from the UCRT).
    $clArgs = @("/nologo","/c","/W4","/O2","/Gy","/Zi","/kernel","/GS",
                "/D_WIN64","/DNDEBUG") + $ArchDefs + @(
                "/I$IncKmCrt","/I$IncKm","/I$IncShared",
                "/Fo$obj","/Fd$OutDir\rp1i2s.pdb",(Join-Path $ProjDir $s))
    & $Cl @clArgs
    if ($LASTEXITCODE -ne 0) { throw "cl failed on $s (exit $LASTEXITCODE)" }
}

$Sys = Join-Path $OutDir "rp1i2s.sys"
Write-Host "== Linking ($Arch) ==" -ForegroundColor Cyan
$linkArgs = @("/nologo","/DRIVER","/SUBSYSTEM:NATIVE,10.0","/MACHINE:$Machine",
              "/NODEFAULTLIB","/INTEGRITYCHECK","/ENTRY:GsDriverEntry","/DEBUG",
              "/PDB:$OutDir\rp1i2s.pdb","/LIBPATH:$LibKm",
              "BufferOverflowFastFailK.lib","ntoskrnl.lib","hal.lib",
              "/OUT:$Sys") + $Objs
& $Link @linkArgs
if ($LASTEXITCODE -ne 0) { throw "link failed (exit $LASTEXITCODE)" }

# stamp a copy of the INF with DriverVer + arch
Copy-Item (Join-Path $ProjDir "rp1i2s.inf") $OutDir -Force
$Stampinf = (Get-ChildItem "$Kit\bin\$SdkVer\x64\stampinf.exe" -EA SilentlyContinue | Select-Object -First 1)
if ($Stampinf) {
    & $Stampinf.FullName -f (Join-Path $OutDir "rp1i2s.inf") -d "*" -a $StampArch -v "1.0.0.0" | Out-Null
}

if ($Sign) {
    $cn = "CN=AprWindowsDriver-Test"
    $cert = Get-ChildItem Cert:\CurrentUser\My | Where-Object Subject -eq $cn | Select-Object -First 1
    if (-not $cert) { $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject $cn -CertStoreLocation Cert:\CurrentUser\My -KeyUsage DigitalSignature -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3") }
    $inf2cat = Get-ChildItem "$Kit\bin\$SdkVer\x86\inf2cat.exe","$Kit\bin\x86\inf2cat.exe" -EA SilentlyContinue | Select-Object -First 1
    if ($inf2cat) { & $inf2cat.FullName /driver:$OutDir /os:10_NI_ARM64,10_VB_ARM64 | Out-Null }
    $signtool = Get-ChildItem "$Kit\bin\$SdkVer\x64\signtool.exe" -EA SilentlyContinue | Select-Object -First 1
    if ($signtool) {
        & $signtool.FullName sign /fd SHA256 /a /s My /n "AprWindowsDriver-Test" $Sys
        $cat = Join-Path $OutDir "rp1i2s.cat"
        if (Test-Path $cat) { & $signtool.FullName sign /fd SHA256 /a /s My /n "AprWindowsDriver-Test" $cat }
    }
}

New-Item -ItemType Directory -Force $Deliver | Out-Null
Copy-Item $Sys,(Join-Path $OutDir "rp1i2s.inf") $Deliver -Force
$catOut = Join-Path $OutDir "rp1i2s.cat"; if (Test-Path $catOut) { Copy-Item $catOut $Deliver -Force }

Write-Host "== DELIVERED to $Deliver ==" -ForegroundColor Green
Get-ChildItem $Deliver | Select-Object Name,Length,LastWriteTime | Format-Table -AutoSize
