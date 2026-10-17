param([Parameter(Mandatory=$true)][int]$ProcessId, [ValidateRange(1,300)][int]$Seconds = 10)
# Read-only measurement of an already running ZeroPad; this never launches it.
$sample = Get-Process -Id $ProcessId -ErrorAction Stop
$binary = $sample.MainModule.FileName
$cpuBefore = $sample.TotalProcessorTime.TotalMilliseconds
$timer = [Diagnostics.Stopwatch]::StartNew()
Start-Sleep -Seconds $Seconds
$sample.Refresh()
$elapsed = $timer.Elapsed.TotalSeconds
[pscustomobject]@{
    ProcessId = $ProcessId
    Seconds = $elapsed
    ExecutableBytes = (Get-Item -LiteralPath $binary).Length
    WorkingSetBytes = $sample.WorkingSet64
    PrivateCommittedBytes = $sample.PrivateMemorySize64
    CpuPercentOfOneLogicalProcessor = ($sample.TotalProcessorTime.TotalMilliseconds - $cpuBefore) / ($elapsed * 10)
    LogicalProcessors = [Environment]::ProcessorCount
}
