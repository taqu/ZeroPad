param([Parameter(Mandatory=$true)][string]$SourceRoot,
      [Parameter(Mandatory=$true)][string]$BuildInfo,
      [Parameter(Mandatory=$true)][string]$Executable,
      [Parameter(Mandatory=$true)][string]$Configuration)
. (Join-Path $PSScriptRoot 'release-common.ps1')
$inputs = @(Get-BinaryInputs $SourceRoot)
$info = Get-Content -LiteralPath $BuildInfo -Raw | ConvertFrom-Json
$record = [ordered]@{
    Version = $info.Version; Configuration = $Configuration
    CompilerId = $info.CompilerId; CompilerVersion = $info.CompilerVersion
    Generator = $info.Generator; Platform = $info.Platform; PointerBytes = $info.PointerBytes; System = $info.System
    ExecutableSHA256 = (Get-FileHash -LiteralPath $Executable -Algorithm SHA256).Hash.ToLowerInvariant()
    BinaryInputsSHA256 = Get-InventoryDigest $inputs
    Inputs = $inputs
}
Write-JsonFile ($Executable + '.build.json') $record
