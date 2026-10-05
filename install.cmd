@echo off
rem Installs OpenRCT2 for New 3DS: asks where your RollerCoaster Tycoon 2 is and copies everything to the 3DS.
rem See the README. The download of a release brings its own Python (the python folder beside this file);
rem a copy of the repository uses the Python 3 of this PC.
setlocal
if not exist "%~dp0scripts\install.py" goto notunpacked
if exist "%~dp0python\python.exe" goto bundled
rem "py" and "python" are tried by running them, not by looking for them: on a Windows without Python,
rem "python" is a shortcut to the Microsoft Store that is found and only prints a message. With "call":
rem where "python" is a batch file (pyenv-win), this one would end with it otherwise.
call py -3 -c "pass" >nul 2>nul
if not errorlevel 1 goto launcher
call python -c "import sys; sys.exit(sys.version_info[0] != 3)" >nul 2>nul
if not errorlevel 1 goto onpath
echo Python 3 was not found on this PC.
echo.
echo The download of a release needs none: get OpenRCT2-n3ds.zip from
echo   https://github.com/gulf1324/OpenRCT2-n3ds/releases/latest
echo unpack it, and run the install.cmd in it.
echo Or install Python 3 from https://www.python.org/downloads/ (tick "Add python.exe to PATH")
echo and run this again.
goto done

:notunpacked
echo The other files of the download are not beside this one.
echo.
echo Unpack the whole ZIP first (right-click the ZIP, "Extract All..."),
echo then run install.cmd in the unpacked folder.
goto done

:bundled
"%~dp0python\python.exe" "%~dp0scripts\install.py" %*
goto done

:launcher
call py -3 "%~dp0scripts\install.py" %*
goto done

:onpath
call python "%~dp0scripts\install.py" %*

:done
echo.
pause
