param([string]$BuildDirectory = 'build', [string]$OutputDirectory = 'dist')
. (Join-Path $PSScriptRoot 'release-common.ps1')
$source = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$build = (Resolve-Path -LiteralPath $BuildDirectory).Path
$exe = Join-Path $build 'Release/ZeroPad.exe'
$record = Get-Content -LiteralPath ($exe + '.build.json') -Raw | ConvertFrom-Json
if ($record.Configuration -ne 'Release' -or $record.PointerBytes -ne 8 -or $record.Version -ne '1.0.0') { throw 'Expected Release x64 1.0.0 build' }
if ((Get-InventoryDigest @(Get-BinaryInputs $source)) -ne $record.BinaryInputsSHA256) { throw 'Sources changed since linking; rebuild ZeroPad' }
if ((Get-FileHash -LiteralPath $exe).Hash.ToLowerInvariant() -ne $record.ExecutableSHA256) { throw 'Executable changed since linking' }
& ctest --test-dir $build -C Release -LE interactive --output-on-failure --no-tests=error
if ($LASTEXITCODE) { throw 'Release regression tests failed' }
$stage = Join-Path $build ('package-' + [Guid]::NewGuid().ToString('N'))
& cmake --install $build --config Release --prefix $stage
if ($LASTEXITCODE) { throw 'Installation failed' }
$payload = Join-Path $stage 'ZeroPad'
$audit = & (Join-Path $PSScriptRoot 'audit-binary.ps1') -Executable (Join-Path $payload 'ZeroPad.exe')
$commit = & git -C $source rev-parse HEAD
if ($LASTEXITCODE) { throw 'Cannot identify source commit' }
$dirty = @(& git -C $source status --porcelain).Count -ne 0
$inventory = @(& git -C $source ls-files --cached --others --exclude-standard | Sort-Object -Unique | ForEach-Object {
    $path = Join-Path $source $_
    if (Test-Path -LiteralPath $path -PathType Leaf) { [ordered]@{ Path = $_; SHA256 = (Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant() } }
})
$info = [ordered]@{ Candidate = $true; BaseCommit = $commit; Dirty = $dirty; Build = $record; Snapshot = $inventory; DependencyAudit = $audit; Verification = 'Release noninteractive CTest passed; extracted hash/version verified. Physical input and clean-machine launch pending.' }
Write-JsonFile (Join-Path $payload 'BUILD_INFO.json') $info
$out = [IO.Path]::GetFullPath($OutputDirectory)
[IO.Directory]::CreateDirectory($out) | Out-Null
$zip = Join-Path $out 'ZeroPad-v1.0.0-win-x64.zip'
$temporary = Join-Path $out ([Guid]::NewGuid().ToString('N') + '.zip')
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::Open($temporary, [IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($file in (Get-ChildItem -LiteralPath $stage -Recurse -File | Sort-Object FullName)) {
        $name = $file.FullName.Substring($stage.Length + 1).Replace('\','/')
        $entry = $archive.CreateEntry($name, [IO.Compression.CompressionLevel]::Optimal)
        $entry.LastWriteTime = [DateTimeOffset]::new(1980,1,1,0,0,0,[TimeSpan]::Zero)
        $inputStream = [IO.File]::OpenRead($file.FullName)
        $outputStream = $entry.Open()
        try { $inputStream.CopyTo($outputStream) }
        finally { $outputStream.Dispose(); $inputStream.Dispose() }
    }
} finally { $archive.Dispose() }
$extract = Join-Path $build ('verify-' + [Guid]::NewGuid().ToString('N'))
[IO.Compression.ZipFile]::ExtractToDirectory($temporary, $extract)
$executables = @(Get-ChildItem -LiteralPath $extract -Recurse -Filter '*.exe')
if ($executables.Count -ne 1 -or (Get-FileHash -LiteralPath $executables[0].FullName).Hash.ToLowerInvariant() -ne $record.ExecutableSHA256) { throw 'Extracted payload mismatch' }
$null = & (Join-Path $PSScriptRoot 'audit-binary.ps1') -Executable $executables[0].FullName
foreach ($required in @('README.md','LICENSE','THIRD_PARTY_NOTICES.txt','CHANGELOG.md','BUILD_INFO.json','docs/configuration.md','docs/troubleshooting.md')) {
    if (!(Test-Path -LiteralPath (Join-Path $extract "ZeroPad/$required"))) { throw "Missing $required" }
}
Move-Item -LiteralPath $temporary -Destination $zip -Force
$sha = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText(($zip + '.sha256'), "$sha  $([IO.Path]::GetFileName($zip))`n", [Text.UTF8Encoding]::new($false))
Write-JsonFile ($zip + '.build.json') $info
Write-Output "Verified $zip SHA256=$sha"
