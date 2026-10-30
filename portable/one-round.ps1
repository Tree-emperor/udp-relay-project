$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$run = Join-Path $root ('runs\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $run -Force | Out-Null
foreach ($name in @('udp_sender.exe','udp_test.exe','udp_receiver.exe','run-guest.ps1')) {
    Copy-Item -LiteralPath (Join-Path $root $name) -Destination $run
}
Write-Host 'Running one round: 200 packets, all on this Windows computer.'
& (Join-Path $run 'run-guest.ps1') -StartRound 0 -EndRound 0
$values = @(Select-String -Path (Join-Path $run 'logs\round-00-relay.log') -Pattern 'local elapsed=([0-9.]+) ms' | ForEach-Object {
    [double]::Parse($_.Matches[0].Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture) * 1000
} | Sort-Object)
if ($values.Count -ne 200) { throw 'Expected 200 forwarded packets. Inspect logs.' }
$stats = $values | Measure-Object -Minimum -Maximum -Average
$counts = Import-Csv (Join-Path $run 'logs\validation.csv')
$text = @(
    'UDP relay local latency (recvfrom return -> sendto return)',
    ('Sent={0} Forwarded={1} Received={2}' -f $counts.Sent,$counts.Forwarded,$counts.Received),
    ('Samples={0} Min={1:F3} Mean={2:F3} P50={3:F3} P95={4:F3} P99={5:F3} Max={6:F3} us' -f $values.Count,$stats.Minimum,$stats.Average,$values[99],$values[189],$values[197],$stats.Maximum),
    ('Saved in: ' + $run)
)
$text | Set-Content -Encoding UTF8 (Join-Path $run 'summary.txt')
$text | ForEach-Object { Write-Host $_ }
if ($counts.Received -ne '200') { Write-Warning 'Receiver did not receive all 200 packets. See logs; do not assume zero loss.' }
# Results are saved and run-guest has stopped every child. Explicitly finish
# the dedicated PowerShell host; Win7 PowerShell 2 teardown can otherwise hang.
[Environment]::Exit(0)
