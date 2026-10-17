param([Parameter(Mandatory=$true)][string]$SourceRoot)
. (Join-Path $SourceRoot 'tools/release-common.ps1')
foreach ($tag in @('v0.0.0','v1.0.0','v65535.65535.65535','v1.1.0-alpha.1','v1.1.0-beta.0','v1.1.0-rc.1')) {
    $version = Get-ReleaseVersion $tag
    if ($version.Version -cne $tag.Substring(1)) { throw "Incorrect version $tag" }
    if ($version.Prerelease -ne $tag.Contains('-')) { throw "Incorrect prerelease $tag" }
    if ($version.FileVersion -cne (($tag.Substring(1) -split '-')[0] + '.0')) { throw "Incorrect numeric version $tag" }
}
foreach ($tag in @('v1','v1.0','v1.0.0-test','release-1.0.0','vabc','v01.0.0','v1.00.0','v1.0.0-rc.01','v1.0.0-RC.1','v1.0.0-rc','v1.0.0+build','v65536.0.0',"v1.0.0`n",'v1.0.0;exit')) {
    $rejected = $false
    try { $null = Get-ReleaseVersion $tag } catch { $rejected = $true }
    if (!$rejected) { throw "Accepted invalid tag: $tag" }
}
$fixture = [IO.Path]::GetTempFileName()
try {
    foreach ($tag in @('v1.0.0','v1.1.0-rc.1')) {
        $version = Get-ReleaseVersion $tag
        Write-JsonFile $fixture @{ Version = $version.Version; Platform = 'x64'; PointerBytes = 8; CompilerId = 'MSVC' }
        $validated = & (Join-Path $SourceRoot 'tools/validate-release.ps1') -Tag $tag -BuildInfo $fixture
        if ($validated.Version -cne $version.Version) { throw 'Metadata validation failed' }
        $rejected = $false
        try { $null = & (Join-Path $SourceRoot 'tools/validate-release.ps1') -Tag 'v9.9.9' -BuildInfo $fixture } catch { $rejected = $true }
        if (!$rejected) { throw 'Version mismatch accepted' }
    }
    Write-JsonFile $fixture @{ Version = '1.0.0'; Platform = 'Win32'; PointerBytes = 4; CompilerId = 'MSVC' }
    $rejected = $false
    try { $null = & (Join-Path $SourceRoot 'tools/validate-release.ps1') -Tag 'v1.0.0' -BuildInfo $fixture } catch { $rejected = $true }
    if (!$rejected) { throw 'Wrong architecture accepted' }
} finally { Remove-Item -LiteralPath $fixture }
Write-Output 'Stable/prerelease syntax, numeric resource version, metadata mismatch and rejection cases passed'
