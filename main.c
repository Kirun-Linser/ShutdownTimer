/*
 * ShutdownTimer - 定时关机工具（深色自绘 UI，与 Castling 统一风格）
 * 双模式：倒计时（小时+分钟） / 定时（HH:MM）
 * 像素制 DPI 缩放（MulDiv(px, scale, 100)）；坐标基于窗口宽高百分比自适应
 * 编译：gcc -mwindows -municode -O2 -static -specs=gcc_shutdowntimer.specs \
 *       -o ShutdownTimer.exe main.c res.o -lgdiplus -lshell32
 * v0.1.0: 改用分层窗口 + GDI+ 抗锯齿圆角（与 Castling 同一方案）；补图标与版本信息
 */
#include <windows.h>
#include <shellapi.h>
#include <gdiplus.h>
#include <wchar.h>
#include <stdio.h>
#include <time.h>

/* ===== 配色（与 Castling 规范一致） ===== */
#define COL_BG      RGB(0x0B, 0x0D, 0x12)   /* 背景 */
#define COL_CARD    RGB(0x14, 0x17, 0x1F)   /* 卡片 */
#define COL_CARD_HI RGB(0x1C, 0x21, 0x2C)   /* 卡片 hover */
#define COL_TITLE   RGB(0xF5, 0xF6, 0xFA)   /* 主文字 */
#define COL_BLUE    RGB(0xA9, 0xD3, 0xF5)   /* 强调蓝 */
#define COL_SUB     RGB(0xA8, 0xB0, 0xC2)   /* 副文字 */
#define COL_HINT    RGB(0x75, 0x7D, 0x8F)   /* 弱提示 */
#define COL_BTN     RGB(0x3D, 0x7B, 0xFF)   /* 主按钮 */
#define COL_BTN_HI  RGB(0x5B, 0x90, 0xFF)   /* 主按钮 hover */
#define COL_EDGE    RGB(0x2D, 0x5A, 0xB8)   /* 边框蓝 / 输入焦点（v0.1.0 提亮） */
#define COL_WHITE   RGB(0xFF, 0xFF, 0xFF)

#define ID_TIMER 2001
#define MODE_COUNT 0
#define MODE_AT    1

/* ===== 状态 ===== */
static int       mode = MODE_COUNT;
static int       running = 0;
static int       exiting = 0;          /* 正在显示“任务已终止”提示，2s 后退出 */
static long long exit_at = 0;          /* 自动退出时间戳 */
static long long target_sec = 0;
static int       hoverStart = 0, hoverCancel = 0, hoverMode[2] = {0, 0};
static int       focusBox = 0;   /* 0=无 1=小时 2=分钟 3=时间点 */
static wchar_t   bufH[4] = L"0", bufM[4] = L"30", bufT[5] = L"2300";
static HFONT     fTitle, fBtn, fBig, fSub, fInput;
static int       g_scale;   /* DPI 缩放百分比 */
static int       g_radius;  /* 圆角半径（随 DPI 缩放） */

/* 前向声明：RenderWindow 定义在 paint() 之后，但更早的函数也要调用 */
static void RenderWindow(HWND hwnd);

/* 界面常量（百分比 / 基字高） */
#define PAD_DIV     20      /* pad = W/20 */
#define ICON_PCT    13      /* 图标直径 = W*13% 上限 56 */
#define MODE_Y_PCT  24      /* 模式卡 top */
#define MODE_H_PCT  12      /* 模式卡高 = H*12% */
#define INPUT_Y_PCT 48      /* 输入区 top */
#define STATUS_Y_PCT 63     /* 状态大字/提示 top */
#define BTN_Y_PCT   80      /* 按钮行 top */
#define BTN_W_PCT   36      /* 按钮宽 = 可用宽*36%（等宽两枚） */
#define BTN_H       44      /* 按钮高 44px（像素制） */

/* 输入区几何常量（绘制与命中共用，避免两处硬编码不同步） */
#define IN_BW_SMALL 62      /* 小时/分钟 输入框宽 */
#define IN_BW_BIG   96      /* 时间点 输入框宽 */
#define IN_BH       36      /* 输入框高 */
#define IN_H_OFF    50      /* 小时框 x = pad + 此值 */
#define IN_M_OFF    172     /* 分钟框 x */
#define IN_T_OFF    100     /* 时间点框 x */

