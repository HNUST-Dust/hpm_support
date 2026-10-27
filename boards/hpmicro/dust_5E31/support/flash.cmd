@echo off
setlocal
if "%~1"=="" (
    echo Build directory is required. 1>&2
    exit /b 2
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0flash.ps1" -ElfFile "%~1\zephyr\zephyr.elf"
exit /b %ERRORLEVEL%
