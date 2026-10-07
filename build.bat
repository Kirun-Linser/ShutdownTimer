@echo off
rem ShutdownTimer Windows build script (MinGW-w64)
setlocal
if "%1"=="" (
    set MINGW=%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin
) else (
    set MINGW=%1
)
echo [1/2] windres resources...
"%MINGW%\windres.exe" -c 65001 res.rc -O coff -o res.o || exit /b 1
echo [2/2] gcc build...
"%MINGW%\gcc.exe" -mwindows -municode -O2 -static -specs=gcc_shutdowntimer.specs main.c res.o -o ShutdownTimer.exe -lgdiplus -lshell32 || exit /b 1
echo Done: ShutdownTimer.exe
endlocal
