[English](README.en.md) | [简体中文](README.md)

轻量级定时关机工具（Windows）。深色自绘 UI。支持两种关机模式：倒计时关机、定时关机。

## 功能

- **倒计时关机**：输入小时 + 分钟，到点自动 `shutdown /s /t 10` 倒计时后关机
- **定时关机**：输入 HH:MM 时间点，到点时自动关机
- **运行态实时倒计时**：窗口中央大字显示剩余 HH:MM:SS，每秒刷新
- **取消即退出**：点击「取消」（或按 ESC）程序直接终止；运行中会先取消已挂起的系统关机任务（`shutdown /a`），并短暂提示「任务已终止」约 2 秒后自动关闭
- **深色自绘 UI**：无边框 + 屏幕置顶 + **分层窗口抗锯齿圆角** + 屏幕 1/3 居中；PerMonitorV2 高 DPI 感知 + 像素制字体缩放；配色与 Castling 规范一致
- **无控制台黑框**：关机命令经 `ShellExecuteW` 以隐藏窗口执行（早期版本用 `system()` 会闪出黑框）
- **零运行时依赖**：单 exe，Win10/11 直接运行，无需安装
- **自带自检**：`ShutdownTimer.exe -selftest` 校验倒计时 / 定时 / 取消 / 非法输入

## 目录结构

```
main.c                       C 源码（Windows, MinGW-w64）
res.rc                       图标 + 版本信息 + manifest 资源
shutdowntimer.manifest       DPI 感知 manifest（PerMonitorV2）
shutdowntimer.ico            应用图标（16/24/32/48/64/128/256 七尺寸）
icon-1024.png                图标源图（1024px，便于日后重新生成）
gcc_shutdowntimer.specs      构建 specs（移除默认 manifest 避免冲突）
build.bat                    一键构建脚本
ShutdownTimer.exe            编译好的发行物
```

## 构建（Windows / MinGW-w64 + gcc）

```bat
build.bat
```

或手动执行：

```bat
windres -c 65001 res.rc -O coff -o res.o
gcc -mwindows -municode -O2 -static -specs=gcc_shutdowntimer.specs main.c res.o -o ShutdownTimer.exe -lgdiplus -lshell32
```

注意：

- `-c 65001` 指定 rc 源文件按 UTF-8 解析（版本信息含中文），缺此参数会乱码
- `-lgdiplus` 提供抗锯齿圆角渲染，`-lshell32` 提供隐藏窗口执行关机命令
- `gcc_shutdowntimer.specs` 移除了 gcc 默认注入的 `default-manifest.o`，避免与自定义 manifest（含 PerMonitorV2 DPI 声明）冲突；链接时请务必带上

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
