# ShutdownTimer

[English](README.en.md) | [简体中文](README.md)

A lightweight scheduled-shutdown tool for Windows. Dark self-drawn UI, styled after [MemoryCleaner](https://github.com/Kirun-Linser/MemoryCleaner). Two modes: countdown shutdown and scheduled shutdown.

## Features

- **Countdown shutdown**: enter hours + minutes; on timeout it auto-runs `shutdown /s /t 10` to shut down
- **Scheduled shutdown**: enter a clock time HH:MM; shutdown triggers at that time
- **Live countdown**: large centered text shows remaining HH:MM:SS, refreshed every second
- **Cancel to exit**: clicking "Cancel" (or pressing ESC) ends the program; if a countdown is running it first aborts the pending system shutdown (`shutdown /a`), shows a short "任务已终止" notice, and auto-closes after ~2 seconds
- **Dark self-drawn UI**: borderless, always-on-top, 44 px rounded corners, centered at 1/3 of the screen; PerMonitorV2 high-DPI awareness with pixel-based font scaling; color palette aligned with the MemoryCleaner spec
- **Zero runtime dependencies**: a single exe, runs directly on Win10/11, no install

## Layout

```
main.c                            C source (Windows, MinGW-w64)
res.rc / shutdowntimer.manifest   resources + DPI-aware manifest
gcc_shutdowntimer.specs           build specs (drops the default manifest)
ShutdownTimer.exe                 prebuilt binary
```

## Build (Windows / MinGW-w64 + gcc)

```bat
windres res.rc -O coff -o res.o
gcc -mwindows -municode -O2 -static -specs=gcc_shutdowntimer.specs main.c res.o -o ShutdownTimer.exe
```

Note: `gcc_shutdowntimer.specs` strips the default `default-manifest.o` injected by gcc to avoid a conflict with the custom manifest (which declares PerMonitorV2 DPI awareness); always link with `-specs=gcc_shutdowntimer.specs`.

Optional: run the built-in self-test to verify the logic:

```
ShutdownTimer.exe -selftest
```

Prints `SELFTEST OK` and exits with code 0 on success.

## Release artifact

| File | Description |
|---|---|
| `ShutdownTimer.exe` | Windows main executable (Win10/11, zero-dependency) |

## Requirements

- Windows 10 / 11 (Win7 requires the UCRT update KB2999226)
- Requires `shutdown.exe` (bundled with Windows)
