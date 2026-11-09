@echo off
setlocal
rem Ninja reads the compiler's header list (/showIncludes) to know what to rebuild.
rem On localized Windows that list is printed in the console code page; switch to
rem UTF-8 so it matches the prefix CMake recorded, otherwise header edits are missed.
chcp 65001 >nul
cd /d "%~dp0"

set "VS_DIR=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools"
if not defined QT_DIR set "QT_DIR=C:\Qt\6.8.3\msvc2022_64"
set "USER_VCPKG_ROOT="
if defined VCPKG_ROOT set "USER_VCPKG_ROOT=%VCPKG_ROOT%"
if not defined VCPKG_ROOT set "VCPKG_ROOT=C:\vcpkg"

if not exist "%QT_DIR%\bin\Qt6Core.dll" (
    echo Qt not found: %QT_DIR%
    echo Set QT_DIR to the msvc2022_64 Qt directory.
    exit /b 1
)
if not exist "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" (
    echo vcpkg not found: %VCPKG_ROOT%
    exit /b 1
)

call "%VS_DIR%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
rem vcvars may set VCPKG_ROOT to the VS-internal vcpkg; restore the standalone one.
if defined USER_VCPKG_ROOT (
    set "VCPKG_ROOT=%USER_VCPKG_ROOT%"
) else (
    set "VCPKG_ROOT=C:\vcpkg"
)

set "CMAKE=%VS_DIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "NINJA=%VS_DIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
set "PATH=%QT_DIR%\bin;%PATH%"

"%CMAKE%" -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_PREFIX_PATH="%QT_DIR%" ^
    -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ^
    -DVCPKG_TARGET_TRIPLET=x64-windows ^
    -DCMAKE_MAKE_PROGRAM="%NINJA%"
if errorlevel 1 exit /b 1

"%CMAKE%" --build build --parallel
if errorlevel 1 exit /b 1

python tools\deploy_dist.py
if errorlevel 1 exit /b 1

echo.
echo Build complete:
echo   dist\MikuLibrary\MikuLibrary.exe
endlocal
