@echo off
REM QUIET Post-Installation Configuration Script
REM This script helps set up the virtual audio device after installation

echo.
echo ============================================
echo QUIET Post-Installation Configuration
echo ============================================
echo.

REM Check if running as administrator
net session >nul 2>&1
if %errorLevel% == 0 (
    echo Running with administrator privileges...
) else (
    echo This script requires administrator privileges.
    echo Please right-click and select "Run as administrator"
    pause
    exit /b 1
)

REM Check if VB-Cable is installed
echo Checking for VB-Audio Virtual Cable...
reg query "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall" /s | findstr /i "VB-Audio" >nul
if %errorLevel% == 0 (
    echo [OK] VB-Audio Virtual Cable is installed
) else (
    echo [WARNING] VB-Audio Virtual Cable not found
    echo.
    echo QUIET requires VB-Audio Virtual Cable for virtual device functionality.
    echo Please download and install it from: https://vb-audio.com/Cable/
    echo.
    choice /C YN /M "Would you like to open the download page now"
    if errorlevel 2 goto skip_vbcable
    start https://vb-audio.com/Cable/
    echo.
    echo Please install VB-Cable and run this script again.
    pause
    exit /b 1
)
:skip_vbcable

REM Configure Windows Audio settings
echo.
echo Configuring Windows audio settings...

REM Enable audio enhancements
echo Enabling audio processing for QUIET...
powershell -Command "Set-ItemProperty -Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\Render\*\Properties' -Name '{24dbb0fc-9311-4b3d-9cf0-18ff155639d4},2' -Value 1 -Force" 2>nul

REM Set QUIET as startup application (optional)
echo.
choice /C YN /M "Would you like QUIET to start automatically with Windows"
if errorlevel 2 goto skip_startup
echo Adding QUIET to startup...
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v "QUIET" /t REG_SZ /d "\"%ProgramFiles%\QUIET\Quiet.exe\" --minimize" /f
:skip_startup

REM Create firewall exception
echo.
echo Adding Windows Firewall exception for QUIET...
netsh advfirewall firewall add rule name="QUIET Audio Processor" dir=in action=allow program="%ProgramFiles%\QUIET\Quiet.exe" enable=yes profile=any

REM Configure default audio devices
echo.
echo ============================================
echo IMPORTANT: Manual Configuration Required
echo ============================================
echo.
echo To use QUIET with your applications:
echo.
echo 1. Open Windows Sound Settings
echo 2. Set your microphone as the DEFAULT INPUT device
echo 3. In your communication apps (Discord, Zoom, etc.):
echo    - Select "CABLE Input (VB-Audio Virtual Cable)" as microphone
echo 4. QUIET will process audio from your mic and route it to the virtual device
echo.
echo Press any key to open Windows Sound Settings...
pause >nul
start ms-settings:sound

echo.
echo ============================================
echo Configuration Complete!
echo ============================================
echo.
echo You can now run QUIET from:
echo - Start Menu
echo - Desktop shortcut
echo - %ProgramFiles%\QUIET\Quiet.exe
echo.
pause