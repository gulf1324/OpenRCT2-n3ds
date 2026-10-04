@echo off
rem Installs OpenRCT2 for New 3DS: asks where your RollerCoaster Tycoon 2 is and copies everything to the 3DS.
rem See README.md. Needs Python 3 (python.org; tick "Add python.exe to PATH" when installing).
setlocal
where py >nul 2>nul && (py -3 "%~dp0scripts\install.py" %* & goto done)
where python >nul 2>nul && (python "%~dp0scripts\install.py" %* & goto done)
echo Python 3 is not installed. Get it from https://www.python.org/downloads/ and run this again.
:done
echo.
pause
