@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

REM ============================================================
REM  ATGraphics Build & Installer Script
REM  Builds Release, deploys Qt libs, generates ATHC_Setup.exe
REM ============================================================
REM
REM  Usage:
REM    Installer.bat              Full build (default)
REM    Installer.bat /?           Show this help
REM    Installer.bat /h           Show this help
REM    Installer.bat /help        Show this help
REM    Installer.bat /dry-run     Verify paths/tools only, no build
REM
REM  Prerequisites:
REM    - Visual Studio 2022 Community (C++ Desktop workload)
REM    - Qt 6.7.3 + Qt Installer Framework 4.11
REM    - ATHC installer project (D:\WorkSpace\Caviar\ATHC\installer\)
REM
REM  Build steps:
REM    1. Pre-check all tool paths
REM    2. Initialize MSVC build environment
REM    3. qmake (skip if Makefile is up-to-date)
REM    4. nmake Release build
REM    5. Verify build artifacts
REM    6. Sync files to installer data dir (incremental)
REM    7. windeployqt (skip if Qt DLLs already up-to-date)
REM    8. binarycreator package into ATHC_Setup.exe
REM
REM  Output:
REM    D:\WorkSpace\Caviar\ATHC\ATHC_Setup.exe
REM
REM ============================================================

REM ---- Check help flags ----
if /i "%~1"=="/?"   goto :show_help
if /i "%~1"=="/h"   goto :show_help
if /i "%~1"=="/help" goto :show_help
if /i "%~1"=="/dry-run" goto :dry_run

REM ============================================================
REM ---- Path configuration ----
REM ============================================================
set "PROJECT_DIR=D:\WorkSpace\Caviar\ATGraphics"
set "QT_DIR=D:\Qt\Qt17.0.0\6.7.3\msvc2022_64"
set "IFW_DIR=D:\Qt\Qt17.0.0\Tools\QtInstallerFramework\4.11"
set "ATHC_DIR=D:\WorkSpace\Caviar\ATHC"
set "FILERIP_DIR=D:\WorkSpace\Caviar\FileRip"
set "INSTALL_DATA_DIR=%ATHC_DIR%\installer\packages\com.caviar.atgraphics\data\ATGraphics"
set "INSTALLER_CONFIG=%ATHC_DIR%\installer\config\config.xml"
set "INSTALLER_PACKAGES=%ATHC_DIR%\installer\packages"
set "SETUP_OUTPUT=%ATHC_DIR%\ATHC_Setup.exe"

set "QMAKE=%QT_DIR%\bin\qmake.exe"
set "WINDEPLOYQT=%QT_DIR%\bin\windeployqt.exe"
set "BINARYCREATOR=%IFW_DIR%\bin\binarycreator.exe"
set "BIN_RELEASE_DIR=%PROJECT_DIR%\Bin\Release"
set "BUILD_RELEASE_DIR=%PROJECT_DIR%\Build\Release"

set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

REM ============================================================
echo ============================================================
echo  Step 1: Pre-check all tool paths
echo ============================================================
set "ALL_OK=1"
if not exist "%VCVARS%"       ( echo [ERROR] vcvars64.bat missing:   %VCVARS%       & set "ALL_OK=0" )
if not exist "%QMAKE%"        ( echo [ERROR] qmake missing:          %QMAKE%        & set "ALL_OK=0" )
if not exist "%WINDEPLOYQT%"  ( echo [ERROR] windeployqt missing:    %WINDEPLOYQT%  & set "ALL_OK=0" )
if not exist "%BINARYCREATOR%" ( echo [ERROR] binarycreator missing:  %BINARYCREATOR% & set "ALL_OK=0" )
if not exist "%INSTALLER_CONFIG%" ( echo [ERROR] config.xml missing: %INSTALLER_CONFIG% & set "ALL_OK=0" )
if "%ALL_OK%"=="0" ( echo. & echo [FAIL] One or more tools/paths missing. Aborted. & exit /b 1 )
echo [PASS] All tool paths verified.

echo.
echo ============================================================
echo  Step 2: Initialize MSVC build environment
echo ============================================================
call "%VCVARS%" x64
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to initialize MSVC environment
    exit /b 1
)
echo [PASS] MSVC environment initialized.

echo.
echo ============================================================
echo  Step 3: Run qmake (Release) -- skip if Makefile is up-to-date
echo ============================================================
pushd "%PROJECT_DIR%"
set "RUN_QMAKE=1"
if exist "%BUILD_RELEASE_DIR%\Makefile" (
    for %%F in ("%BUILD_RELEASE_DIR%\Makefile") do set "MF_TIME=%%~tF"
    for %%F in ("ATGraphics.pro") do set "PRO_TIME=%%~tF"
    if "!MF_TIME!" geq "!PRO_TIME!" (
        echo Makefile is up-to-date, skipping qmake.
        set "RUN_QMAKE=0"
    )
)
if "!RUN_QMAKE!"=="1" (
    "%QMAKE%" -r -spec win32-msvc "CONFIG+=release" ATGraphics.pro
    if !ERRORLEVEL! neq 0 (
        popd
        echo [ERROR] qmake failed
        exit /b 1
    )
    echo [PASS] qmake completed.
)

