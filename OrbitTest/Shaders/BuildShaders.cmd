@echo off
rem Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
rem Licensed under the GNU General Public License v3.0 or later; see LICENSE.

rem Compiles the HLSL shaders to Xbox 360 microcode headers (Shaders\*.h).
rem Run by the project's pre-build step; a shader error fails the build.

setlocal
set FXC="%XEDK%\bin\win32\fxc.exe"
pushd "%~dp0"

call :fx OrbVS        vs_3_0 || goto fail
call :fx OrbPS        ps_3_0 || goto fail
call :fx SolidVS      vs_3_0 || goto fail
call :fx SolidPS      ps_3_0 || goto fail
call :fx QuadVS       vs_3_0 || goto fail
call :fx BackgroundPS ps_3_0 || goto fail
call :fx StarVS       vs_3_0 || goto fail
call :fx BrightPS     ps_3_0 || goto fail
call :fx DownPS       ps_3_0 || goto fail
call :fx BlurPS       ps_3_0 || goto fail
call :fx CompositePS  ps_3_0 || goto fail
call :fx FinalPS      ps_3_0 || goto fail
call :fx LabelVS      vs_3_0 || goto fail
call :fx LabelPS      ps_3_0 || goto fail
call :fx TrackPS      ps_3_0 || goto fail
call :fx HudVS        vs_3_0 || goto fail
call :fx HudSpritePS  ps_3_0 || goto fail
call :fx HudTextPS    ps_3_0 || goto fail
call :fx CasePS       ps_3_0 || goto fail

popd
exit /b 0

:fx
%FXC% /nologo /T%2 /Emain /Fh%1.h /Vng_p%1 %1.hlsl
exit /b %errorlevel%

:fail
popd
echo BuildShaders: shader compile failed
exit /b 1
