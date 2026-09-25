@echo off
chcp 65001 >nul
cd /d "%~dp0"
py -3 --version >nul 2>&1
if errorlevel 1 (
  echo Python 3 is required. Install it from https://www.python.org/downloads/windows/
  pause
  exit /b 1
)
py -3 -m pip install "esptool==5.3.0" "pyserial==3.5"
if errorlevel 1 (
  pause
  exit /b 1
)
py -3 update.py
pause