echo.
echo ============================================================
echo  Step 4: Build Release
echo ============================================================
nmake release
if %ERRORLEVEL% neq 0 (
    popd
    echo [ERROR] Build failed
    exit /b 1
)
popd
echo [PASS] Build completed.

echo.
echo ============================================================
echo  Step 5: Verify build artifacts
echo ============================================================
if not exist "%BIN_RELEASE_DIR%\ATGraphics.exe" (
    echo [ERROR] ATGraphics.exe not found: %BIN_RELEASE_DIR%
    exit /b 1
)
echo [PASS] ATGraphics.exe found.

echo.
echo ============================================================
echo  Step 6: Sync app files to installer data dir (incremental)
echo ============================================================
if not exist "%INSTALL_DATA_DIR%" mkdir "%INSTALL_DATA_DIR%"
echo Syncing from %BIN_RELEASE_DIR% to %INSTALL_DATA_DIR%

rem --- Main executable ---
copy /y "%BIN_RELEASE_DIR%\ATGraphics.exe" "%INSTALL_DATA_DIR%\" >nul

rem --- Project DLLs and third-party DLLs (batch copy, skip missing) ---
for %%D in (ColorTrans.dll Layout.dll QSimpleUpdater.dll QtColorWidgets.dll QtGradientEditor.dll lcms2.dll tiff.dll opencv_world4100.dll ExportEngine.dll) do (
    if exist "%BIN_RELEASE_DIR%\%%D" copy /y "%BIN_RELEASE_DIR%\%%D" "%INSTALL_DATA_DIR%\" >nul
)

rem --- Config and resource files ---
for %%F in (config.xml config.ini main.txt) do (
    if exist "%BIN_RELEASE_DIR%\%%F" copy /y "%BIN_RELEASE_DIR%\%%F" "%INSTALL_DATA_DIR%\" >nul
)

rem --- ICC Profile directory (use robocopy for speed, skip unchanged) ---
if exist "%BIN_RELEASE_DIR%\ICC Profile" (
    robocopy "%BIN_RELEASE_DIR%\ICC Profile" "%INSTALL_DATA_DIR%\ICC Profile" /e /njh /njs /ndl /np /xc >nul
    if ERRORLEVEL 8 ( echo [WARN] robocopy ICC Profile error ) else ( echo   ICC Profile synced. )
)

rem --- Sync FileRip files ---
echo.
echo   Syncing FileRip files from %FILERIP_DIR%
if exist "%FILERIP_DIR%" (
    if exist "%FILERIP_DIR%\FileRIP.exe" (
        copy /y "%FILERIP_DIR%\FileRIP.exe" "%INSTALL_DATA_DIR%\" >nul
        echo     FileRIP.exe copied.
    ) else (
        echo     [WARN] FileRIP.exe not found
    )
    for %%F in (config.ini ripconfig.xml 2.bin 360X1200.p FMX.p) do (
        if exist "%FILERIP_DIR%\%%F" copy /y "%FILERIP_DIR%\%%F" "%INSTALL_DATA_DIR%\" >nul
    )
    echo     FileRip config files copied.
    for %%D in (iconengines imageformats platforms styles) do (
        if exist "%FILERIP_DIR%\%%D" (
            robocopy "%FILERIP_DIR%\%%D" "%INSTALL_DATA_DIR%\%%D" /e /njh /njs /ndl /np /xc >nul
            if ERRORLEVEL 8 ( echo     [WARN] robocopy %%D error ) else ( echo     FileRip %%D\ synced. )
        )
    )
) else (
    echo   [WARN] FileRip directory not found: %FILERIP_DIR%
)

echo [PASS] Application files synced.

echo.
echo ============================================================
echo  Step 7: Run windeployqt (skip if Qt DLLs already up-to-date)
echo ============================================================
set "SKIP_DEPLOY=0"
if exist "%INSTALL_DATA_DIR%\Qt6Core.dll" (
    for %%F in ("%INSTALL_DATA_DIR%\ATGraphics.exe") do set "EXE_TIME=%%~tF"
    for %%F in ("%INSTALL_DATA_DIR%\Qt6Core.dll")  do set "QT_TIME=%%~tF"
    if "!QT_TIME!" geq "!EXE_TIME!" (
        echo Qt DLLs already up-to-date, skipping windeployqt.
        set "SKIP_DEPLOY=1"
    )
)
if "!SKIP_DEPLOY!"=="0" (
    echo Running windeployqt on %INSTALL_DATA_DIR%\ATGraphics.exe...
    "%WINDEPLOYQT%" "%INSTALL_DATA_DIR%\ATGraphics.exe" --no-translations --no-compiler-runtime
    if !ERRORLEVEL! neq 0 (
        echo [WARN] windeployqt returned non-zero: !ERRORLEVEL!
    )
    echo [PASS] windeployqt completed.
)

