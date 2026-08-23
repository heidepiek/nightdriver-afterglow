@echo off
setlocal enabledelayedexpansion

echo Voorbereiden van shared libraries...
python tools\setup_shared_libs_cli.py
if errorlevel 1 (
    echo Fout bij het voorbereiden van libraries. Build gestopt.
    pause
    exit /b 1
)

REM Als er een omgeving is meegegeven als parameter, gebruik die direct
if not "%~1"=="" (
    set ENVNAME=%~1
    goto :build
)

echo.
echo Beschikbare build-omgevingen:
echo ------------------------------
for /f "tokens=1 delims= " %%a in ('findstr /r "^\[env:" platformio.ini') do (
    set LINE=%%a
    set LINE=!LINE:[env:=!
    set LINE=!LINE:]=!
    echo   !LINE!
)
echo ------------------------------
echo.
set /p ENVNAME="Welke omgeving wil je bouwen? "

:build
echo.
echo Starten van de build voor omgeving: %ENVNAME%
pio run -e %ENVNAME%

pause