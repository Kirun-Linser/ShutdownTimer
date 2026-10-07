[English](README.en.md) | [简体中文](README.md)

A lightweight shutdown timer for Windows. Dark custom-drawn UI. Two modes: countdown shutdown and scheduled shutdown.

## Features

- **Countdown shutdown**: enter hours + minutes; when it hits zero the app runs `shutdown /s /t 10`
- **Scheduled shutdown**: enter a HH:MM time; it shuts down when that time arrives
- **Live countdown**: the remaining HH:MM:SS is shown in large type in the middle of the window, refreshed every second
- **Cancel = exit**: clicking "Cancel" (or pressing ESC) terminates the program; if a shutdown is pending it first runs `shutdown /a`, briefly shows "task terminated" and closes itself after about 2 seconds
- **Dark custom-drawn UI**: borderless, always-on-top, **layered-window antialiased rounded corners**, centered at one third of the screen; PerMonitorV2 DPI awareness with pixel-based font scaling; palette consistent with Castling
- **No console flash**: the shutdown command is launched through `ShellExecuteW` with a hidden window (earlier versions used `system()` and flashed a console box)
- **Zero runtime dependencies**: a single exe, runs directly on Win10/11, nothing to install
- **Built-in self-test**: `ShutdownTimer.exe -selftest` verifies countdown / scheduled / cancel / invalid-input paths

## Directory Layout

```
main.c                       C source (Windows, MinGW-w64)
res.rc                       icon + version info + manifest resources
shutdowntimer.manifest       DPI-awareness manifest (PerMonitorV2)
shutdowntimer.ico            app icon (16/24/32/48/64/128/256)
icon-1024.png                1024px icon source (for regenerating later)
gcc_shutdowntimer.specs      build specs (removes the default manifest)
build.bat                    one-click build script
ShutdownTimer.exe            prebuilt release
```

## Building (Windows / MinGW-w64 + gcc)

```bat
build.bat
```

Or manually:

```bat
windres -c 65001 res.rc -O coff -o res.o
gcc -mwindows -municode -O2 -static -specs=gcc_shutdowntimer.specs main.c res.o -o ShutdownTimer.exe -lgdiplus -lshell32
```

Notes:

- `-c 65001` tells windres to read the rc file as UTF-8 (the version info contains Chinese); without it the text is garbled
- `-lgdiplus` provides the antialiased rounded-corner rendering, `-lshell32` provides the hidden-window command execution
- `gcc_shutdowntimer.specs` removes the `default-manifest.o` that gcc injects by default, so it does not conflict with the custom manifest (which carries the PerMonitorV2 DPI declaration); always pass it when linking

Optional: run the built-in self-test:

```
ShutdownTimer.exe -selftest
```

It prints `SELFTEST OK` and exits with code 0.

## Releases

| File | Description |
|---|---|
| `ShutdownTimer.exe` | Windows main program (runs directly on Win10/11, zero dependencies) |

## System Requirements

- Windows 10 / 11 (Win7 needs UCRT update KB2999226)
- Requires `shutdown.exe` (ships with Windows)
