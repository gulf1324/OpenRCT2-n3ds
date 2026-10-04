@echo off
rem Pack the built game into a CIA for the 3DS HOME menu. See scripts\make_cia.py for the details.
rem Usage: scripts\make_cia.cmd [put] [cxi]     put: also upload it to /cias on the SD card (ftpd)
python "%~dp0make_cia.py" %*
