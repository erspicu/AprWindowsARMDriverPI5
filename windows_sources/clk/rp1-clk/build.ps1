<# Build the RP1 clock KMDF driver for ARM64. #>
[CmdletBinding()]
param([ValidateSet("ARM64","x64")][string]$Arch="ARM64",[string]$SdkVer="10.0.26100.0",[string]$KmdfVer="1.33",[switch]$CompileOnly)
$ErrorActionPreference="Stop"
$ProjDir=Split-Path -Parent $MyInvocation.MyCommand.Definition
$Kit="C:\Program Files (x86)\Windows Kits\10"; $VsRoot="C:\Program Files\Microsoft Visual Studio\2022\Community"
$Deliver="C:\ai_project\AprWindowsDriver\windows_driver\clk"
$ClArch=$Arch.ToLower(); $Machine=$Arch.ToUpper()
$ArchDefs= if($Arch -eq "ARM64"){@("/D_ARM64_","/DARM64")}else{@("/D_AMD64_","/DAMD64")}
$KmdfMaj=$KmdfVer.Split('.')[0]; $KmdfMin=$KmdfVer.Split('.')[1]
$Vc=Get-ChildItem "$VsRoot\VC\Tools\MSVC" -Directory | Sort-Object Name -Descending | Select-Object -First 1
$Cl=Join-Path $Vc.FullName "bin\Hostx64\$ClArch\cl.exe"; $Link=Join-Path $Vc.FullName "bin\Hostx64\$ClArch\link.exe"
$IncKmdf="$Kit\Include\wdf\kmdf\$KmdfVer"; $IncKmCrt="$Kit\Include\$SdkVer\km\crt"; $IncKm="$Kit\Include\$SdkVer\km"; $IncShared="$Kit\Include\$SdkVer\shared"
$LibKm="$Kit\Lib\$SdkVer\km\$ClArch"; $LibKmdf="$Kit\Lib\wdf\kmdf\$ClArch\$KmdfVer"
$OutDir=Join-Path $ProjDir "build\$Arch"; New-Item -ItemType Directory -Force $OutDir | Out-Null
$Sources=Get-ChildItem $ProjDir -Filter *.c | Select-Object -ExpandProperty FullName; $Objs=@()
Write-Host "== Compiling ($Arch, KMDF) ==" -ForegroundColor Cyan
foreach($s in $Sources){
  $obj=Join-Path $OutDir ([IO.Path]::GetFileNameWithoutExtension($s)+".obj"); $Objs+=$obj
  $a=@("/nologo","/c","/W4","/O2","/Gy","/Zi","/kernel","/GS","/D_WIN64","/DNDEBUG","/DKMDF_VERSION_MAJOR=$KmdfMaj","/DKMDF_VERSION_MINOR=$KmdfMin")+$ArchDefs+@("/I$IncKmdf","/I$IncKmCrt","/I$IncKm","/I$IncShared","/Fo$obj","/Fd$OutDir\rp1clk.pdb",$s)
  & $Cl @a; if($LASTEXITCODE -ne 0){throw "cl failed on $([IO.Path]::GetFileName($s))"}
}
if($CompileOnly){Write-Host "== Compile-only OK ==" -ForegroundColor Green; return}
$Sys=Join-Path $OutDir "rp1clk.sys"
Write-Host "== Linking ==" -ForegroundColor Cyan
$la=@("/nologo","/DRIVER","/SUBSYSTEM:NATIVE,10.0","/MACHINE:$Machine","/NODEFAULTLIB","/INTEGRITYCHECK","/ENTRY:FxDriverEntry","/DEBUG","/PDB:$OutDir\rp1clk.pdb","/LIBPATH:$LibKm","/LIBPATH:$LibKmdf","wdfdriverentry.lib","wdfldr.lib","libcntpr.lib","BufferOverflowFastFailK.lib","ntoskrnl.lib","hal.lib","/OUT:$Sys")+$Objs
& $Link @la; if($LASTEXITCODE -ne 0){throw "link failed"}
New-Item -ItemType Directory -Force $Deliver | Out-Null; Copy-Item $Sys $Deliver -Force
Write-Host "== DELIVERED to $Deliver ==" -ForegroundColor Green
Get-ChildItem $Deliver | Select-Object Name,Length | Format-Table -AutoSize
