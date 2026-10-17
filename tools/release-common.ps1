# Shared only by the local release scripts. No network access or publication.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
function Get-ReleaseVersion([string]$Tag) {
    if ($Tag -cnotmatch '\Av(?<core>(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*))(?<pre>-(?:alpha|beta|rc)\.(?:0|[1-9][0-9]*))?\z') {
        throw 'Expected vMAJOR.MINOR.PATCH optionally followed by -alpha.N, -beta.N or -rc.N'
    }
    $core = $Matches.core
    $pre = if ($Matches.ContainsKey('pre')) { $Matches.pre } else { '' }
    foreach ($part in $core.Split('.')) {
        if ([decimal]$part -gt 65535) { throw 'Version exceeds Windows 16-bit resource component range' }
    }
    [pscustomobject]@{ Version = $Tag.Substring(1); FileVersion = "$core.0"; Prerelease = $pre.Length -gt 0 }
}
function Get-BinaryInputs([string]$SourceRoot) {
    $root = (Resolve-Path -LiteralPath $SourceRoot).Path.TrimEnd('\','/')
    $paths = @((Join-Path $root 'CMakeLists.txt'))
    foreach ($folder in @('src','include','resources','third_party','cmake')) {
        $paths += Get-ChildItem -LiteralPath (Join-Path $root $folder) -Recurse -File | ForEach-Object FullName
    }
    @($paths | Sort-Object -Unique | ForEach-Object {
        [ordered]@{ Path = $_.Substring($root.Length + 1).Replace('\','/'); SHA256 = (Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash.ToLowerInvariant() }
    })
}
function Get-InventoryDigest($Inventory) {
    $records = @($Inventory | ForEach-Object { $_.Path + ' ' + $_.SHA256 })
    $bytes = [Text.Encoding]::UTF8.GetBytes(($records -join "`n") + "`n")
    $algorithm = [Security.Cryptography.SHA256]::Create()
    try { ([BitConverter]::ToString($algorithm.ComputeHash($bytes))).Replace('-','').ToLowerInvariant() }
    finally { $algorithm.Dispose() }
}
function Write-JsonFile([string]$Path, $Value) {
    [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 10) + "`n", [Text.UTF8Encoding]::new($false))
}
