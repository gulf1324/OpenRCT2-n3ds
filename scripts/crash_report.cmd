@echo off
rem Luma3DS crash dump analyzer. Usage: scripts\crash_report.cmd crash_dump_XXXXXXXX.dmp path\to\app.elf
python "%~dp0crash_report.py" %*
