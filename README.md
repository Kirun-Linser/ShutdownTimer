[English](README.en.md) | [简体中文](README.md)

轻量级定时关机工具（Windows）。深色自绘 UI。支持两种关机模式：倒计时关机、定时关机。

功能(待完善)

## 功能

- **倒计时关机**：输入小时 + 分钟，到点自动 `shutdown /s /t 10` 倒计时后关机
- **定时关机**：输入 HH:MM 时间点，到点时自动关机
- **运行态实时倒计时**：窗口中央大字显示剩余 HH:MM:SS，每秒刷新
- **取消即退出**：点击「取消」（或按 ESC）程序直接终止；运行中会先取消已挂起的系统关机任务（`shutdown /a`），并短暂提示「任务已终止」约 2 秒后自动关闭
- **深色自绘 UI**：无边框 + 屏幕置顶 + 圆角 44px + 屏幕 1/3 居中；PerMonitorV2 高 DPI 感知 + 像素制字体缩放；配色与 MemoryCleaner 规范一致
- **零运行时依赖**：单 exe，Win10/11 直接运行，无需安装

## 目录结构

```
main.c                        C 源码（Windows, MinGW-w64）
res.rc / shutdowntimer.manifest   资源 + DPI 感知 manifest
gcc_shutdowntimer.specs       构建 specs（移除默认 manifest 避免冲突）
ShutdownTimer.exe             编译好的发行物
```

## 构建（Windows / MinGW-w64 + gcc）

```bat
windres res.rc -O coff -o res.o
gcc -mwindows -municode -O2 -static -specs=gcc_shutdowntimer.specs main.c res.o -o ShutdownTimer.exe
```

注意：`gcc_shutdowntimer.specs` 移除了 gcc 默认注入的 `default-manifest.o`，避免与自定义 manifest（含 PerMonitorV2 DPI 声明）冲突；链接时请务必带上 `-specs=gcc_shutdowntimer.specs`。

可选：运行程序内自检验证逻辑：

```
ShutdownTimer.exe -selftest
```

正常输出 `SELFTEST OK`，退出码 0。

## 发行物

| 文件 | 说明 |
|---|---|
| `ShutdownTimer.exe` | Windows 主程序（Win10/11 直接运行，零依赖） |

## 系统要求

- Windows 10 / 11（Win7 需 UCRT 更新 KB2999226）
- 需要 `shutdown.exe`（Windows 自带）