static HFONT MakeFont(int px, int weight) {
    return CreateFontW(-MulDiv(px, g_scale, 100), 0, 0, 0, weight, 0, 0, 0,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
}

static void DrawRoundRect(HDC dc, int x, int y, int w, int h, int r, COLORREF c) {
    HBRUSH br = CreateSolidBrush(c);
    HPEN   pn = CreatePen(PS_SOLID, 1, c);
    HGDIOBJ ob = SelectObject(dc, br), op = SelectObject(dc, pn);
    RoundRect(dc, x, y, x + w, y + h, r * 2, r * 2);
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(br); DeleteObject(pn);
}

static int in_rect(int mx, int my, int x, int y, int w, int h) {
    return mx >= x && mx < x + w && my >= y && my < y + h;
}

    /* 文本辅助：在指定 RECT 内居中 / 左对齐整行（paint 顶部已设 TRANSPARENT） */
static void DrawRectText(HDC dc, const wchar_t *t, int x, int y, int w, int h,
                         HFONT f, COLORREF c, UINT fmt) {
    RECT rr = { x, y, x + w, y + h };
    SetTextColor(dc, c); SelectObject(dc, f);
    DrawTextW(dc, t, -1, &rr, fmt | DT_VCENTER | DT_SINGLELINE);
}
static void TextCenter(HDC dc, const wchar_t *t, int y, int w, int h, HFONT f, COLORREF c) {
    DrawRectText(dc, t, 0, y, w, h, f, c, DT_CENTER);
}
static void TextLeft(HDC dc, const wchar_t *t, int x, int y, int w, int h, HFONT f, COLORREF c) {
    DrawRectText(dc, t, x, y, w, h, f, c, DT_LEFT);
}

/* 各区域矩形（与绘制/命中统一计算，保证“点哪就是哪”） */
static void layout(int Wd, int Hd, int *padP, int *modeY, int *modeH,
                   int *inputY, int *statusY, int *btnY, int *btnW) {
    int pad = Wd / PAD_DIV;
    *padP    = pad;
    *modeY   = Hd * MODE_Y_PCT / 100;
    *modeH   = Hd * MODE_H_PCT / 100;
    *inputY  = Hd * INPUT_Y_PCT / 100;
    *statusY = Hd * STATUS_Y_PCT / 100;
    *btnY    = Hd * BTN_Y_PCT / 100;
    *btnW    = (Wd - 2 * pad) * BTN_W_PCT / 100;
}

/* ===== 输入 ===== */
static void input_char(wchar_t ch) {
    if (ch < L'0' || ch > L'9') return;
    if (focusBox == 1 && wcslen(bufH) < 2) { wcscat(bufH, &ch); return; }
    if (focusBox == 2 && wcslen(bufM) < 2) { wcscat(bufM, &ch); return; }
    if (focusBox == 3) {
        if (wcslen(bufT) < 4) {
            int len = (int)wcslen(bufT);
            if (len == 0 && ch == L'0') return;
            bufT[len] = ch; bufT[len + 1] = 0;
            return;
        }
    }
}
static void input_backspace(void) {
    int len;
    if (focusBox == 1) { len = (int)wcslen(bufH); if (len > 1) bufH[len - 1] = 0; }
    if (focusBox == 2) { len = (int)wcslen(bufM); if (len > 1) bufM[len - 1] = 0; }
    if (focusBox == 3) { len = (int)wcslen(bufT); if (len > 0) bufT[len - 1] = 0; }
}

/* ===== 任务 ===== */
/* 隐藏窗口执行系统 shutdown 命令（原用 system() 会闪出控制台黑框） */
static void run_hidden(const wchar_t *args) {
    ShellExecuteW(NULL, L"open", L"shutdown", args, NULL, SW_HIDE);
}
static void do_shutdown(void) { run_hidden(L"/s /t 10"); }
static void do_cancel(void)   { run_hidden(L"/a"); }

static int  start_task(HWND hwnd) {   /* 返回 1=已启动 */
    long long now = (long long)time(NULL);
    if (mode == MODE_COUNT) {
        int h = _wtoi(bufH), m = _wtoi(bufM);
        if (h == 0 && m == 0) return 0;
        target_sec = now + (long long)h * 3600 + (long long)m * 60;
    } else {
        int hh = 0, mm = 0;
        if (wcslen(bufT) == 4) swscanf(bufT, L"%2d%2d", &hh, &mm);
        if (wcslen(bufT) != 4 || hh < 0 || hh > 23 || mm < 0 || mm > 59) return 0;
        time_t tnow = time(NULL);
        struct tm t; localtime_s(&t, &tnow);
        t.tm_hour = hh; t.tm_min = mm; t.tm_sec = 0;
        target_sec = (long long)mktime(&t);
        if (target_sec <= now) target_sec += 86400;
    }
    running = 1;
    focusBox = 0;
    SetTimer(hwnd, ID_TIMER, 1000, NULL);
    RenderWindow(hwnd);
    return 1;
}
static void cancel_task(HWND hwnd) {
    if (running) {
        running = 0;
        KillTimer(hwnd, ID_TIMER);
        do_cancel();   /* 取消系统关机任务 */
    }
    RenderWindow(hwnd);
}
/* “取消”/ESC：中止任务并显示“任务已终止”，约 2s 后自动退出程序。 */
static void cancel_exit(HWND hwnd) {
    if (exiting) return;
    cancel_task(hwnd);                       /* 中止并复位（内部仅在运行中 do_cancel） */
    exiting = 1;
    exit_at = (long long)time(NULL) + 2;
    SetTimer(hwnd, ID_TIMER, 500, NULL);     /* 每 0.5s check，到点退出 */
    RenderWindow(hwnd);
}

/* ===== 绘制 ===== */
static void paint(HWND hwnd, HDC dc) {
    RECT rc; GetClientRect(hwnd, &rc);
    int Wd = rc.right, Hd = rc.bottom;
    HBRUSH bg = CreateSolidBrush(COL_BG);
    FillRect(dc, &rc, bg);
    DeleteObject(bg);
    SetBkMode(dc, TRANSPARENT);

    /* 取消/退出提示态：仅显示“任务已终止”，约 2s 后自动关窗口 */
    if (exiting) {
        TextCenter(dc, L"任务已终止", Hd * 42 / 100, Wd, 52, fBig, COL_BLUE);
        TextCenter(dc, L"即将关闭…", Hd * 42 / 100 + 56, Wd, 30, fSub, COL_HINT);
        return;
    }

    int pad, modeY, modeH, inputY, statusY, btnY, btnW;
    layout(Wd, Hd, &pad, &modeY, &modeH, &inputY, &statusY, &btnY, &btnW);

    /* ---- 顶部：图标 + 标题（无副标题） ---- */
    int icon = Wd * ICON_PCT / 100; if (icon > 56) icon = 56;
    int icx = pad + icon / 2, icy = Hd * 9 / 100 + icon / 2;
    {
        HBRUSH icf = CreateSolidBrush(COL_CARD);
        HPEN   icp = CreatePen(PS_SOLID, 3, COL_BLUE);
        HGDIOBJ obf = SelectObject(dc, icf), obp = SelectObject(dc, icp);
        Ellipse(dc, icx - icon / 2, icy - icon / 2, icx + icon / 2, icy + icon / 2);
        HPEN lpn = CreatePen(PS_SOLID, 3, COL_BLUE);
        SelectObject(dc, lpn);
        MoveToEx(dc, icx, icy, NULL);   LineTo(dc, icx + icon / 4, icy - icon / 5);
        MoveToEx(dc, icx, icy, NULL);   LineTo(dc, icx - icon / 5, icy + icon / 4);
        SelectObject(dc, obf); SelectObject(dc, obp);
        DeleteObject(lpn); DeleteObject(icp); DeleteObject(icf);
    }
    TextLeft(dc, L"定时关机工具", pad + icon + Wd * 3 / 100, Hd * 8 / 100, Wd - 2 * pad, 46, fTitle, COL_TITLE);

    /* ---- 模式卡：两个横向卡片，选中=蓝块白字居中 ---- */
    {
        int mw = Wd / 2 - pad * 2, sx0 = pad + pad / 2;
        for (int i = 0; i < 2; i++) {
            int sx = sx0 + i * (Wd / 2);
            int sel = (mode == i);
            DrawRoundRect(dc, sx, modeY, mw, modeH, 8, sel ? COL_BTN : (hoverMode[i] ? COL_CARD_HI : COL_CARD));
            DrawRectText(dc, i ? L"定时" : L"倒计时", sx, modeY, mw, modeH, fBtn, sel ? COL_WHITE : COL_SUB, DT_CENTER);
        }
    }

    /* ---- 输入区 ---- */
    {
        int bh = IN_BH, iy = inputY;
        if (mode == MODE_COUNT) {
            int bw = IN_BW_SMALL, bx = pad + IN_H_OFF;
            TextLeft(dc, L"小时", pad, iy + 6, 46, 29, fSub, COL_HINT);
            int f1 = (focusBox == 1);
            DrawRoundRect(dc, bx, iy, bw, bh, 6, f1 ? COL_EDGE : COL_CARD);
            DrawRectText(dc, bufH, bx, iy, bw, bh, fInput, f1 ? COL_BLUE : COL_TITLE, DT_CENTER);
            TextLeft(dc, L"分钟", pad + 122, iy + 6, 46, 29, fSub, COL_HINT);
            int f2 = (focusBox == 2);
            DrawRoundRect(dc, pad + IN_M_OFF, iy, bw, bh, 6, f2 ? COL_EDGE : COL_CARD);
            DrawRectText(dc, bufM, pad + IN_M_OFF, iy, bw, bh, fInput, f2 ? COL_BLUE : COL_TITLE, DT_CENTER);
        } else {
            int bw = IN_BW_BIG, bx = pad + IN_T_OFF;
            TextLeft(dc, L"时间点", pad, iy + 6, 96, 29, fSub, COL_HINT);
            int f3 = (focusBox == 3);
            DrawRoundRect(dc, bx, iy, bw, bh, 6, f3 ? COL_EDGE : COL_CARD);
            wchar_t disp[6] = L"--:--";
            static const int m4[4] = {0, 1, 3, 4};   /* bufT(4位) → display(冒号位) */
            for (int k = 0; k < 4; k++) if (bufT[k]) disp[m4[k]] = bufT[k];
            DrawRectText(dc, disp, bx, iy, bw, bh, fInput, f3 ? COL_BLUE : COL_TITLE, DT_CENTER);
        }
    }

    /* ---- 状态大字 / 提示 ---- */
    if (running) {
        long long r = target_sec - (long long)time(NULL); if (r < 0) r = 0;
        wchar_t big[64];
        wsprintfW(big, L"%02I64d:%02I64d:%02I64d", r / 3600, (r % 3600) / 60, r % 60);
        TextCenter(dc, big, statusY, Wd, 56, fBig, COL_BLUE);
        TextCenter(dc, L"运行中 · 点击\"取消\"中止", statusY + 58, Wd, 26, fSub, COL_HINT);
    } else {
        TextCenter(dc, L"点击\"开始\"启动，到点自动关机", statusY, Wd, 34, fSub, COL_HINT);
    }

    /* ---- 底部按钮：开始(蓝主钮) + 取消(始终可点，结束程序) ---- */
    {
        int bw = btnW, bh = BTN_H, cx2 = Wd - pad - bw;
        if (running) {
            DrawRoundRect(dc, pad, btnY, bw, bh, 8, COL_CARD);
            DrawRectText(dc, L"运行中", pad, btnY, bw, bh, fBtn, COL_HINT, DT_CENTER);
        } else {
            DrawRoundRect(dc, pad, btnY, bw, bh, 8, hoverStart ? COL_BTN_HI : COL_BTN);
            DrawRectText(dc, L"开始", pad, btnY, bw, bh, fBtn, COL_WHITE, DT_CENTER);
        }
        DrawRoundRect(dc, cx2, btnY, bw, bh, 8, hoverCancel ? COL_CARD_HI : COL_CARD);
        DrawRectText(dc, L"取消", cx2, btnY, bw, bh, fBtn, COL_WHITE, DT_CENTER);
        if (hoverCancel) TextCenter(dc, L"结束程序（或按 ESC）", btnY + bh + 6, Wd, 26, fSub, COL_HINT);
    }

}

/* ===== 分层窗口渲染：GDI+ 抗锯齿圆角 ===== */

/* 构建圆角矩形路径（半径 r，起点 x,y，宽 w 高 h） */
static void MakeRoundRectPath(GpPath **out, int x, int y, int w, int h, int r) {
    GpPath *p = NULL;
    REAL d = (REAL)(r * 2);
    GdipCreatePath(FillModeAlternate, &p);
    GdipAddPathArc(p, (REAL)x, (REAL)y, d, d, 180.0f, 90.0f);
    GdipAddPathArc(p, (REAL)(x + w) - d, (REAL)y, d, d, 270.0f, 90.0f);
    GdipAddPathArc(p, (REAL)(x + w) - d, (REAL)(y + h) - d, d, d, 0.0f, 90.0f);
    GdipAddPathArc(p, (REAL)x, (REAL)(y + h) - d, d, d, 90.0f, 90.0f);
    GdipClosePathFigure(p);
    *out = p;
}

/* 整窗渲染：内容走 paint()，圆角由 GDI+ 遮罩提供 alpha，按预乘 alpha 提交给分层窗口 */
static void RenderWindow(HWND hwnd) {
    RECT rc; GetClientRect(hwnd, &rc);
    int W = rc.right, H = rc.bottom;
    if (W <= 0 || H <= 0) return;
    if (!fTitle) return;   /* 字体未初始化（如 -selftest 路径）则跳过渲染 */
    int stride = W * 4, R = g_radius;
    if (R * 2 > W) R = W / 2;
    if (R * 2 > H) R = H / 2;
    HDC screen = GetDC(NULL), mem = CreateCompatibleDC(screen);
    BITMAPINFO bi; ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = W;
    bi.bmiHeader.biHeight = -H;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void *bits = NULL, *mk = NULL;
    HBITMAP dib = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    HBITMAP mdib = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &mk, NULL, 0);
    if (!dib || !mdib || !bits || !mk) {
        if (dib) DeleteObject(dib);
        if (mdib) DeleteObject(mdib);
        DeleteDC(mem); ReleaseDC(NULL, screen);
        return;
    }
    HGDIOBJ ob = SelectObject(mem, dib);
    paint(hwnd, mem);
    {   /* 抗锯齿圆角描边（路径内缩半个线宽，避免被窗口边界裁掉） */
        GpGraphics *g = NULL;
        if (GdipCreateFromHDC(mem, &g) == 0 && g) {
            GpPath *path = NULL; GpPen *pen = NULL;
            MakeRoundRectPath(&path, 1, 1, W - 2, H - 2, R);
            GdipCreatePen1(0xFF2D5AB8u, 1.6f, UnitPixel, &pen);
            GdipSetSmoothingMode(g, SmoothingModeAntiAlias);
            GdipSetPixelOffsetMode(g, PixelOffsetModeHighQuality);
            if (pen) { GdipDrawPath(g, pen, path); GdipDeletePen(pen); }
            GdipDeletePath(path);
            GdipDeleteGraphics(g);
        }
    }
    SelectObject(mem, ob);
    {   /* 圆角遮罩：只取 alpha 通道 */
        memset(mk, 0, (size_t)stride * H);
        HDC mdc = CreateCompatibleDC(screen);
        HGDIOBJ om = SelectObject(mdc, mdib);
        GpGraphics *g2 = NULL;
        if (GdipCreateFromHDC(mdc, &g2) == 0 && g2) {
            GpPath *path = NULL; GpSolidFill *br = NULL;
            GdipSetSmoothingMode(g2, SmoothingModeAntiAlias);
            GdipSetPixelOffsetMode(g2, PixelOffsetModeHighQuality);
            MakeRoundRectPath(&path, 0, 0, W, H, R);
            GdipCreateSolidFill(0xFFFFFFFFu, &br);
            if (br) { GdipFillPath(g2, br, path); GdipDeleteBrush(br); }
            GdipDeletePath(path);
            GdipDeleteGraphics(g2);
        }
        SelectObject(mdc, om); DeleteDC(mdc);
    }
    {   /* 合成 + 预乘 */
        BYTE *op = (BYTE*)bits, *mp = (BYTE*)mk;
        int i, n = W * H;
        for (i = 0; i < n; i++) {
            unsigned a = mp[i * 4 + 3];
            op[i * 4 + 0] = (BYTE)(op[i * 4 + 0] * a / 255);
            op[i * 4 + 1] = (BYTE)(op[i * 4 + 1] * a / 255);
            op[i * 4 + 2] = (BYTE)(op[i * 4 + 2] * a / 255);
            op[i * 4 + 3] = (BYTE)a;
        }
    }
    {   /* 提交（pptDst 传 NULL = 保持窗口当前位置） */
        HDC sdc = CreateCompatibleDC(screen);
        HGDIOBJ os = SelectObject(sdc, dib);
        BLENDFUNCTION bf; SIZE sz; POINT sp;
        bf.BlendOp = AC_SRC_OVER; bf.BlendFlags = 0;
        bf.SourceConstantAlpha = 255; bf.AlphaFormat = AC_SRC_ALPHA;
        sz.cx = W; sz.cy = H; sp.x = 0; sp.y = 0;
        UpdateLayeredWindow(hwnd, screen, NULL, &sz, sdc, &sp, 0, &bf, ULW_ALPHA);
        SelectObject(sdc, os); DeleteDC(sdc);
    }
    DeleteObject(dib); DeleteObject(mdib);
    DeleteDC(mem); ReleaseDC(NULL, screen);
}


