@echo off
cd /d "%~dp0"
echo Close the three manual test programs before running this automatic test.
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0one-round.ps1"
if errorlevel 1 echo TEST FAILED. Read the error above.
pause
