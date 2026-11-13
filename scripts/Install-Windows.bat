@echo off
REM Messy Hat Tricks - Windows installer
REM
REM Right-click this file and choose "Run as administrator": the shared VST3 and
REM CLAP folders live under Program Files, which needs elevation to write to.

setlocal
cd /d "%~dp0"

echo.
echo   MESSY HAT TRICKS - installer
echo   ===========================
echo.

net session >nul 2>&1
if %errorlevel% neq 0 (
    echo   This needs administrator rights.
    echo   Close this window, right-click Install-Windows.bat
    echo   and choose "Run as administrator".
    echo.
    pause
    exit /b 1
)

set "VST3=%CommonProgramFiles%\VST3"
set "CLAP=%CommonProgramFiles%\CLAP"

if not exist "%VST3%" mkdir "%VST3%"
if not exist "%CLAP%" mkdir "%CLAP%"

set COUNT=0

for %%F in (*.vst3) do (
    if exist "%VST3%\%%~nxF" rmdir /s /q "%VST3%\%%~nxF" 2>nul
    xcopy /e /i /y /q "%%F" "%VST3%\%%~nxF\" >nul && (
        echo   [ok] VST3        -^> %VST3%
        set /a COUNT+=1
    )
)

for %%F in (*.clap) do (
    copy /y "%%F" "%CLAP%\" >nul && (
        echo   [ok] CLAP        -^> %CLAP%
        set /a COUNT+=1
    )
)

for %%F in (*.exe) do (
    echo   [--] Standalone  -^> keep "%%F" wherever you like
)

echo.
if %COUNT%==0 (
    echo   Nothing to install - keep this script in the same folder
    echo   as the .vst3 and .clap files.
) else (
    echo   Done. Open your DAW and rescan your plug-in folders.
    echo   Bitwig does not rescan on its own: Settings - Locations - rescan.
)
echo.
echo   The first time you open the standalone, Windows SmartScreen may warn
echo   that the publisher is unknown. Click "More info" then "Run anyway".
echo.
pause
