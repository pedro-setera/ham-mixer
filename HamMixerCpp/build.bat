@echo off
setlocal enabledelayedexpansion

echo.
echo ============================================
echo    HamMixer C++ Qt6 Build Script
echo ============================================
echo.

:: Check for Qt installation
set QT_FOUND=0
set QT_PATH=

:: Common Qt installation locations
for %%P in (
    "C:\Qt\6.8.0\msvc2022_64"
    "C:\Qt\6.7.3\msvc2022_64"
    "C:\Qt\6.7.2\msvc2022_64"
    "C:\Qt\6.7.0\msvc2022_64"
    "C:\Qt\6.6.0\msvc2022_64"
    "C:\Qt\6.5.3\msvc2019_64"
    "C:\Qt\6.5.0\msvc2019_64"
    "C:\Qt\6.8.0\mingw_64"
    "C:\Qt\6.7.3\mingw_64"
    "C:\Qt\6.7.2\mingw_64"
    "C:\Qt\6.6.0\mingw_64"
    "C:\Qt\6.5.3\mingw_64"
) do (
    if exist "%%~P\bin\qmake.exe" (
        set QT_PATH=%%~P
        set QT_FOUND=1
        goto :qt_found
    )
)

:qt_found
if %QT_FOUND%==0 (
    echo [ERROR] Qt6 not found!
    echo.
    echo Please install Qt6 using one of these methods:
    echo.
    echo 1. Qt Online Installer (Recommended):
    echo    https://www.qt.io/download-qt-installer
    echo    - Select Qt 6.5 or later
    echo    - Select MSVC 2019/2022 64-bit kit
    echo.
    echo 2. vcpkg:
    echo    vcpkg install qt6:x64-windows
    echo.
    echo After installing, run this script again.
    echo.
    pause
    exit /b 1
)

echo [OK] Found Qt at: %QT_PATH%

:: Check for CMake
where cmake >nul 2>&1
if errorlevel 1 (
    echo [ERROR] CMake not found! Please install CMake 3.21+
    pause
    exit /b 1
)
echo [OK] CMake found

:: Check for Visual Studio
set VS_FOUND=0
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community
    set VS_FOUND=1
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
    set VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Professional
    set VS_FOUND=1
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
    set VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Enterprise
    set VS_FOUND=1
)
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community
    set VS_FOUND=1
)

:: Check for MinGW as fallback
set MINGW_FOUND=0
where g++ >nul 2>&1
if not errorlevel 1 (
    set MINGW_FOUND=1
    echo [OK] MinGW found
)

if %VS_FOUND%==0 if %MINGW_FOUND%==0 (
    echo [ERROR] No C++ compiler found!
    echo Please install Visual Studio 2019/2022 or MinGW-w64
    pause
    exit /b 1
)

:: Create build directory
set BUILD_DIR=%~dp0build
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

echo.
echo Configuring project...
echo.

if %VS_FOUND%==1 (
    echo Using Visual Studio generator...
    cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="%QT_PATH%" ..
    if errorlevel 1 (
        echo [ERROR] CMake configuration failed!
        pause
        exit /b 1
    )

    echo.
    echo Building Release configuration...
    echo.
    cmake --build . --config Release
    if errorlevel 1 (
        echo [ERROR] Build failed!
        pause
        exit /b 1
    )

    set EXE_PATH=%BUILD_DIR%\bin\Release\HamMixer.exe
) else (
    echo Using MinGW generator...
    cmake -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="%QT_PATH%" -DCMAKE_BUILD_TYPE=Release ..
    if errorlevel 1 (
        echo [ERROR] CMake configuration failed!
        pause
        exit /b 1
    )

    echo.
    echo Building...
    echo.
    mingw32-make -j4
    if errorlevel 1 (
        echo [ERROR] Build failed!
        pause
        exit /b 1
    )

    set EXE_PATH=%BUILD_DIR%\bin\HamMixer.exe
)

echo.
echo ============================================
echo    Build Successful!
echo ============================================
echo.
echo Executable: %EXE_PATH%
echo.

:: Deploy Qt DLLs
echo Deploying Qt DLLs...
set WINDEPLOYQT=%QT_PATH%\bin\windeployqt.exe
if exist "%WINDEPLOYQT%" (
    "%WINDEPLOYQT%" --release --no-translations "%EXE_PATH%"
    echo [OK] Qt DLLs deployed
) else (
    echo [WARNING] windeployqt not found, you may need to copy Qt DLLs manually
)

echo.
echo Done! You can now run HamMixer.exe
echo.
pause
