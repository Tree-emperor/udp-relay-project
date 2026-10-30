param([ValidateSet('x86', 'x64', 'all')][string]$Architecture = 'all')
$ErrorActionPreference = 'Stop'
$targets = if ($Architecture -eq 'all') { @('x86','x64') } else { @($Architecture) }
foreach ($target in $targets) {
    $directory = Join-Path $PSScriptRoot ('dist\udp_demo_win_' + $target)
    & (Join-Path $PSScriptRoot 'build.ps1') -Architecture $target -OutputDirectory $directory
    if (-not $?) { throw "Build failed: $target" }
    Compress-Archive -Path (Join-Path $directory '*') -DestinationPath ($directory + '.zip') -Force
    Write-Host "Created $directory.zip"
}
