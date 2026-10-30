@echo off
cd /d "%~dp0"
echo Relay: wait for "Timer=QueryPerformanceCounter" and "Listening on UDP 9000".
echo Then start 03_sender.cmd. Press Ctrl+C here when finished.
echo local elapsed is in ms. Multiply by 1000 for microseconds.
udp_test.exe
pause
