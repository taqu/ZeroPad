param([Parameter(Mandatory=$true)][string]$Tag,
      [Parameter(Mandatory=$true)][string]$BuildInfo)
. (Join-Path $PSScriptRoot 'release-common.ps1')
$version = Get-ReleaseVersion $Tag
$configured = Get-Content -LiteralPath $BuildInfo -Raw | ConvertFrom-Json
if ($version.Version -cne $configured.Version) { throw "Tag $Tag does not match configured source version $($configured.Version)" }
if ($configured.Platform -ne 'x64' -or $configured.PointerBytes -ne 8 -or $configured.CompilerId -ne 'MSVC') { throw 'Expected MSVC Windows x64 configuration' }
if ($env:GITHUB_OUTPUT) {
    @("version=$($version.Version)", "prerelease=$($version.Prerelease.ToString().ToLowerInvariant())") | Add-Content -LiteralPath $env:GITHUB_OUTPUT -Encoding utf8
}
$version
