# SPDX-License-Identifier: Apache-2.0
param(
    [Parameter(Mandatory = $true)]
    [string]$ElfFile,
    [string]$Probe = 'cmsis_dap',
    [string]$OpenOcdPath,
    [string]$ConfigRoot
)

$ErrorActionPreference = 'Stop'

$glueRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path
$hpmRoot = Split-Path $glueRoot -Parent
if (-not $OpenOcdPath) {
    $OpenOcdPath = Join-Path $hpmRoot 'sdk_env\tools\openocd\openocd.exe'
}
if (-not $ConfigRoot) {
    $ConfigRoot = Join-Path $hpmRoot 'sdk_env\hpm_sdk\boards\openocd'
}

$elf = (Resolve-Path -LiteralPath $ElfFile).Path
$probeCfg = Join-Path $ConfigRoot "probes\$Probe.cfg"
$socCfg = Join-Path $ConfigRoot 'soc\hpm5e00.cfg'
$boardCfg = Join-Path $PSScriptRoot 'openocd.cfg'
foreach ($path in @($OpenOcdPath, $probeCfg, $socCfg, $boardCfg)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required file not found: $path"
    }
}

$stream = [System.IO.File]::OpenRead($elf)
try {
    $reader = [System.IO.BinaryReader]::new($stream)
    $ident = $reader.ReadBytes(6)
    if ($ident.Length -ne 6 -or $ident[0] -ne 0x7f -or
        $ident[1] -ne 0x45 -or $ident[2] -ne 0x4c -or
        $ident[3] -ne 0x46 -or $ident[4] -ne 1 -or $ident[5] -ne 1) {
        throw "Expected a little-endian ELF32 image: $elf"
    }
    $stream.Position = 24
    $entry = $reader.ReadUInt32()
} finally {
    $stream.Dispose()
}
if ($entry -lt 2147483648 -or $entry -ge 2148532224) {
    throw ('ELF entry 0x{0:X8} is outside the HPM5E31 XPI0 Flash range. Rebuild with CONFIG_XIP=y.' -f $entry)
}

$elfOpenOcd = $elf.Replace('\', '/')
$command = 'program "' + $elfOpenOcd + '" verify; reset_soc; shutdown'
& $OpenOcdPath -s $ConfigRoot -f $probeCfg -f $socCfg -f $boardCfg -c $command
if ($LASTEXITCODE -ne 0) {
    throw "OpenOCD failed with exit code $LASTEXITCODE"
}
