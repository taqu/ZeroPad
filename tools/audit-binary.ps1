param([Parameter(Mandatory=$true)][string]$Executable,
      [Parameter(Mandatory=$true)][string]$ExpectedVersion)
. (Join-Path $PSScriptRoot 'release-common.ps1')
$expected = Get-ReleaseVersion ("v" + $ExpectedVersion)
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$dumpbin = Get-ChildItem -Path "$installation/VC/Tools/MSVC/*/bin/Hostx64/x64/dumpbin.exe" | Sort-Object FullName -Descending | Select-Object -First 1
if (!$dumpbin) { throw 'MSVC dumpbin not found' }
$raw = @(& $dumpbin.FullName /dependents $Executable)
if ($LASTEXITCODE) { throw 'dumpbin failed' }
$imports = @($raw | ForEach-Object { if ($_ -match '^\s+([A-Za-z0-9_.-]+\.dll)\s*$') { $Matches[1] } })
$allowed = '^(KERNEL32|USER32|GDI32|SHELL32|OLE32|COMCTL32|WTSAPI32|XINPUT9_1_0|ADVAPI32|COMDLG32|IMM32|OLEAUT32|VERSION|SHLWAPI|api-ms-win-.+|ext-ms-.+)\.dll$'
if (!$imports.Count) { throw 'No imports found' }
foreach ($dll in $imports) { if ($dll -notmatch $allowed) { throw "Unexpected dependency: $dll" } }
$headers = @(& $dumpbin.FullName /headers $Executable)
if ($LASTEXITCODE -or !(($headers -join "`n") -match '8664 machine')) { throw 'Expected x64 PE' }
$version = (Get-Item -LiteralPath $Executable).VersionInfo
if ($version.FileVersion -cne $expected.FileVersion -or $version.ProductVersion -cne $expected.Version) { throw 'Wrong executable version' }
$numericFile = @($version.FileMajorPart,$version.FileMinorPart,$version.FileBuildPart,$version.FilePrivatePart) -join '.'
$numericProduct = @($version.ProductMajorPart,$version.ProductMinorPart,$version.ProductBuildPart,$version.ProductPrivatePart) -join '.'
if ($numericFile -cne $expected.FileVersion -or $numericProduct -cne $expected.FileVersion -or $version.IsPreRelease -ne $expected.Prerelease) { throw 'Inconsistent numeric/prerelease resources' }
[ordered]@{ Imports = $imports; Machine = 'x64'; FileVersion = $version.FileVersion; ProductVersion = $version.ProductVersion; Signature = (Get-AuthenticodeSignature -LiteralPath $Executable).Status.ToString() }
