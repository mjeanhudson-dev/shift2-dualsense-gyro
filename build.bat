@echo off
setlocal
cd /d "%~dp0"
echo.
echo === DualSense XInput proxy builder (32-bit) ===
echo Tip: drag the GAME'S FOLDER (or its .exe) onto this file to build AND install in one step.
echo.

set "GPP="
if exist "C:\msys64\mingw32\bin\g++.exe" set "GPP=C:\msys64\mingw32\bin\g++.exe"
if not defined GPP for /f "delims=" %%i in ('where g++ 2^>nul') do if not defined GPP set "GPP=%%i"
if not defined GPP goto nocompiler

for /f "delims=" %%t in ('"%GPP%" -dumpmachine') do set "TARGET=%%t"
echo Compiler: %GPP%
echo Target:   %TARGET%
echo %TARGET%| find /i "i686" >nul
if errorlevel 1 goto wrongarch

for %%d in ("%GPP%") do set "PATH=%%~dpd;%PATH%"

"%GPP%" -std=c++17 -O2 -Wall -Wno-cast-function-type -shared -static -static-libgcc -static-libstdc++ -s -o xinput1_3.dll xinput_proxy.cpp xinput1_3.def -lsetupapi -Wl,--kill-at
if errorlevel 1 goto failed
echo.
echo Built xinput1_3.dll

set "DEST=%~1"
if not defined DEST goto manual
if exist "%DEST%\" (set "DEST=%DEST%\") else for %%f in ("%DEST%") do set "DEST=%%~dpf"
dir /b "%DEST%*.exe" >nul 2>nul
if errorlevel 1 echo WARNING: no .exe found in %DEST% - is this really the game folder?
set "NAME=%~2"
if not defined NAME set "NAME=xinput1_3"
copy /y xinput1_3.dll "%DEST%%NAME%.dll" >nul
if errorlevel 1 goto copyfail
if exist "%DEST%shift2_dualsense.ini" goto keepini
copy shift2_dualsense.ini "%DEST%" >nul
goto inidone
:keepini
copy /y shift2_dualsense.ini "%DEST%shift2_dualsense.new.ini" >nul
echo Kept your existing shift2_dualsense.ini. New settings and notes are in shift2_dualsense.new.ini - compare the two.
:inidone
echo Installed %NAME%.dll into %DEST%
echo Log file after a run: shift2_dualsense.log in that folder (or in %%TEMP%%)
goto end

:manual
echo Copy xinput1_3.dll and shift2_dualsense.ini next to the game's .exe.
goto end

:nocompiler
echo No g++ found. Install MSYS2, open its MINGW32 shell and run:
echo     pacman -S mingw-w64-i686-gcc
echo Then run this file again.
goto end

:wrongarch
echo That g++ is not the 32-bit one (target must contain i686).
echo In the MSYS2 MINGW32 shell run: pacman -S mingw-w64-i686-gcc
goto end

:failed
echo.
echo BUILD FAILED - copy the error text above and send it to me.
goto end

:copyfail
echo Could not copy into %DEST% - try again as administrator, or copy the files by hand.

:end
echo.
if not defined NO_PAUSE if not defined CI pause
