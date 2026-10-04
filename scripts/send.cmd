@echo off
rem Send a .3dsx to the 3DS over Wi-Fi and run it. Works from cmd and PowerShell.
rem On the 3DS, open the Homebrew Launcher and press Y first.
rem Usage: scripts\send.cmd path\to\app.3dsx [arguments for the app]
rem   e.g.  scripts\send.cmd build\openrct2\openrct2.3dsx --verbose
rem Optional: put N3DS_IP=x.x.x.x in local.env. Without it, 3dslink searches the network.
setlocal
set "ROOT=%~dp0.."
set "N3DS_IP="
if exist "%ROOT%\local.env" for /f "usebackq eol=# tokens=1,* delims==" %%a in ("%ROOT%\local.env") do if /i "%%a"=="N3DS_IP" set "N3DS_IP=%%b"
if "%~1"=="" (echo Usage: scripts\send.cmd path\to\app.3dsx [args] & exit /b 1)
set "APP=%~1"
rem Keep a copy of the matching .elf so a later crash dump can be symbolized (scriptsash_report.cmd)
if exist "%~dpn1.elf" (
  if not exist "%ROOT%\crashdumps\elf" mkdir "%ROOT%\crashdumps\elf"
  for /f %%t in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd-HHmmss"') do copy /y "%~dpn1.elf" "%ROOT%\crashdumps\elf\%~n1-%%t.elf" >nul
)
rem Everything after the .3dsx goes to the app. "--" keeps 3dslink from parsing it.
set "APPARGS="
:collect
shift
if "%~1"=="" goto send
set "APPARGS=%APPARGS% %1"
goto collect
:send
set "ADDR="
if defined N3DS_IP set "ADDR=-a %N3DS_IP%"
if defined APPARGS (
  "%ROOT%\tools\devkitpro\tools\bin\3dslink.exe" -s %ADDR% "%APP%" --%APPARGS%
) else (
  "%ROOT%\tools\devkitpro\tools\bin\3dslink.exe" -s %ADDR% "%APP%"
)
