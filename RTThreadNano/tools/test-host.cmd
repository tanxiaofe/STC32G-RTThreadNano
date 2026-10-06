@echo off
setlocal
cd /d "%~dp0.."
if /I not "%VSCMD_ARG_TGT_ARCH%"=="x86" call "D:\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
if not exist build\host mkdir build\host
cl /nologo /TC /O2 /W3 /DRT_HOST_TEST /D_CRT_SECURE_NO_WARNINGS /I. /Ikernel\include /Icomponents\finsh /Iapp /Iport tests\kernel_host.c components\finsh\msh.c components\finsh\shell.c app\commands.c kernel\src\clock.c kernel\src\idle.c kernel\src\ipc.c kernel\src\irq.c kernel\src\kservice.c kernel\src\object.c kernel\src\scheduler.c kernel\src\thread.c kernel\src\timer.c /Fo:build\host\ /Fe:build\host\kernel_host.exe /link /MACHINE:X86
if errorlevel 1 exit /b 1
build\host\kernel_host.exe

if errorlevel 1 exit /b 1
cl /nologo /TC /O2 /W3 /DRT_HOST_TEST /D_CRT_SECURE_NO_WARNINGS /I. /Ikernel\include /Icomponents\finsh tests\usb_console_host.c bsp\usb_console.c /Fo:build\host\ /Fe:build\host\usb_console_host.exe /link /MACHINE:X86
if errorlevel 1 exit /b 1
build\host\usb_console_host.exe
