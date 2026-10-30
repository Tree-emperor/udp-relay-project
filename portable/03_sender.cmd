@echo off
cd /d "%~dp0"
echo Sender: sending 200 packets to the local relay.
udp_sender.exe
pause
