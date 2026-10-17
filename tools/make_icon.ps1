# Regenerate the simple native controller icon. No image libraries required.
$iconSizes = @(16, 20, 24, 32, 48, 64)
$images = @()
foreach ($size in $iconSizes) {
    $stream = [IO.MemoryStream]::new()
    $writer = [IO.BinaryWriter]::new($stream)
    $writer.Write([int]40); $writer.Write([int]$size); $writer.Write([int]($size * 2))
    $writer.Write([int16]1); $writer.Write([int16]32)
    $writer.Write([int]0); $writer.Write([int]($size * $size * 4))
    foreach ($unused in 1..4) { $writer.Write([int]0) }
    $stride = [int]([Math]::Ceiling($size / 32.0) * 4)
    $mask = [byte[]]::new($stride * $size)
    for ($y = $size - 1; $y -ge 0; --$y) {
        for ($x = 0; $x -lt $size; ++$x) {
            $nx = ($x + 0.5) / $size; $ny = ($y + 0.5) / $size
            $body = $nx -ge .08 -and $nx -le .92 -and $ny -ge .22 -and $ny -le .80
            if (($nx -lt .18 -or $nx -gt .82) -and ($ny -lt .32 -or $ny -gt .70)) { $body = $false }
            $cross = (($nx -ge .23 -and $nx -le .43 -and $ny -ge .45 -and $ny -le .55) -or ($nx -ge .28 -and $nx -le .38 -and $ny -ge .35 -and $ny -le .65))
            $dot = ([Math]::Pow($nx - .67, 2) + [Math]::Pow($ny - .43, 2) -lt .004) -or ([Math]::Pow($nx - .79, 2) + [Math]::Pow($ny - .57, 2) -lt .004)
            if (-not $body) {
                $writer.Write([uint32]0)
                $offset = ($size - 1 - $y) * $stride + [int][Math]::Floor($x / 8)
                $mask[$offset] = $mask[$offset] -bor (1 -shl (7 - ($x % 8)))
            } elseif ($cross -or $dot) { $writer.Write([uint32]4294967295) }
            else { $writer.Write([uint32]4281152353) }
        }
    }
    $writer.Write($mask)
    $images += ,$stream.ToArray()
    $writer.Dispose(); $stream.Dispose()
}
$outputPath = Join-Path $PSScriptRoot '../resources/zeropad.ico'
$writer = [IO.BinaryWriter]::new([IO.File]::Create($outputPath))
$writer.Write([int16]0); $writer.Write([int16]1); $writer.Write([int16]$iconSizes.Length)
$offset = 6 + 16 * $iconSizes.Length
for ($i = 0; $i -lt $iconSizes.Length; ++$i) {
    $writer.Write([byte]$iconSizes[$i]); $writer.Write([byte]$iconSizes[$i])
    $writer.Write([int16]0); $writer.Write([int16]1); $writer.Write([int16]32)
    $writer.Write([int]$images[$i].Length); $writer.Write([int]$offset)
    $offset += $images[$i].Length
}
foreach ($bytes in $images) { $writer.Write([byte[]]$bytes) }
$writer.Dispose()
