$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$culture = [Globalization.CultureInfo]::InvariantCulture
$checks = @(Import-Csv (Join-Path $root 'logs\validation.csv'))
if ($checks.Count -ne 11) { throw 'Expected a warmup plus 10 measured rounds.' }
foreach ($check in $checks) {
    if ($check.Sent -ne '200' -or $check.Forwarded -ne '200') { throw 'Forwarding count validation failed.' }
}
foreach ($err in Get-ChildItem (Join-Path $root 'logs') -Filter '*.err') {
    if ($err.Length -ne 0) { throw "Nonempty stderr: $($err.Name)" }
}
function Get-Stats($values) {
    $sorted = @($values | Sort-Object)
    $m = $sorted | Measure-Object -Minimum -Maximum -Average
    $sum = [decimal]0
    foreach ($value in $sorted) { $sum += [decimal]$value }
    [pscustomobject]@{
        count = $sorted.Count
        min_us = $m.Minimum
        mean_us = [decimal]::Round($sum / $sorted.Count, 4, [MidpointRounding]::AwayFromZero)
        p50_us = $sorted[[int][Math]::Ceiling(0.50 * $sorted.Count) - 1]
        p95_us = $sorted[[int][Math]::Ceiling(0.95 * $sorted.Count) - 1]
        p99_us = $sorted[[int][Math]::Ceiling(0.99 * $sorted.Count) - 1]
        max_us = $m.Maximum
        at_least_1ms = @($sorted | Where-Object { $_ -ge 1000 }).Count
    }
}
$samples = @()
$rounds = @()
foreach ($round in 1..10) {
    $path = Join-Path $root ('logs\round-{0:00}-relay.log' -f $round)
    $lines = Get-Content -LiteralPath $path
    if ($lines[0] -ne 'Timer=QueryPerformanceCounter; frequency=10000000 Hz') { throw "Unexpected clock in $path" }
    $index = 0
    $roundValues = @()
    foreach ($line in $lines) {
        if ($line -match '^Forwarded (\d+) bytes .*local elapsed=([0-9.]+) ms$') {
            $us = [Math]::Round([double]::Parse($matches[2],$culture) * 1000, 3)
            $index++
            $samples += [pscustomobject]@{ round=$round; packet=$index; bytes=[int]$matches[1]; latency_us=$us }
            $roundValues += $us
        }
    }
    if ($index -ne 200) { throw "Expected 200 samples in $path" }
    $rounds += [pscustomobject]@{ round=$round; stats=(Get-Stats $roundValues) }
}
$summary = [pscustomobject]@{
    date='2026-10-30'; timezone='Asia/Shanghai'; os='Windows 7 Professional SP1 x64 (6.1.7601)'
    metric='recvfrom return to sendto return; same guest loopback; QPC'
    unit='us'; frequency_hz=10000000; warmup_rounds_excluded=1; measured_rounds=10
    quantile_method='nearest rank: sorted[ceil(p*N)-1]'; outliers_removed=0
    sent_packets=2000; forwarded_packets=2000
    received_packets=($checks | Where-Object { [int]$_.Round -gt 0 } | Measure-Object -Property Received -Sum).Sum
    overall=(Get-Stats @($samples | ForEach-Object { $_.latency_us })); rounds=$rounds
}
$samples | Export-Csv -NoTypeInformation -Encoding UTF8 (Join-Path $root 'samples.csv')
$summary | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 (Join-Path $root 'summary.json')
$summary.overall | Format-List
$rounds | ForEach-Object { [pscustomobject]@{round=$_.round; min_us=$_.stats.min_us; mean_us=$_.stats.mean_us; p50_us=$_.stats.p50_us; p95_us=$_.stats.p95_us; p99_us=$_.stats.p99_us; max_us=$_.stats.max_us} } | Format-Table
