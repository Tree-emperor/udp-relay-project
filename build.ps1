param(
    [ValidateSet('x86', 'x64')][string]$Architecture = 'x64',
    [string]$OutputDirectory = (Join-Path $PSScriptRoot 'build')
)
$ErrorActionPreference = 'Stop'

$compiler = Join-Path $PSScriptRoot '.tools\w64devkit\bin\g++.exe'
if (-not (Test-Path -LiteralPath $compiler)) {
    throw 'Missing compiler: extract w64devkit into .tools\w64devkit first. See the project instructions.'
}

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$archFlag = if ($Architecture -eq 'x86') { '-m32' } else { '-m64' }
foreach ($name in @('udp_sender', 'udp_test', 'udp_receiver')) {
    $source = Join-Path $PSScriptRoot ($name + '.cpp')
    $output = Join-Path $OutputDirectory ($name + '.exe')
    Write-Host "Building $name ..."
    & $compiler $archFlag -std=c++11 -Wall -Wextra -Wpedantic -O2 -s -static $source -o $output -lws2_32 `
        '-Wl,--major-os-version,6,--minor-os-version,1,--major-subsystem-version,6,--minor-subsystem-version,1'
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed: $name. Stop any running demo processes before rebuilding."
    }
}
foreach ($file in Get-ChildItem (Join-Path $PSScriptRoot 'portable') -File) {
    $content = Get-Content -LiteralPath $file.FullName -Raw -Encoding UTF8
    $content = $content.Replace('{{ARCH}}', $Architecture) -replace '\r?\n', "`r`n"
    $encoding = if ($file.Extension -eq '.txt') { New-Object System.Text.UTF8Encoding($true) } else { [Text.Encoding]::ASCII }
    [IO.File]::WriteAllText((Join-Path $OutputDirectory $file.Name), $content, $encoding)
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot '.tools\w64devkit\COPYING.MinGW-w64-runtime.txt') -Destination $OutputDirectory -Force
Write-Host "Build succeeded: $OutputDirectory ($Architecture). Run 04_one_round.cmd."
