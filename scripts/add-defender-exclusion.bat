@echo off
REM Quick Windows Defender exclusion setup for AIGuardian build folder
REM Requires Administrator privileges

echo Adding Windows Defender exclusion for build folder...
echo.

set BUILD_PATH=%~dp0..\build

powershell -Command "Start-Process powershell -Verb RunAs -ArgumentList '-NoExit', '-Command', 'Add-MpPreference -ExclusionPath \"%BUILD_PATH%\"; Write-Host \"Exclusion added successfully!\" -ForegroundColor Green; Write-Host \"Path: %BUILD_PATH%\" -ForegroundColor Cyan'"

echo.
echo If UAC prompt appeared, approve it to complete the exclusion.
echo After adding the exclusion, rebuild and run tests.
pause
