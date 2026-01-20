@echo off
REM ============================================================================
REM Lumen Windows Installer Build Script
REM Target: Windows 11+
REM Requires: Visual Studio 2019+, Qt 6.5+, NSIS
REM ============================================================================

setlocal enabledelayedexpansion

echo === Lumen Windows Installer Builder ===

set PROJECT_DIR=%~dp0..
set BUILD_DIR=%PROJECT_DIR%\build-release
set INSTALL_DIR=%BUILD_DIR%\install

REM Check for required tools
where cmake >nul 2>&1 || (
    echo Error: cmake not found in PATH
    exit /b 1
)

where windeployqt >nul 2>&1 || (
    echo Error: windeployqt not found in PATH
    echo Please ensure Qt bin directory is in PATH
    exit /b 1
)

REM Check for NSIS
set NSIS_PATH=
if exist "C:\Program Files (x86)\NSIS\makensis.exe" (
    set NSIS_PATH=C:\Program Files (x86)\NSIS\makensis.exe
) else if exist "C:\Program Files\NSIS\makensis.exe" (
    set NSIS_PATH=C:\Program Files\NSIS\makensis.exe
) else (
    echo Warning: NSIS not found. Will build but not package.
)

REM Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

REM Configure with CMake
echo Configuring build...
cmake "%PROJECT_DIR%" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_INSTALL_PREFIX="%INSTALL_DIR%" ^
    -DBUILD_GUI=ON ^
    -DBUILD_CLI=ON ^
    -DBUILD_SERVER=OFF ^
    -DBUILD_TESTS=OFF

if errorlevel 1 (
    echo CMake configuration failed
    exit /b 1
)

REM Build
echo Building...
cmake --build . --config Release --parallel

if errorlevel 1 (
    echo Build failed
    exit /b 1
)

REM Install
echo Installing...
cmake --install . --config Release

if errorlevel 1 (
    echo Installation failed
    exit /b 1
)

REM Deploy Qt dependencies
echo Deploying Qt dependencies...
windeployqt --release --no-translations --no-system-d3d-compiler ^
    "%INSTALL_DIR%\bin\lumen-gui.exe"

if errorlevel 1 (
    echo Qt deployment failed
    exit /b 1
)

REM Copy additional DLLs if needed
echo Copying runtime libraries...
REM Visual C++ Runtime should be installed system-wide

REM Create installer with NSIS
if defined NSIS_PATH (
    echo Creating NSIS installer...
    "%NSIS_PATH%" /DVERSION=1.0.0 /DINSTALL_DIR="%INSTALL_DIR%" ^
        "%PROJECT_DIR%\scripts\lumen-installer.nsi"

    if errorlevel 1 (
        echo NSIS packaging failed
        exit /b 1
    )

    echo.
    echo === Build Complete ===
    echo Installer: %BUILD_DIR%\Lumen-1.0.0-Setup.exe
) else (
    echo.
    echo === Build Complete ===
    echo Portable build: %INSTALL_DIR%
    echo Note: NSIS not found, installer not created
)

endlocal