static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: RenderWindow(hwnd); ValidateRect(hwnd, NULL); return 0;
    case WM_LBUTTONDOWN: {
        if (exiting) return 0;
        int x = (short)LOWORD(lParam), y = (short)HIWORD(lParam);
        RECT cr; GetClientRect(hwnd, &cr);
        int Wd2 = cr.right, Hd2 = cr.bottom;
        int pad, modeY, modeH, inputY, statusY, btnY, btnW;
        layout(Wd2, Hd2, &pad, &modeY, &modeH, &inputY, &statusY, &btnY, &btnW);
        int bh = BTN_H;

        if (!running) {
            int mw = Wd2 / 2 - pad * 2;
            if (in_rect(x, y, pad + pad / 2, modeY, mw, modeH)) { mode = MODE_COUNT; focusBox = 0; RenderWindow(hwnd); return 0; }
            if (in_rect(x, y, Wd2 / 2 + pad / 2, modeY, mw, modeH)) { mode = MODE_AT; focusBox = 0; RenderWindow(hwnd); return 0; }
        }
        {   /* 开始 */
            if (!running && in_rect(x, y, pad, btnY, btnW, bh)) { start_task(hwnd); return 0; }
            if (in_rect(x, y, Wd2 - pad - btnW, btnY, btnW, bh)) { cancel_exit(hwnd); return 0; }
        }
        {   /* 输入区聚焦 */
            if (mode == MODE_COUNT) {
                int bw = IN_BW_SMALL, bh2 = IN_BH;
                focusBox = in_rect(x, y, pad + IN_H_OFF, inputY, bw, bh2) ? 1
                         : (in_rect(x, y, pad + IN_M_OFF, inputY, bw, bh2) ? 2 : 0);
            } else {
                int bw = IN_BW_BIG, bh2 = IN_BH;
                focusBox = in_rect(x, y, pad + IN_T_OFF, inputY, bw, bh2) ? 3 : 0;
                if (focusBox == 3 && wcscmp(bufT, L"2300") == 0) bufT[0] = 0;
            }
            if (running) focusBox = 0;
            RenderWindow(hwnd);
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        int x = (short)LOWORD(lParam), y = (short)HIWORD(lParam);
        RECT cr; GetClientRect(hwnd, &cr);
        int Wd2 = cr.right, Hd2 = cr.bottom;
        int pad, modeY, modeH, inputY, statusY, btnY, btnW;
        layout(Wd2, Hd2, &pad, &modeY, &modeH, &inputY, &statusY, &btnY, &btnW);
        int bh = BTN_H;
        int hs = in_rect(x, y, pad, btnY, btnW, bh);
        int hc = in_rect(x, y, Wd2 - pad - btnW, btnY, btnW, bh);
        int mw = Wd2 / 2 - pad * 2;
        int hm0 = in_rect(x, y, pad + pad / 2, modeY, mw, modeH);
        int hm1 = in_rect(x, y, Wd2 / 2 + pad / 2, modeY, mw, modeH);
        if (hs != hoverStart || hc != hoverCancel || hm0 != hoverMode[0] || hm1 != hoverMode[1]) {
            hoverStart = hs; hoverCancel = hc; hoverMode[0] = hm0; hoverMode[1] = hm1;
            RenderWindow(hwnd);
        }
        return 0;
    }
    case WM_NCHITTEST: {
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        RECT rc; GetWindowRect(hwnd, &rc);
        int x = pt.x - rc.left, y = pt.y - rc.top;
        RECT cr; GetClientRect(hwnd, &cr);
        if (y < cr.bottom * 16 / 100 && x < cr.right - 70) return HTCAPTION;
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    case WM_CHAR:
        if (exiting) return 0;
        if (!running) input_char((wchar_t)wParam);
        RenderWindow(hwnd);
        return 0;
    case WM_KEYDOWN:
        if (exiting) return 0;
        if (wParam == VK_BACK) { if (!running) input_backspace(); RenderWindow(hwnd); }
        else if (wParam == VK_RETURN) { if (!running) start_task(hwnd); }
        else if (wParam == VK_ESCAPE) {
            cancel_exit(hwnd);   /* ESC = 结束程序（运行中先中止任务） */
        }
        return 0;
    case WM_TIMER:
        if (wParam == ID_TIMER) {
            if (exiting) {
                if ((long long)time(NULL) >= exit_at) DestroyWindow(hwnd);
                return 0;
            }
            if (target_sec - (long long)time(NULL) <= 0) {
                running = 0;
                KillTimer(hwnd, ID_TIMER);
                do_shutdown();
            }
            RenderWindow(hwnd);
        }
        return 0;
    case WM_CLOSE:
        if (running) {
            int r = MessageBoxW(hwnd, L"关机任务仍在进行，退出将取消关机。\n确定退出吗？",
                                L"退出确认", MB_YESNO | MB_ICONQUESTION);
            if (r != IDYES) return 0;
            do_cancel();
        }
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

/* ===== 程序内自检 ===== */
static int selftest(void) {
    /* 构造一个 hwnd 兼容的消息窗口用于 SetTimer/调用流程校验 */
    HWND hwnd = CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED, 0, 0, 0, 0, NULL, NULL, NULL, NULL);
    int ok = 1;
    FILE *fp = _wfopen(L"ShutdownTimer-selftest.txt", L"w");
    if (!fp) fp = stdout;   /* 兜底 */

    /* 倒计时模式 */
    mode = MODE_COUNT;
    running = 0;
    if (start_task(hwnd) != 1 || running != 1) { fprintf(fp, "FAIL: count start\n"); ok = 0; }
    if (target_sec <= (long long)time(NULL)) { fprintf(fp, "FAIL: target future\n"); ok = 0; }
    cancel_task(hwnd);
    if (running != 0) { fprintf(fp, "FAIL: cancel not reset\n"); ok = 0; }

    /* 定时模式 */
    wcsncpy(bufT, L"1230", 4); mode = MODE_AT;
    if (start_task(hwnd) != 1 || running != 1) { fprintf(fp, "FAIL: at start\n"); ok = 0; }
    cancel_task(hwnd);
    if (running != 0) { fprintf(fp, "FAIL: at cancel\n"); ok = 0; }

    /* 非法输入不应启动 */
    wcsncpy(bufT, L"3", 1); mode = MODE_AT;
    if (start_task(hwnd) != 0 || running != 0) { fprintf(fp, "FAIL: invalid input\n"); ok = 0; }

    DestroyWindow(hwnd);
    fprintf(fp, ok ? "SELFTEST OK\n" : "SELFTEST FAILED\n");
    if (fp != stdout) fclose(fp);
    return ok;
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmd, int nShow) {
    (void)hPrev;
    if (cmd && cmd[0] == L'-' && wcsncmp(cmd, L"-selftest", 9) == 0) {
        return selftest() ? 0 : 1;
    }

    GdiplusStartupInput gsi; ULONG_PTR gdipToken = 0;
    ZeroMemory(&gsi, sizeof(gsi)); gsi.GdiplusVersion = 1;
    GdiplusStartup(&gdipToken, &gsi, NULL);

    WNDCLASSW wc = {0};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(NULL, IDC_HAND);
    wc.lpszClassName = L"ShutdownTimerWnd2";
    RegisterClassW(&wc);

    HDC sdc = GetDC(NULL);
    g_scale = GetDeviceCaps(sdc, LOGPIXELSX) * 100 / 96;
    ReleaseDC(NULL, sdc);
    fTitle = MakeFont(26, FW_BOLD);
    fBtn   = MakeFont(20, FW_NORMAL);
    fBig   = MakeFont(40, FW_BOLD);
    fSub   = MakeFont(18, FW_NORMAL);
    fInput = MakeFont(22, FW_NORMAL);

    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    int WX = sw / 3, WY = sh / 3;
    g_radius = 44 * g_scale / 100;
    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED, L"ShutdownTimerWnd2", L"定时关机工具",
        WS_POPUP, (sw - WX) / 2, (sh - WY) / 2, WX, WY, NULL, NULL, hInst, NULL);
    if (!hwnd) { GdiplusShutdown(gdipToken); return 1; }
    RenderWindow(hwnd);
    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    GdiplusShutdown(gdipToken);
    return 0;
}