echo.
echo ============================================================
echo  Step 8: Run binarycreator to package installer
echo ============================================================
echo Generating installer: %SETUP_OUTPUT%
"%BINARYCREATOR%" --offline-only -c "%INSTALLER_CONFIG%" -p "%INSTALLER_PACKAGES%" "%SETUP_OUTPUT%"
if %ERRORLEVEL% neq 0 (
    echo [ERROR] binarycreator packaging failed
    exit /b 1
)
echo [PASS] Installer generated: %SETUP_OUTPUT%

echo.
echo ============================================================
echo  Done! Installer: %SETUP_OUTPUT%
echo ============================================================

endlocal
exit /b 0

REM ============================================================
REM ---- Help ----
REM ============================================================
:show_help
echo.
echo ============================================================
echo  ATGraphics Build ^& Installer Script
echo ============================================================
echo.
echo  Usage:
echo    Installer.bat              Full build (default)
echo    Installer.bat /?  /h  /help Show this help
echo    Installer.bat /dry-run     Verify paths/tools only, no build
echo.
echo  Prerequisites:
echo    - Visual Studio 2022 Community (C++ Desktop workload)
echo    - Qt 6.7.3 (path: %QT_DIR%)
echo    - Qt Installer Framework 4.11 (path: %IFW_DIR%)
echo    - ATHC installer project (path: %ATHC_DIR%\installer\)
echo.
echo  Build steps (8 total):
echo    1. Pre-check all tool paths
echo    2. Initialize MSVC build environment
echo    3. qmake (skip if Makefile is up-to-date)
echo    4. nmake Release build
echo    5. Verify build artifacts
echo    6. Sync files to installer data dir (incremental)
echo    7. windeployqt (skip if Qt DLLs already up-to-date)
echo    8. binarycreator package into ATHC_Setup.exe
echo.
echo  Output:
echo    D:\WorkSpace\Caviar\ATHC\ATHC_Setup.exe
echo.
echo  Notes:
echo    - This script does NOT clear the installer data directory
echo      (%INSTALL_DATA_DIR%). It only overwrites same-name files.
echo      For a clean build, delete that directory manually first.
echo    - If MSVC initialization fails, verify that Visual Studio 2022
echo      Community is installed and paths match.
echo.
exit /b 0

REM ============================================================
REM ---- Dry-run (verify paths only, no build) ----
REM ============================================================
:dry_run
echo.
echo ============================================================
echo  Dry-run mode: verify paths and tools only, no actual build
echo ============================================================
set "ALL_OK=1"
echo.
echo  [1] vcvars64.bat ............. %VCVARS%
if exist "%VCVARS%"       ( echo      Status: OK ) else ( echo      Status: MISSING & set "ALL_OK=0" )
echo  [2] qmake.exe ................ %QMAKE%
if exist "%QMAKE%"        ( echo      Status: OK ) else ( echo      Status: MISSING & set "ALL_OK=0" )
echo  [3] windeployqt.exe .......... %WINDEPLOYQT%
if exist "%WINDEPLOYQT%"  ( echo      Status: OK ) else ( echo      Status: MISSING & set "ALL_OK=0" )
echo  [4] binarycreator.exe ........ %BINARYCREATOR%
if exist "%BINARYCREATOR%" ( echo      Status: OK ) else ( echo      Status: MISSING & set "ALL_OK=0" )
echo  [5] config.xml ............... %INSTALLER_CONFIG%
if exist "%INSTALLER_CONFIG%" ( echo      Status: OK ) else ( echo      Status: MISSING & set "ALL_OK=0" )
echo  [6] Project dir .............. %PROJECT_DIR%
if exist "%PROJECT_DIR%"  ( echo      Status: OK ) else ( echo      Status: MISSING & set "ALL_OK=0" )
echo  [7] ATGraphics.pro ........... %PROJECT_DIR%\ATGraphics.pro
if exist "%PROJECT_DIR%\ATGraphics.pro" ( echo      Status: OK ) else ( echo      Status: MISSING & set "ALL_OK=0" )
echo  [8] Bin\Release .............. %BIN_RELEASE_DIR%
if exist "%BIN_RELEASE_DIR%" ( echo      Status: OK ) else ( echo      Status: N/A (created after build) )
echo  [9] ATHC installer project ... %ATHC_DIR%
if exist "%ATHC_DIR%"     ( echo      Status: OK ) else ( echo      Status: MISSING & set "ALL_OK=0" )
echo  [10] Installer data dir ....... %INSTALL_DATA_DIR%
if exist "%INSTALL_DATA_DIR%" ( echo      Status: OK ) else ( echo      Status: N/A (created on first run) )
echo.
if "%ALL_OK%"=="0" (
    echo [FAIL] Some paths/tools are missing. Fix the configuration and retry.
    exit /b 1
)
echo [PASS] All required paths and tools are available. Ready to build.
echo.
exit /b 0
