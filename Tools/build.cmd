@echo off
rem Builds Orbit inside the WinBoat Windows VM (Xbox 360 SDK + VS 2010 installed there).
rem Started by Tools/build.sh through RDP; can also be run by hand inside Windows.
rem The project is reached through WinBoat's share of the Linux home (\\host.lan\Data),
rem copied to C:\Projects\Orbit (MSBuild and the SDK tools don't like UNC paths),
rem built, and Orbit.xex and the log come back to <project>\Builds\.
rem
rem   build.cmd [Release|Debug]

setlocal
set CONFIG=%~1
if "%CONFIG%"=="" set CONFIG=Release
set SRC=%~dp0..
set OUT=%~dp0..\Builds
set WORK=C:\Projects\Orbit
if not exist "%OUT%" mkdir "%OUT%"
set LOG=%OUT%\build.log

echo START %DATE% %TIME% %CONFIG% > "%LOG%"
robocopy "%SRC%" "%WORK%" /E /XD .git __pycache__ Builds Backups /NFL /NDL /NJH /NP >> "%LOG%"
cd /d "%WORK%"
C:\Windows\Microsoft.NET\Framework\v4.0.30319\MSBuild.exe OrbitTest.sln /p:Configuration=%CONFIG% /p:Platform="Xbox 360" /v:minimal >> "%LOG%" 2>&1
echo MSBUILD_EXIT %ERRORLEVEL% >> "%LOG%"
if exist "OrbitTest\%CONFIG%\OrbitTest.xex" copy /y "OrbitTest\%CONFIG%\OrbitTest.xex" "%OUT%\Orbit.xex" >> "%LOG%"
echo DONE >> "%LOG%"
