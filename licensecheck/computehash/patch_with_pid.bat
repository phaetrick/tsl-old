@echo off
setlocal EnableDelayedExpansion

set "TARGET=%~1"
set "PATCHER=%~2"

echo Target: %TARGET%
echo Patcher: %PATCHER%

if not exist "%TARGET%" (
    echo ERROR: Target file not found: %TARGET%
    exit /b 1
)

if not exist "%PATCHER%" (
    echo ERROR: Patcher file not found: %PATCHER%
    exit /b 1
)

echo Launching %TARGET%...
start "" "%TARGET%" --hash-capture-mode

echo Waiting for process to start...
timeout /t 3 /nobreak >nul

echo Getting process name...
for %%F in ("%TARGET%") do set "EXENAME=%%~nxF"
echo Looking for process: %EXENAME%

echo Getting PID...
set PID=
for /f "skip=3 tokens=2" %%a in ('tasklist /FI "IMAGENAME eq %EXENAME%" 2^>nul') do (
    set PID=%%a
    goto :found
)

echo ERROR: Could not find process
exit /b 1

:found
echo Found PID: %PID%
echo Running patcher...
"%PATCHER%" "%TARGET%" %PID%

echo Terminating process...
taskkill /PID %PID% /F >nul 2>&1

echo Done!
exit /b 0
