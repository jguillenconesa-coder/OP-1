@echo off
setlocal
cd /d "%~dp0\..\.."
where cmake >nul 2>&1
if errorlevel 1 (
  echo Install CMake, Visual Studio 2022 C++ tools, Git and NSIS 3 first.
  exit /b 1
)
cmake -S . -B build -A x64
if errorlevel 1 exit /b 1
cmake --build build --config Release --parallel 2
if errorlevel 1 exit /b 1
ctest --test-dir build -C Release --output-on-failure
if errorlevel 1 exit /b 1
powershell -NoProfile -File packaging/windows/build-installer.ps1 -BuildDir build
if errorlevel 1 exit /b 1
echo Installer is available in the dist folder.
