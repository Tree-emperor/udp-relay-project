param([int]$StartRound = 0, [int]$EndRound = 10, [switch]$RequireWindows7)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root
New-Item -ItemType Directory -Path 'logs' -Force | Out-Null
$os = Get-WmiObject Win32_OperatingSystem
$cpu = Get-WmiObject Win32_Processor | Select-Object -First 1
@(
    ('OS=' + $os.Caption), ('Version=' + $os.Version),
    ('ServicePack=' + $os.ServicePackMajorVersion), ('Architecture=' + $os.OSArchitecture),
    ('CPU=' + $cpu.Name), ('LogicalProcessors=' + $cpu.NumberOfLogicalProcessors),
    ('VisibleMemoryKB=' + $os.TotalVisibleMemorySize),
    ('StopwatchFrequency=' + [System.Diagnostics.Stopwatch]::Frequency),
    ('StopwatchIsHighResolution=' + [System.Diagnostics.Stopwatch]::IsHighResolution),
    ('GuestTime=' + (Get-Date -Format 'yyyy-MM-ddTHH:mm:sszzz'))
) | Set-Content -Encoding UTF8 'logs\environment.txt'
if ($RequireWindows7 -and $os.Version -notlike '6.1.*') { throw 'This benchmark must run inside Windows 7.' }
if (Get-Process udp_sender,udp_test,udp_receiver -ErrorAction SilentlyContinue) {
    throw 'Existing UDP demo process found. Stop it before running this benchmark.'
}
$rows = @()
if ($StartRound -gt 0) { $rows = @(Import-Csv 'logs\validation.csv') }
# Use .NET directly so guest-control sessions do not depend on an interactive shell.
function Start-BenchProcess([string]$name, [string]$output, [string]$errors) {
    $info = New-Object System.Diagnostics.ProcessStartInfo
    $info.FileName = "$env:SystemRoot\System32\cmd.exe"
    $info.Arguments = '/d /c ""' + $root + '\' + $name + '.exe" > "' + $output + '" 2> "' + $errors + '""'
    $info.WorkingDirectory = $root
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    return [System.Diagnostics.Process]::Start($info)
}
# Round 0 warms the binaries and filesystem; rounds 1-10 are retained in statistics.
for ($round = $StartRound; $round -le $EndRound; $round++) {
    $tag = 'round-{0:00}' -f $round
    $relay = $null
    $receiver = $null
    $sender = $null
    try {
        $receiver = Start-BenchProcess 'udp_receiver' "$root\$tag-receiver.tmp" "$root\logs\$tag-receiver.err"
        $relay = Start-BenchProcess 'udp_test' "$root\logs\$tag-relay.log" "$root\logs\$tag-relay.err"
        Start-Sleep -Milliseconds 1000
        if ($receiver.HasExited -or $relay.HasExited) { throw 'Receiver or relay exited before sending.' }
        $sender = Start-BenchProcess 'udp_sender' "$root\$tag-sender.tmp" "$root\logs\$tag-sender.err"
        if (-not $sender.WaitForExit(15000)) { throw 'Sender timeout.' }
        if (-not $receiver.WaitForExit(15000)) { throw 'Receiver timeout.' }
        Start-Sleep -Milliseconds 200
        $sender.Refresh()
        $receiver.Refresh()
        $sentCount = @(Select-String -Path "$root\$tag-sender.tmp" -Pattern '^Sent \d+/200 ').Count
        $forwardedCount = @(Select-String -Path "$root\logs\$tag-relay.log" -Pattern 'local elapsed=').Count
        $receiverText = Get-Content "$root\$tag-receiver.tmp" | Out-String
        $receivedCount = 0
        if ($receiverText -match 'ONE-WAY latency \(send -> receive\) \((\d+) samples\)') { $receivedCount = [int]$matches[1] }
        $ok = $sentCount -eq 200 -and $forwardedCount -eq 200 -and $receivedCount -eq 200 -and -not $relay.HasExited
        foreach ($errFile in Get-ChildItem "$root\logs\$tag-*.err") {
            if ($errFile.Length -ne 0) { $ok = $false }
        }
        $rows += New-Object PSObject -Property @{ Round=$round; Sent=$sentCount; Forwarded=$forwardedCount; Received=$receivedCount; Passed=$ok }
        $rows | Select-Object Round,Sent,Forwarded,Received,Passed | Export-Csv -NoTypeInformation -Path 'logs\validation.csv'
        Write-Output "$tag sent=$sentCount forwarded=$forwardedCount received=$receivedCount passed=$ok"
        if ($sentCount -ne 200 -or $forwardedCount -ne 200 -or $relay.HasExited) { throw "Incomplete forwarding round: $tag" }
    } finally {
        foreach ($proc in @($sender,$receiver,$relay)) {
            if ($null -ne $proc -and -not $proc.HasExited) { & "$env:SystemRoot\System32\taskkill.exe" /PID $proc.Id /T /F | Out-Null }
            if ($null -ne $proc) { $proc.Dispose() }
        }
    }
    Start-Sleep -Milliseconds 300
}
Write-Output 'Benchmark completed; all test processes stopped.'
