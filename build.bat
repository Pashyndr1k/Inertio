@echo off
rem Builds InertiaCursor.exe with Visual Studio's C++ tools (or any CMake toolchain on PATH).
cd /d "%~dp0"
where cmake >nul 2>nul
if errorlevel 1 (
  echo CMake was not found. Install "Desktop development with C++" from the Visual Studio
  echo Installer, then run this again from "Developer Command Prompt for VS".
  pause
  exit /b 1
)
cmake -S . -B build || goto :fail
cmake --build build --config Release || goto :fail
ctest --test-dir build -C Release --output-on-failure || goto :fail
for %%F in (build\Release\InertiaCursor.exe build\InertiaCursor.exe) do if exist %%F (
  echo.
  echo Built %%F
  exit /b 0
)
:fail
echo Build failed.
pause
exit /b 1
