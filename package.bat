@echo off
setlocal
cd /d "%~dp0"

echo ========================================================
echo Packaging alpha2gogModernPatch.zip
echo ========================================================

set "ZIPNAME=alpha2gogModernPatch.zip"

if exist "%ZIPNAME%" (
    echo Removing existing archive...
    del "%ZIPNAME%"
)

echo Verifying required files...
set "MISSING=0"
for %%F in (ddraw.dll DDrawCompat.ini dinput.dll sfa2_patch.ini LEIAME.txt) do (
    if not exist "%%F" (
        echo [ERROR] Missing required file: %%F
        set "MISSING=1"
    )
)

if "%MISSING%"=="1" (
    echo.
    echo Build failed: Required files are missing!
    pause
    exit /b 1
)

echo Creating %ZIPNAME%...
powershell -NoProfile -Command "Compress-Archive -Path ddraw.dll, DDrawCompat.ini, dinput.dll, sfa2_patch.ini, LEIAME.txt -DestinationPath '%ZIPNAME%' -Force"

if exist "%ZIPNAME%" (
    echo [SUCCESS] Package created successfully: %ZIPNAME%
    echo Archive contents:
    powershell -NoProfile -Command "tar -tf '%ZIPNAME%'"
) else (
    echo [ERROR] Failed to create ZIP archive.
)

echo.
