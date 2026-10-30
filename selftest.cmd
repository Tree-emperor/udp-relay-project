@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1"
if errorlevel 1 (
    echo Build failed.
    pause
    exit /b 1
)
call "%~dp0build\04_one_round.cmd"
endlocal
