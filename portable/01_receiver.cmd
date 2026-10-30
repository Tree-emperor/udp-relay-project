@echo off
cd /d "%~dp0"
echo Receiver: wait for "Listening on UDP 9001", then start 02_relay.cmd.
echo Only use this window to check packet counts, not relay latency.
udp_receiver.exe
pause
