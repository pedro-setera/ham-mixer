@echo off
setlocal enabledelayedexpansion

echo.
echo ============================================
echo    Qt6 Installation Helper
echo ============================================
echo.

:: Check if Qt is already installed
for %%P in (
    "C:\Qt\6.8.0\msvc2022_64"
    "C:\Qt\6.7.2\msvc2022_64"
    "C:\Qt\6.6.0\msvc2022_64"
    "C:\Qt\6.5.3\msvc2019_64"
) do (
    if exist "%%~P\bin\qmake.exe" (
        echo [OK] Qt already installed at: %%~P
        echo.
        echo You can proceed with building HamMixer.
        echo Run: build.bat
        echo.
        pause
        exit /b 0
    )
)

echo Qt6 is not installed on this system.
echo.
echo Choose installation method:
echo.
echo [1] Download Qt Online Installer (Opens browser)
echo     - Easiest method, but requires Qt account
echo     - Full IDE and tools included
echo.
echo [2] Install via aqtinstall (Python required)
echo     - Command-line installer
echo     - Faster, no account needed
echo.
echo [3] Install via vcpkg
echo     - Package manager approach
echo     - Good for development
echo.
echo [4] Cancel
echo.

set /p CHOICE="Enter choice (1-4): "

if "%CHOICE%"=="1" (
    echo.
    echo Opening Qt download page...
    start https://www.qt.io/download-qt-installer
    echo.
    echo After installing Qt:
    echo 1. Select Qt 6.5 or later
    echo 2. Select MSVC 2019/2022 64-bit kit
    echo 3. Run build.bat to compile HamMixer
    echo.
    pause
    exit /b 0
)

if "%CHOICE%"=="2" (
    echo.
    echo Checking for Python...
    where python >nul 2>&1
    if errorlevel 1 (
        echo Python not found! Please install Python 3.8+ first.
        echo Download from: https://www.python.org/downloads/
        pause
        exit /b 1
    )

    echo Installing aqtinstall...
    pip install aqtinstall

    echo.
    echo Installing Qt 6.7.2 MSVC 2019 64-bit...
    echo This may take 10-20 minutes...
    echo.

    aqt install-qt windows desktop 6.7.2 win64_msvc2019_64 -O C:\Qt

    if errorlevel 1 (
        echo.
        echo [ERROR] Qt installation failed!
        echo You may need to try the Online Installer instead.
        pause
        exit /b 1
    )

    echo.
    echo [OK] Qt installed to C:\Qt\6.7.2\msvc2019_64
    echo.
    echo Now run: build.bat
    pause
    exit /b 0
)

if "%CHOICE%"=="3" (
    echo.
    echo Checking for vcpkg...

    if not exist "C:\vcpkg\vcpkg.exe" (
        echo vcpkg not found. Installing vcpkg...
        git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
        cd /d C:\vcpkg
        call bootstrap-vcpkg.bat
    )

    echo.
    echo Installing Qt6 via vcpkg...
    echo This may take 30-60 minutes...
    echo.

    C:\vcpkg\vcpkg install qt6:x64-windows

    if errorlevel 1 (
        echo.
        echo [ERROR] Qt installation failed!
        pause
        exit /b 1
    )

    echo.
    echo [OK] Qt installed via vcpkg
    echo.
    echo Set this environment variable before building:
    echo CMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
    echo.
    pause
    exit /b 0
)

echo Cancelled.
pause
exit /b 0
