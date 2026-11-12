/* ============================================================
 *  航空客运订票系统 —— EasyX 图形主界面
 *  启动即进入图形窗口，所有操作在窗口中完成
 * ============================================================ */
#include "airline.h"
#include <graphics.h>
#include <tchar.h>
#include <windows.h>
#include <stdlib.h>
#include <stdarg.h>
#include <wchar.h>

#define WIN_W 1080
#define WIN_H 720

/* ---------------- 亮色主题 ---------------- */
#define C_BG        RGB(240, 245, 251)
#define C_TITLE     RGB(26, 42, 62)
#define C_SUB       RGB(92, 110, 132)
#define C_CARD      RGB(255, 255, 255)
#define C_CARD_ALT  RGB(244, 248, 253)
#define C_LINE      RGB(188, 206, 226)
#define C_BTN       RGB(219, 232, 247)
#define C_BTN_LINE  RGB(130, 168, 210)
#define C_BTN_TXT   RGB(28, 56, 90)
#define C_ACCENT    RGB(244, 196, 84)
#define C_ACCENT_L  RGB(206, 150, 20)
#define C_ACCENT_T  RGB(70, 48, 0)

/* 当前屏幕 */
enum { SC_MENU = 0, SC_QUERY, SC_LIST, SC_DETAIL, SC_ADMIN, SC_AEDIT, SC_TEXT };

/* 航班列表用途 */
enum { P_BOOK = 0, P_REFUND, P_VIEW, P_BOARD, P_DEMO };

static int      g_screen = SC_MENU;
static int      g_purpose = P_VIEW;
static Flight  *g_flight = NULL;
static int      g_cls = CLS_ECONOMY;
static int      g_sel[MAX_SEATS];
static int      g_nsel = 0;
static char     g_startFilter[MAX_CITY] = "";
static char     g_destFilter[MAX_CITY] = "";
static char     g_qStart[MAX_CITY] = "";
static char     g_qDest[MAX_CITY] = "";
static char     g_cities[24][MAX_CITY];
static int      g_cityCount = 0;
static int      g_listBack = SC_MENU;
static int      g_listTop = 0;      /* 航班列表第一行的下标 */
static int      g_afPage = 0;       /* 管理员航班选择当前页 */

static void draw_aedit(void);
static char     g_status[512] = "";
static char     g_text[65536];
static int      g_textTop = 0;
static int      g_textBack = SC_MENU;
static int      g_quit = 0;

static int      g_listIdx[MAX_FLIGHTS];
static int      g_listCount = 0;

/* ---------------- 按钮 ---------------- */
typedef struct {
    int x, y, w, h;
    char label[96];         /* 复制保存，避免指向局部缓冲区 */
    int id;
    int hi;                 /* 是否高亮（当前选中的选项） */
} Btn;

static Btn g_btns[48];
static int g_btnCount = 0;
static int g_hiA = -1;   /* 需要高亮的按钮 id（查询页起飞/到达选择） */
static int g_hiB = -1;

/* ---------------- 编码转换 ---------------- */
static void u8_to_ansi(const char *src, char *dst, int n)
{
    wchar_t w[1024];

    MultiByteToWideChar(CP_UTF8, 0, src, -1, w, 1024);
    WideCharToMultiByte(CP_ACP, 0, w, -1, dst, n, NULL, NULL);
    dst[n - 1] = '\0';
}

static void putU8(int x, int y, const char *u8)
{
    char a[1536];

    u8_to_ansi(u8, a, (int)sizeof a);
    outtextxy(x, y, a);
}

static void msg_box(const char *u8, const char *title)
{
    wchar_t w[4096], t[256];

    MultiByteToWideChar(CP_UTF8, 0, u8, -1, w, 4096);
    MultiByteToWideChar(CP_UTF8, 0, title, -1, t, 256);
    MessageBoxW(GetHWnd(), w, t, MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
}

static void raise_main_window(void)
{
    HWND h = GetHWnd();
    if (h == NULL) return;
    SetWindowPos(h, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(h);
}

/* ---------------- 自建输入对话框（支持回车确认、Esc 取消） ---------------- */
static int      g_inDone = 0;
static int      g_inOk = 0;
static wchar_t  g_inText[256];
static WNDPROC  g_inEditProc = NULL;

static LRESULT CALLBACK InEditProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_KEYDOWN) {
        if (wp == VK_RETURN) {
            PostMessageW(GetParent(h), WM_COMMAND, 1, 0);
            return 0;
        }
        if (wp == VK_ESCAPE) {
            PostMessageW(GetParent(h), WM_COMMAND, 2, 0);
            return 0;
        }
    }
    return CallWindowProcW(g_inEditProc, h, msg, wp, lp);
}

static LRESULT CALLBACK InDlgProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_COMMAND:
        if (LOWORD(wp) == 1) {
            GetWindowTextW(GetDlgItem(h, 101), g_inText, 256);
            g_inOk = 1;
            DestroyWindow(h);
        } else if (LOWORD(wp) == 2) {
            g_inOk = 0;
            DestroyWindow(h);
        }
        return 0;
    case WM_CLOSE:
        g_inOk = 0;
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        g_inDone = 1;
        return 0;
    default:
        break;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static int ask_dialog_w(const wchar_t *prompt, const wchar_t *title,
                        const wchar_t *def, wchar_t *out, int n)
{
    static int registered = 0;
    HWND parent = GetHWnd();
    HWND dlg, edit, btn;
    HFONT font;
    RECT pr;
    MSG msg;
    int x, y;

    if (!registered) {
        WNDCLASSW wc;
        memset(&wc, 0, sizeof wc);
        wc.lpfnWndProc = InDlgProc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"AirlineInputBox";
        RegisterClassW(&wc);
        registered = 1;
    }

    GetWindowRect(parent, &pr);
    x = pr.left + ((pr.right - pr.left) - 480) / 2;
    y = pr.top + ((pr.bottom - pr.top) - 210) / 2;

    dlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"AirlineInputBox", title,
                          WS_POPUP | WS_CAPTION | WS_SYSMENU,
                          x, y, 480, 210, parent, NULL,
                          GetModuleHandleW(NULL), NULL);
    if (dlg == NULL) return 0;

    font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                       DEFAULT_CHARSET, 0, 0, 0, 0, L"Microsoft YaHei");

    {
        HWND st = CreateWindowExW(0, L"STATIC", prompt, WS_CHILD | WS_VISIBLE,
                                  20, 18, 440, 40, dlg, (HMENU)100, NULL, NULL);
        SendMessageW(st, WM_SETFONT, (WPARAM)font, TRUE);

        edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", def,
                               WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                               20, 62, 440, 28, dlg, (HMENU)101, NULL, NULL);
        SendMessageW(edit, WM_SETFONT, (WPARAM)font, TRUE);
        g_inEditProc = (WNDPROC)SetWindowLongPtrW(edit, GWLP_WNDPROC,
                                                  (LONG_PTR)InEditProc);

        btn = CreateWindowExW(0, L"BUTTON", L"确定",
                              WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                              260, 116, 90, 32, dlg, (HMENU)1, NULL, NULL);
        SendMessageW(btn, WM_SETFONT, (WPARAM)font, TRUE);

        btn = CreateWindowExW(0, L"BUTTON", L"取消",
                              WS_CHILD | WS_VISIBLE,
                              360, 116, 90, 32, dlg, (HMENU)2, NULL, NULL);
        SendMessageW(btn, WM_SETFONT, (WPARAM)font, TRUE);
    }

    g_inDone = 0;
    g_inOk = 0;
    g_inText[0] = 0;

    EnableWindow(parent, FALSE);
    ShowWindow(dlg, SW_SHOW);
    SetWindowPos(dlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(dlg);
    SetFocus(edit);
    SendMessageW(edit, EM_SETSEL, 0, -1);

    while (!g_inDone) {
        if (GetMessageW(&msg, NULL, 0, 0) <= 0) break;
        if (IsDialogMessageW(dlg, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    EnableWindow(parent, TRUE);
    SetActiveWindow(parent);
    DeleteObject(font);

    if (g_inOk) {
        wcsncpy(out, g_inText, (size_t)(n - 1));
        out[n - 1] = 0;
        return 1;
    }
    return 0;
}

/* 弹出输入框；成功返回 1，结果转成 UTF-8 */
static int ask_impl(const char *prompt, const char *title, const char *def,
                    char *outU8, int outLen, int allowEmpty)
{
    wchar_t wp[512], wt[128], wd[256], wo[256];

    MultiByteToWideChar(CP_UTF8, 0, prompt, -1, wp, 512);
    MultiByteToWideChar(CP_UTF8, 0, title, -1, wt, 128);
    MultiByteToWideChar(CP_UTF8, 0, def, -1, wd, 256);
    wo[0] = 0;

    if (!ask_dialog_w(wp, wt, wd, wo, 256)) return 0;

    WideCharToMultiByte(CP_UTF8, 0, wo, -1, outU8, outLen, NULL, NULL);
    if (!allowEmpty && outU8[0] == '\0') return 0;
    return 1;
}

static int ask(const char *prompt, const char *title, const char *def,
               char *outU8, int outLen)
{
    return ask_impl(prompt, title, def, outU8, outLen, 0);
}

static int ask_int(const char *prompt, const char *title, int def)
{
    char defs[32], out[64];

    snprintf(defs, sizeof defs, "%d", def);
    if (!ask(prompt, title, defs, out, (int)sizeof out)) return def;
    return atoi(out);
}

/* 日期弹窗：显示航班近期日期按钮，并保留手动输入入口。 */
static char g_dateChoice[11];
static int g_dateDialogDone, g_dateManual;
static char g_dateOptions[14][11];
static int g_dateOptionCount;

static LRESULT CALLBACK DateDlgProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    int id = LOWORD(wp);
    (void)lp;
    if (msg == WM_COMMAND) {
        if (id >= 200 && id < 200 + g_dateOptionCount) {
            snprintf(g_dateChoice, sizeof g_dateChoice, "%s", g_dateOptions[id - 200]);
            g_dateManual = 0; g_dateDialogDone = 1; DestroyWindow(h); return 0;
        }
        if (id == 299) { g_dateManual = 1; g_dateDialogDone = 1; DestroyWindow(h); return 0; }
        if (id == 2) { g_dateManual = 0; g_dateDialogDone = 1; DestroyWindow(h); return 0; }
    }
    if (msg == WM_CLOSE) { g_dateManual = 0; g_dateDialogDone = 1; DestroyWindow(h); return 0; }
    return DefWindowProcW(h, msg, wp, lp);
}

static int choose_flight_date(const Flight *f, char *out, int outLen)
{
    static int registered;
    HWND parent = GetHWnd(), dlg;
    WNDCLASSW wc;
    RECT pr;
    MSG msg;
    time_t now = time(NULL);
    struct tm base = *localtime(&now);
    int offset, x, y;
    g_dateOptionCount = 0;
    for (offset = 0; offset <= 30 && g_dateOptionCount < 14; offset++) {
        struct tm t = base;
        int bit;
        t.tm_mday += offset; t.tm_isdst = -1; mktime(&t);
        bit = t.tm_wday == 0 ? 6 : t.tm_wday - 1;
        if (!(f->weekMask & (1u << bit))) continue;
        snprintf(g_dateOptions[g_dateOptionCount], sizeof g_dateOptions[0], "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
        g_dateOptionCount++;
    }
    if (!registered) {
        memset(&wc, 0, sizeof wc); wc.lpfnWndProc = DateDlgProc; wc.hInstance = GetModuleHandleW(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW); wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1); wc.lpszClassName = L"AirlineDatePicker";
        RegisterClassW(&wc); registered = 1;
    }
    GetWindowRect(parent, &pr); x = pr.left + ((pr.right-pr.left)-480)/2; y = pr.top + ((pr.bottom-pr.top)-400)/2;
    dlg = CreateWindowExW(WS_EX_DLGMODALFRAME|WS_EX_TOPMOST, L"AirlineDatePicker", L"选择航班日期", WS_POPUP|WS_CAPTION|WS_SYSMENU, x,y,480,400,parent,NULL,GetModuleHandleW(NULL),NULL);
    if (!dlg) return 0;
    g_dateChoice[0] = '\0';
    {
        HFONT font = CreateFontW(-16,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,0,0,L"Microsoft YaHei");
        HWND st = CreateWindowExW(0,L"STATIC",L"点击选择可飞日期，或使用手动输入：",WS_CHILD|WS_VISIBLE,18,12,430,28,dlg,(HMENU)100,NULL,NULL);
        int i;
        SendMessageW(st,WM_SETFONT,(WPARAM)font,TRUE);
        for (i=0;i<g_dateOptionCount;i++) {
            wchar_t label[32]; MultiByteToWideChar(CP_UTF8,0,g_dateOptions[i],-1,label,32);
            HWND b=CreateWindowExW(0,L"BUTTON",label,WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,20+(i%2)*215,48+(i/2)*36,195,30,dlg,(HMENU)(INT_PTR)(200+i),NULL,NULL);
            SendMessageW(b,WM_SETFONT,(WPARAM)font,TRUE);
        }
        {
            HWND b=CreateWindowExW(0,L"BUTTON",L"手动输入日期",WS_CHILD|WS_VISIBLE,20,310,195,32,dlg,(HMENU)299,NULL,NULL);
            SendMessageW(b,WM_SETFONT,(WPARAM)font,TRUE);
            b=CreateWindowExW(0,L"BUTTON",L"取消",WS_CHILD|WS_VISIBLE,335,310,90,32,dlg,(HMENU)2,NULL,NULL);
            SendMessageW(b,WM_SETFONT,(WPARAM)font,TRUE);
        }
        g_dateDialogDone=0; g_dateManual=0; EnableWindow(parent,FALSE); ShowWindow(dlg,SW_SHOW);
        SetWindowPos(dlg, HWND_TOPMOST, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW); SetForegroundWindow(dlg);
        while (!g_dateDialogDone && GetMessageW(&msg,NULL,0,0)>0) { if(IsDialogMessageW(dlg,&msg)) continue; TranslateMessage(&msg); DispatchMessageW(&msg); }
        EnableWindow(parent,TRUE); SetActiveWindow(parent); DeleteObject(font);
    }
    if (g_dateManual) return ask("请输入日期（YYYY-MM-DD）：","手动输入日期","",out,outLen);
    if (g_dateChoice[0]) { snprintf(out,(size_t)outLen,"%s",g_dateChoice); return 1; }
    return 0;
}

static void appendf(char *buf, int n, size_t *len, const char *fmt, ...)
{
    va_list ap;
    int k;

    if (*len + 1 >= (size_t)n) return;
    va_start(ap, fmt);
    k = vsnprintf(buf + *len, (size_t)n - *len, fmt, ap);
    va_end(ap);
    if (k > 0) *len += (size_t)k;
    if (*len >= (size_t)n) *len = (size_t)n - 1;
}

/* ---------------- 航空公司色牌 ---------------- */
static COLORREF airline_colref(const char *flightNo)
{
    unsigned c = airline_color(flightNo);
    return RGB((int)((c >> 16) & 0xFF), (int)((c >> 8) & 0xFF), (int)(c & 0xFF));
}

static void draw_airline_badge(int x, int y, int w, int h, const char *flightNo)
{
    char a[128];
    int tw, th;
    COLORREF c = airline_colref(flightNo);

    u8_to_ansi(airline_name(flightNo), a, (int)sizeof a);

    setfillcolor(c);
    setlinecolor(c);
    fillroundrect(x, y, x + w, y + h, 8, 8);
    settextcolor(RGB(255, 255, 255));
    settextstyle(16, 0, _T("Microsoft YaHei"));
    tw = textwidth(a);
    th = textheight(a);
    outtextxy(x + (w - tw) / 2, y + (h - th) / 2, a);
}

/* ---------------- 按钮绘制 ---------------- */
static void add_btn2(int x, int y, int w, int h, const char *label, int id, int hi)
{
    Btn *b;

    if (g_btnCount >= (int)(sizeof g_btns / sizeof g_btns[0])) return;
    b = &g_btns[g_btnCount++];
    b->x = x; b->y = y; b->w = w; b->h = h;
    snprintf(b->label, sizeof b->label, "%s", label);
    b->id = id;
    b->hi = hi;
}

static void add_btn(int x, int y, int w, int h, const char *label, int id)
{
    add_btn2(x, y, w, h, label, id, 0);
}

static void draw_btns(void)
{
    int i;

    for (i = 0; i < g_btnCount; i++) {
        Btn *b = &g_btns[i];
        char a[256];
        int tw, th;
        int active = b->hi || (b->id == g_hiA || b->id == g_hiB) ||
                     (g_screen == SC_DETAIL && g_purpose == P_BOOK &&
                      b->id == 10 + g_cls);

        u8_to_ansi(b->label, a, (int)sizeof a);

        setfillcolor(active ? C_ACCENT : C_BTN);
        setlinecolor(active ? C_ACCENT_L : C_BTN_LINE);
        fillroundrect(b->x, b->y, b->x + b->w, b->y + b->h, 10, 10);

        settextcolor(active ? C_ACCENT_T : C_BTN_TXT);
        settextstyle(20, 0, _T("Microsoft YaHei"));
        tw = textwidth(a);
        th = textheight(a);
        outtextxy(b->x + (b->w - tw) / 2, b->y + (b->h - th) / 2, a);
    }
}

static int hit_btn(int x, int y)
{
    int i;

    for (i = 0; i < g_btnCount; i++)
        if (x >= g_btns[i].x && x <= g_btns[i].x + g_btns[i].w &&
            y >= g_btns[i].y && y <= g_btns[i].y + g_btns[i].h)
            return g_btns[i].id;
    return -1;
}

/* ---------------- 列表数据 ---------------- */
static void build_list(void)
{
    int i;

    g_listCount = 0;
    for (i = 0; i < flight_count() && g_listCount < MAX_FLIGHTS; i++) {
        Flight *f = flight_at(i);
        if (g_startFilter[0] != '\0' && strstr(f->start, g_startFilter) == NULL)
            continue;
        if (g_destFilter[0] != '\0' && strstr(f->dest, g_destFilter) == NULL)
            continue;
        g_listIdx[g_listCount++] = i;
    }
}

/* 收集所有出现过的城市（起点 + 终点），用于查询页点选 */
static void build_cities(void)
{
    int i, j, k;

    g_cityCount = 0;
    for (i = 0; i < flight_count() && g_cityCount < (int)(sizeof g_cities / sizeof g_cities[0]); i++) {
        Flight *f = flight_at(i);
        for (j = 0; j < 2; j++) {
            const char *c = (j == 0) ? f->start : f->dest;
            int found = 0;
            for (k = 0; k < g_cityCount; k++)
                if (strcmp(g_cities[k], c) == 0) { found = 1; break; }
            if (!found) snprintf(g_cities[g_cityCount++], MAX_CITY, "%s", c);
        }
    }
}

static const char *purpose_name(int p)
{
    switch (p) {
    case P_BOOK:   return "订票";
    case P_REFUND: return "退票";
    case P_VIEW:   return "查看座位图";
    case P_BOARD:  return "模拟登机";
    case P_DEMO:   return "超售演示";
    default:       return "";
    }
}

/* ---------------- 菜单 ---------------- */
static void draw_menu(void)
{
    char buf[256];

    cleardevice();
    setbkmode(TRANSPARENT);

    settextcolor(C_TITLE);
    settextstyle(50, 0, _T("Microsoft YaHei"));
    putU8(70, 70, "航空客运订票系统");

    settextcolor(C_SUB);
    settextstyle(23, 0, _T("Microsoft YaHei"));
    putU8(72, 136, "数据结构课程设计  ·  订票 / 退票 / 候补 / 超售与登机调整");

    g_btnCount = 0;
    add_btn(120, 220, 380, 86, "查询航线", 1);
    add_btn(560, 220, 380, 86, "订  票",   2);
    add_btn(120, 330, 380, 86, "退  票",   3);
    add_btn(560, 330, 380, 86, "座位图",   4);
    add_btn(120, 440, 380, 86, "模拟登机", 5);
    add_btn(560, 440, 380, 86, "管理员",   6);
    add_btn(880, 620, 140, 50, "退出",     0);
    draw_btns();

    settextcolor(C_SUB);
    settextstyle(18, 0, _T("Microsoft YaHei"));
    snprintf(buf, sizeof buf, "已载入 %d 条航线", flight_count());
    putU8(72, 660, buf);
    if (g_status[0] != '\0') putU8(72, 686, g_status);
}

/* ---------------- 查询：城市选择 ---------------- */
static void draw_city_panel(const char *title, int x, const char *sel,
                            int ctrlBase, int cityBase)
{
    char buf[160];
    int i;

    settextcolor(C_TITLE);
    settextstyle(25, 0, _T("Microsoft YaHei"));
    snprintf(buf, sizeof buf, "%s：%s", title, (sel[0] != '\0') ? sel : "不限");
    putU8(x, 126, buf);

    add_btn(x, 166, 100, 34, "不限", ctrlBase);
    add_btn(x + 112, 166, 130, 34, "手动输入", ctrlBase + 1);

    for (i = 0; i < g_cityCount && i < 10; i++) {
        int row = i / 2;
        int col = i % 2;
        add_btn(x + col * 218, 220 + row * 56, 200, 46,
                g_cities[i], cityBase + i);
    }
}

static void draw_query(void)
{
    int i;

    cleardevice();
    setbkmode(TRANSPARENT);

    settextcolor(C_TITLE);
    settextstyle(36, 0, _T("Microsoft YaHei"));
    putU8(50, 28, "查询航线");

    settextcolor(C_SUB);
    settextstyle(18, 0, _T("Microsoft YaHei"));
    putU8(52, 76, "选择起飞城市和到达城市：可以直接点击，也可以手动输入；留空表示不限");

    build_cities();

    g_btnCount = 0;
    draw_city_panel("起飞城市", 50,  g_qStart, 100, 1000);
    draw_city_panel("到达城市", 570, g_qDest,  200, 2000);

    /* 高亮当前选择；空表示“不限”高亮 */
    g_hiA = (g_qStart[0] == '\0') ? 100 : -1;
    g_hiB = (g_qDest[0]  == '\0') ? 200 : -1;
    for (i = 0; i < g_cityCount && i < 10; i++) {
        if (strcmp(g_qStart, g_cities[i]) == 0) g_hiA = 1000 + i;
        if (strcmp(g_qDest,  g_cities[i]) == 0) g_hiB = 2000 + i;
    }

    if (g_cityCount > 10) {
        settextcolor(C_SUB);
        settextstyle(16, 0, _T("Microsoft YaHei"));
        putU8(52, 512, "城市较多时，可用“手动输入”指定列表之外的城市");
    }

    add_btn(420, 600, 240, 56, "查询", 104);
    add_btn(700, 600, 160, 56, "返回", 105);
    draw_btns();

    if (g_status[0] != '\0') {
        settextcolor(C_SUB);
        settextstyle(17, 0, _T("Microsoft YaHei"));
        putU8(52, 676, g_status);
    }
}

/* ---------------- 航班列表 ---------------- */
static void draw_list(void)
{
    char buf[256], week[64];
    int i;

    build_list();

    if (g_listTop > g_listCount - 15) g_listTop = g_listCount - 15;
    if (g_listTop < 0) g_listTop = 0;

    cleardevice();
    setbkmode(TRANSPARENT);

    settextcolor(C_TITLE);
    settextstyle(32, 0, _T("Microsoft YaHei"));
    snprintf(buf, sizeof buf, "选择航班 — %s", purpose_name(g_purpose));
    putU8(50, 36, buf);

    settextcolor(C_SUB);
    settextstyle(18, 0, _T("Microsoft YaHei"));
    if (g_listCount > 15) {
        int last = g_listTop + 15;
        if (last > g_listCount) last = g_listCount;
        if (g_startFilter[0] != '\0' || g_destFilter[0] != '\0')
            snprintf(buf, sizeof buf, "查询：%s → %s    共 %d 条（第 %d-%d 条）",
                     (g_startFilter[0] != '\0') ? g_startFilter : "不限",
                     (g_destFilter[0]  != '\0') ? g_destFilter  : "不限",
                     g_listCount, g_listTop + 1, last);
        else
            snprintf(buf, sizeof buf, "共 %d 条航线，第 %d-%d 条，点击一行选择",
                     g_listCount, g_listTop + 1, last);
    } else {
        if (g_startFilter[0] != '\0' || g_destFilter[0] != '\0')
            snprintf(buf, sizeof buf, "查询：%s → %s    共 %d 条",
                     (g_startFilter[0] != '\0') ? g_startFilter : "不限",
                     (g_destFilter[0]  != '\0') ? g_destFilter  : "不限",
                     g_listCount);
        else
            snprintf(buf, sizeof buf, "共 %d 条航线，点击一行选择", g_listCount);
    }
    putU8(50, 76, buf);

    for (i = 0; i < 15 && g_listTop + i < g_listCount; i++) {
        Flight *f = flight_at(g_listIdx[g_listTop + i]);
        int y = 110 + i * 36;
        char route[64];

        setfillcolor((i % 2 == 0) ? C_CARD : C_CARD_ALT);
        setlinecolor(C_LINE);
        fillroundrect(40, y, 1040, y + 32, 6, 6);

        snprintf(route, sizeof route, "%s → %s", f->start, f->dest);
        weekmask_text(f->weekMask, week, (int)sizeof week);

        draw_airline_badge(50, y + 4, 108, 24, f->flightNo);

        settextcolor(C_TITLE);
        settextstyle(19, 0, _T("Microsoft YaHei"));
        putU8(172, y + 7, f->flightNo);
        putU8(268, y + 7, route);
        settextcolor(C_SUB);
        putU8(500, y + 7, week);
        {
            char dateDesc[64], date[11]; int days;
            Flight *inv = f;
            if (next_flight_date(f, &days, dateDesc, (int)sizeof dateDesc) && strlen(dateDesc) >= 10) {
                memcpy(date, dateDesc, 10); date[10] = '\0';
                inv = flight_instance(f, date);
                if (inv == NULL) inv = f;
            }
            snprintf(buf, sizeof buf, "最近班余票  头 %d   商 %d   经 %d",
                     remaining(inv, CLS_FIRST), remaining(inv, CLS_BUSINESS), remaining(inv, CLS_ECONOMY));
        }
        putU8(740, y + 7, buf);
    }

    if (g_listCount == 0) {
        settextcolor(RGB(190, 70, 70));
        settextstyle(23, 0, _T("Microsoft YaHei"));
        putU8(60, 200, "没有匹配的航班，请更换筛选条件。");
    }

    g_btnCount = 0;
    if (g_listCount > 15) {
        add_btn(600, 650, 100, 42, "上翻", 52);
        add_btn(710, 650, 100, 42, "下翻", 53);
    }
    add_btn(900, 650, 120, 42, "返回", 51);
    draw_btns();
}

/* ---------------- 详情 / 座位图 ---------------- */
static void draw_detail(void)
{
    int selCls = (g_purpose == P_BOOK) ? g_cls : -1;

    seat_view_draw(g_flight, selCls, g_sel, g_nsel, 0, "");

    g_btnCount = 0;
    if (g_purpose == P_BOOK) {
        add_btn(640, 516, 95, 36, "头等舱", 10);
        add_btn(745, 516, 95, 36, "商务舱", 11);
        add_btn(850, 516, 95, 36, "经济舱", 12);
    }
    if (g_purpose == P_BOOK)        add_btn(640, 566, 170, 44, "订票", 20);
    else if (g_purpose == P_REFUND) add_btn(640, 566, 170, 44, "退票", 20);
    else if (g_purpose == P_BOARD)  add_btn(640, 566, 170, 44, "模拟登机", 20);

    add_btn(640, 626, 110, 36, "返回", 21);
    draw_btns();
}

/* ---------------- 管理员 ---------------- */
static void draw_admin(void)
{
    cleardevice();
    setbkmode(TRANSPARENT);

    settextcolor(C_TITLE);
    settextstyle(36, 0, _T("Microsoft YaHei"));
    putU8(70, 60, "管理员功能");

    settextcolor(C_SUB);
    settextstyle(19, 0, _T("Microsoft YaHei"));
    putU8(72, 112, "航线维护、订票记录与超售演示");

    g_btnCount = 0;
    add_btn(120, 180, 380, 76, "新建航线", 30);
    add_btn(560, 180, 380, 76, "修改乘员定额", 31);
    add_btn(120, 290, 380, 76, "修改票价", 32);
    add_btn(560, 290, 380, 76, "修改飞行日 / 飞机号", 33);
    add_btn(120, 400, 380, 76, "查看全部订票记录", 34);
    add_btn(560, 400, 380, 76, "超售演示", 35);
    add_btn(120, 540, 200, 50, "返回", 36);
    draw_btns();

    if (g_status[0] != '\0') {
        settextcolor(C_SUB);
        settextstyle(17, 0, _T("Microsoft YaHei"));
        putU8(72, 620, g_status);
    }
}

/* ---------------- 文本屏 ---------------- */
static int text_lines(void)
{
    int n = 1;
    const char *p = g_text;

    while (*p) {
        if (*p == '\n') n++;
        p++;
    }
    return n;
}

static void draw_text_screen(void)
{
    const char *p = g_text;
    int line = 0, drawn = 0;

    cleardevice();
    setbkmode(TRANSPARENT);

    settextcolor(C_TITLE);
    settextstyle(26, 0, _T("Microsoft YaHei"));
    putU8(50, 30, "操作结果 / 订票记录");

    settextstyle(18, 0, _T("Microsoft YaHei"));
    settextcolor(RGB(40, 58, 80));

    while (*p && drawn < 24) {
        const char *nl = strchr(p, '\n');
        int len = nl ? (int)(nl - p) : (int)strlen(p);
        char tmp[256];

        if (line >= g_textTop) {
            if (len > 200) len = 200;
            memcpy(tmp, p, (size_t)len);
            tmp[len] = '\0';
            putU8(56, 78 + drawn * 22, tmp);
            drawn++;
        }
        line++;
        p = nl ? nl + 1 : p + strlen(p);
    }

    g_btnCount = 0;
    add_btn(690, 650, 90, 40, "上翻", 40);
    add_btn(790, 650, 90, 40, "下翻", 41);
    add_btn(890, 650, 120, 40, "返回", 42);
    draw_btns();
}

/* ---------------- 屏幕调度 ---------------- */
static void draw_screen(void)
{
    g_hiA = -1;
    g_hiB = -1;

    switch (g_screen) {
    case SC_MENU:   draw_menu();        break;
    case SC_QUERY:  draw_query();       break;
    case SC_LIST:   draw_list();        break;
    case SC_DETAIL: draw_detail();      break;
    case SC_ADMIN:  draw_admin();       break;
    case SC_AEDIT:  draw_aedit();       break;
    case SC_TEXT:   draw_text_screen(); break;
    default: break;
    }
}

/* ---------------- 业务操作 ---------------- */
static void toggle_seat(int idx)
{
    int i;

    for (i = 0; i < g_nsel; i++) {
        if (g_sel[i] == idx) {
            for (; i < g_nsel - 1; i++) g_sel[i] = g_sel[i + 1];
            g_nsel--;
            return;
        }
    }
    if (g_nsel < MAX_SEATS) g_sel[g_nsel++] = idx;
}

static void do_book(void)
{
    char name[MAX_NAME], id[MAX_ID], msg[512];
    int qty, r, action, i, nrec;
    Flight *rec[3];

    if (g_nsel > 0) {
        qty = g_nsel;
    } else {
        qty = ask_int("订票张数：", "订票", 1);
        if (qty < 1 || qty > MAX_SEATS) {
            msg_box("订票张数不合法。", "订票");
            return;
        }
    }

    if (!ask("姓名：", "订票", "", name, (int)sizeof name)) return;
    if (!ask("身份证号：", "订票", "", id, (int)sizeof id)) return;
    for (;;) {
        r = book_ticket(g_flight, name, id, g_cls, qty, g_flight->serviceDate,
                        (g_nsel > 0) ? g_sel : NULL, g_nsel,
                        msg, (int)sizeof msg);
        if (r == 1) {
            msg_box(msg, "订票成功"); g_nsel = 0;
            snprintf(g_status, sizeof g_status, "订票成功：%s %s %d 张", g_flight->flightNo, class_name(g_cls), qty);
            return;
        }
        if (r != -1) { msg_box(msg, "订票结果"); return; }

        {
            char recText[2048] = "";
            size_t used = 0;
            nrec = recommend_flights(g_flight, g_flight->serviceDate, g_cls, qty, rec, 3);
            for (i = 0; i < nrec; i++) {
                char row[256];
                snprintf(row, sizeof row, "\n%s  %s→%s  同日余票：%d", rec[i]->flightNo, rec[i]->start, rec[i]->dest,
                         remaining(flight_instance(rec[i], g_flight->serviceDate), g_cls));
                used += (size_t)snprintf(recText + used, sizeof recText - used, "%s", row);
            }
            if (nrec == 0) snprintf(recText, sizeof recText, "\n没有同日有足够余票的同目的地航班。");
            {
                char prompt[2600];
                snprintf(prompt, sizeof prompt, "%s%s\n\n1 修改订票张数并重试\n2 按当前要求登记候补\n0 结束", msg, recText);
                action = ask_int(prompt, "余票不足", 0);
            }
        }
        if (action == 1) {
            do {
                qty = ask_int("请输入新的订票张数（1 至 8）：", "重新订票", qty);
                if (qty < 1 || qty > MAX_SEATS) msg_box("订票张数须为 1 至 8。", "重新订票");
            } while (qty < 1 || qty > MAX_SEATS);
            g_nsel = 0; /* 数量改变后清除原座位选择 */
            continue;
        }
        if (action == 2) {
            if (add_waitlist(g_flight, name, id, g_cls, qty, g_flight->serviceDate))
                msg_box("已按登记顺序加入该航班、该日期和舱位的候补队列。", "候补登记");
            else msg_box("候补登记失败，请检查订票张数。", "候补登记");
        }
        return;
    }
}

static void do_refund(void)
{
    char name[MAX_NAME], id[MAX_ID], msg[256], wbuf[2048];
    int q = 0, n, cls = -1, cursor = 0, candidate, candidateQty, available;

    if (!ask("姓名：", "退票", "", name, (int)sizeof name)) return;
    if (!ask("身份证号：", "退票", "", id, (int)sizeof id)) return;
    if (!refund_ticket(g_flight, name, id, g_flight->serviceDate, &q, &cls, msg, (int)sizeof msg)) {
        msg_box(msg, "退票");
        return;
    }

    msg_box(msg, "退票成功");
    available = q;
    while ((n = process_waitlist(g_flight, g_flight->serviceDate, cls, available, cursor,
                                 &candidate, &candidateQty, wbuf, (int)sizeof wbuf)) > 0) {
        char answer[16], result[256];
        if (!ask(wbuf, "候补旅客确认", "y", answer, (int)sizeof answer)) break;
        if (answer[0] == 'y' || answer[0] == 'Y') available -= candidateQty;
        confirm_waiter(g_flight, candidate, answer[0] == 'y' || answer[0] == 'Y', result, (int)sizeof result);
        msg_box(result, "候补处理结果");
        cursor = candidate;
    }
    if (n == 0 && wbuf[0]) msg_box(wbuf, "候补处理");
    snprintf(g_status, sizeof g_status, "退票成功：%s %d 张", name, q);
}

static void do_board(void)
{
    simulate_boarding(g_flight, g_text, (int)sizeof g_text);
    g_textTop = 0;
    g_textBack = SC_DETAIL;
    g_screen = SC_TEXT;
}

static void do_demo(Flight *f)
{
    int cls;
    char buf[512];

    cls = ask_int("舱位（0 头等 / 1 商务 / 2 经济）：", "超售演示", 2);
    if (cls < 0 || cls >= CLASS_CNT) return;

    demo_overbook(f, cls, buf, (int)sizeof buf);
    msg_box(buf, "超售演示");
    snprintf(g_status, sizeof g_status, "%s", buf);
}

/* ---------------- 订票记录 ---------------- */
static void build_records(void)
{
    size_t len = 0;
    int i, total = flight_count() + flight_instance_count();

    g_text[0] = '\0';
    for (i = 0; i < total; i++) {
        Flight *f = (i < flight_count()) ? flight_at(i) : flight_instance_at(i - flight_count());
        Booking *b;
        Waiter *w;

        appendf(g_text, (int)sizeof g_text, &len,
                "===== %s  %s → %s  %s =====\n",
                f->flightNo, f->start, f->dest, f->serviceDate[0] ? f->serviceDate : "航线模板");

        if (f->bookings == NULL) {
            appendf(g_text, (int)sizeof g_text, &len, "  （无订票记录）\n");
        } else {
            for (b = f->bookings; b != NULL; b = b->next) {
                char seatbuf[160];
                int k, first = 1;
                size_t sl = 0;
                char lab[16];

                seatbuf[0] = '\0';
                for (k = 0; k < b->nseat; k++) {
                    if (b->seats[k] == SEAT_NONE) continue;
                    seat_label(f, b->cls, b->seats[k], lab);
                    sl += (size_t)snprintf(seatbuf + sl, sizeof seatbuf - sl,
                                           first ? "%s" : "、%s", lab);
                    first = 0;
                }
                if (first) snprintf(seatbuf, sizeof seatbuf, "无固定座位");

                appendf(g_text, (int)sizeof g_text, &len,
                        "  #%-3d %-10s %-20s %-10s %-8s %d 张  %s\n",
                        b->seq, b->name, b->id, b->date, class_name(b->cls), b->qty, seatbuf);
            }
        }

        if (f->waitlist != NULL) {
            appendf(g_text, (int)sizeof g_text, &len, "  候补队列：\n");
            for (w = f->waitlist; w != NULL; w = w->next)
                appendf(g_text, (int)sizeof g_text, &len,
                        "    %-10s %-20s %-10s %-8s %d 张\n",
                        w->name, w->id, w->date, class_name(w->cls), w->qty);
        }
        appendf(g_text, (int)sizeof g_text, &len, "\n");
    }
}

/* ---------------- 管理员：表单式编辑 ---------------- */
enum { AF_NEW = 0, AF_CAP, AF_PRICE, AF_SCHED, AF_DEMO };

static int  g_afMode = AF_NEW;
static char g_afFlight[MAX_NO] = "";
static char g_afStart[MAX_CITY] = "";
static char g_afDest[MAX_CITY] = "";
static char g_afPlane[MAX_NO] = "B738";
static char g_afNo[MAX_NO] = "";
static char g_afAirline[8] = "";
static char g_afPrice[CLASS_CNT][16] = { "1000", "1000", "1000" };
static char g_afCap[16] = "8";
static int  g_afWeek = 0x7F;
static int  g_afCls = CLS_ECONOMY;

static Flight *af_flight(void)
{
    return (g_afFlight[0] != '\0') ? find_flight(g_afFlight) : NULL;
}

static void af_load(Flight *f)
{
    int c;

    snprintf(g_afFlight, MAX_NO, "%s", f->flightNo);
    snprintf(g_afPlane, MAX_NO, "%s", f->planeNo);
    g_afWeek = (int)f->weekMask;
    for (c = 0; c < CLASS_CNT; c++)
        snprintf(g_afPrice[c], sizeof g_afPrice[c], "%.0f", f->price[c]);
    snprintf(g_afCap, sizeof g_afCap, "%d", f->cap[g_afCls]);
}

static void af_open(int mode)
{
    int c;

    g_afMode = mode;
    g_afFlight[0] = '\0';
    g_afStart[0] = '\0';
    g_afDest[0] = '\0';
    g_afNo[0] = '\0';
    snprintf(g_afPlane, MAX_NO, "B738");
    g_afWeek = 0x7F;
    g_afCls = CLS_ECONOMY;
    for (c = 0; c < CLASS_CNT; c++)
        snprintf(g_afPrice[c], sizeof g_afPrice[c], "1000");
    snprintf(g_afCap, sizeof g_afCap, "8");

    g_afPage = 0;
    if (mode != AF_NEW) {
        Flight *f = g_flight;
        int idx;
        if (f == NULL && flight_count() > 0) f = flight_at(0);
        if (f != NULL) {
            af_load(f);
            for (idx = 0; idx < flight_count(); idx++)
                if (flight_at(idx) == f) { g_afPage = idx / 15; break; }
        }
    }
    g_screen = SC_AEDIT;
}

static void af_title(char *buf, int n)
{
    switch (g_afMode) {
    case AF_NEW:   snprintf(buf, (size_t)n, "新建航线"); break;
    case AF_CAP:   snprintf(buf, (size_t)n, "修改乘员定额"); break;
    case AF_PRICE: snprintf(buf, (size_t)n, "修改票价"); break;
    case AF_SCHED: snprintf(buf, (size_t)n, "修改飞行日 / 飞机号"); break;
    case AF_DEMO:  snprintf(buf, (size_t)n, "超售演示"); break;
    default:       snprintf(buf, (size_t)n, "管理员"); break;
    }
}

static void af_label(int x, int y, const char *text)
{
    settextcolor(C_TITLE);
    settextstyle(19, 0, _T("Microsoft YaHei"));
    putU8(x, y, text);
}

static void af_city_row(const char *label, const char *sel, int y,
                        int base, int manualId)
{
    int i, n, shown, selInList = 0;

    af_label(50, y + 8, label);
    shown = (g_cityCount < 8) ? g_cityCount : 8;
    for (i = 0; i < shown; i++) {
        if (strcmp(sel, g_cities[i]) == 0) selInList = 1;
        add_btn2(160 + i * 84, y, 80, 34, g_cities[i], base + i,
                 strcmp(sel, g_cities[i]) == 0);
    }
    n = shown;
    /* 手动输入的城市不在列表里时，单独显示成一个金色按钮 */
    if (sel[0] != '\0' && !selInList) {
        add_btn2(160 + n * 84, y, 80, 34, sel, -1, 1);
        n++;
    }
    add_btn(160 + n * 84, y, 110, 34, "手动输入", manualId);
}

static void af_flight_chips(void)
{
    int i, start, end, slot, totalPages;

    if (g_afPage < 0) g_afPage = 0;
    totalPages = (flight_count() + 14) / 15;
    if (totalPages < 1) totalPages = 1;
    if (g_afPage >= totalPages) g_afPage = totalPages - 1;

    start = g_afPage * 15;
    end = start + 15;
    if (end > flight_count()) end = flight_count();

    for (i = start; i < end; i++) {
        Flight *f = flight_at(i);
        char lab[80];
        slot = i - start;
        snprintf(lab, sizeof lab, "%s %s→%s", f->flightNo, f->start, f->dest);
        add_btn2(50 + (slot % 3) * 330, 108 + (slot / 3) * 46, 310, 38, lab,
                 3000 + i, strcmp(g_afFlight, f->flightNo) == 0);
    }
    add_btn(50, 108 + 5 * 46, 230, 36, "手动输入航班号", 3204);

    if (flight_count() > 15) {
        char buf[64];
        add_btn(300, 108 + 5 * 46, 110, 36, "上一页", 3400);
        add_btn(420, 108 + 5 * 46, 110, 36, "下一页", 3401);
        snprintf(buf, sizeof buf, "第 %d/%d 页", g_afPage + 1, totalPages);
        settextcolor(C_SUB);
        settextstyle(17, 0, _T("Microsoft YaHei"));
        putU8(550, 108 + 5 * 46 + 10, buf);
    }
}

static void af_class_chips(int y)
{
    int c;

    af_label(50, y + 8, "舱位：");
    for (c = 0; c < CLASS_CNT; c++)
        add_btn2(160 + c * 118, y, 108, 34, class_name(c), 3180 + c,
                 g_afCls == c);
}

static void af_week_chips(int y)
{
    static const char *wd[] = { "周一", "周二", "周三", "周四", "周五", "周六", "周日" };
    int i;

    af_label(50, y + 8, "飞行日：");
    for (i = 0; i < 7; i++)
        add_btn2(160 + i * 78, y, 70, 34, wd[i], 3160 + i,
                 (g_afWeek >> i) & 1);
    add_btn2(160 + 7 * 78, y, 80, 34, "每天", 3170,
             (g_afWeek & 0x7F) == 0x7F);
}

static void af_plane_chips(int y)
{
    static const char *pl[] = { "B738", "A320", "A321", "A330", "B787" };
    int i;

    af_label(50, y + 8, "飞机号：");
    for (i = 0; i < 5; i++)
        add_btn2(160 + i * 98, y, 90, 34, pl[i], 3140 + i,
                 strcmp(g_afPlane, pl[i]) == 0);
    add_btn(160 + 5 * 98, y, 110, 34, "手动输入", 3203);
}

static void af_price_buttons(int y)
{
    int c;

    af_label(50, y + 8, "票价：");
    for (c = 0; c < CLASS_CNT; c++) {
        char lab[64];
        snprintf(lab, sizeof lab, "%s ￥%s", class_name(c), g_afPrice[c]);
        add_btn(160 + c * 230, y, 210, 36, lab, 3205 + c);
    }
}

static void af_airline_chips(int y)
{
    int i, n = airline_total();

    af_label(50, y + 8, "航空公司：");
    for (i = 0; i < n && i < 7; i++) {
        char lab[48];
        snprintf(lab, sizeof lab, "%s %s", airline_name_at(i), airline_code_at(i));
        add_btn2(160 + i * 128, y, 120, 34, lab, 3500 + i,
                 (g_afAirline[0] != '\0' &&
                  strcmp(g_afAirline, airline_code_at(i)) == 0));
    }
}

static void draw_aedit(void)
{
    char title[64];

    g_btnCount = 0;
    build_cities();

    cleardevice();
    setbkmode(TRANSPARENT);

    af_title(title, (int)sizeof title);
    settextcolor(C_TITLE);
    settextstyle(34, 0, _T("Microsoft YaHei"));
    putU8(50, 24, title);

    settextcolor(C_SUB);
    settextstyle(17, 0, _T("Microsoft YaHei"));
    putU8(52, 70, "选项可直接点击，也可以点“手动输入”自己填写；金色表示当前选择");

    if (g_afMode == AF_NEW) {
        af_city_row("起飞城市：", g_afStart, 104, 3100, 3200);
        af_city_row("到达城市：", g_afDest, 158, 3120, 3201);

        af_label(50, 220, "航班号：");
        add_btn(160, 212, 260, 36,
                (g_afNo[0] != '\0') ? g_afNo : "点击输入航班号", 3202);

        af_airline_chips(264);
        af_plane_chips(318);
        af_week_chips(372);
        af_price_buttons(426);
    } else {
        af_flight_chips();

        if (g_afMode == AF_CAP) {
            char lab[80];
            af_class_chips(400);
            snprintf(lab, sizeof lab, "当前定额：%s（点击输入）", g_afCap);
            add_btn(160, 448, 320, 38, lab, 3208);
        } else if (g_afMode == AF_PRICE) {
            char lab[80];
            af_class_chips(400);
            snprintf(lab, sizeof lab, "当前票价：￥%s（点击修改）", g_afPrice[g_afCls]);
            add_btn(160, 448, 320, 38, lab, 3210);
        } else if (g_afMode == AF_SCHED) {
            af_week_chips(400);
            af_plane_chips(454);
        } else { /* AF_DEMO */
            af_class_chips(400);
        }
    }

    if (g_afMode == AF_NEW)
        add_btn(700, 640, 180, 46, "创建航线", 3300);
    else if (g_afMode == AF_DEMO)
        add_btn(700, 640, 180, 46, "开始演示", 3300);
    else
        add_btn(700, 640, 180, 46, "确定修改", 3300);

    add_btn(900, 640, 120, 46, "返回", 3301);
    draw_btns();
}

static void af_confirm(void)
{
    char msg[256];
    Flight *f;

    switch (g_afMode) {
    case AF_NEW:
        {
            int cap[CLASS_CNT] = { 8, 24, 60 };
            double price[CLASS_CNT];
            int i;

            if (g_afStart[0] == '\0' || g_afDest[0] == '\0' || g_afNo[0] == '\0') {
                msg_box("请填写起飞城市、到达城市和航班号（可点击或手动输入）。", "新建航线");
                return;
            }
            if (strlen(g_afNo) < 3) {
                msg_box("请补全航班号（航空公司代码 + 数字，如 CZ3101）。", "新建航线");
                return;
            }
            if (g_afWeek == 0) {
                msg_box("请至少选择一个飞行日。", "新建航线");
                return;
            }
            for (i = 0; i < CLASS_CNT; i++) {
                price[i] = atof(g_afPrice[i]);
                if (price[i] <= 0.0) price[i] = 1.0;
            }
            f = admin_new_flight(g_afStart, g_afDest, g_afNo, g_afPlane,
                                 (unsigned)g_afWeek, cap, price);
            if (f != NULL) {
                msg_box("航线创建成功。", "新建航线");
                snprintf(g_status, sizeof g_status, "已新建 %s %s→%s",
                         g_afNo, g_afStart, g_afDest);
                g_screen = SC_ADMIN;
            } else {
                msg_box("创建失败：航班号重复、航线已满或参数不合法。", "新建航线");
            }
        }
        break;

    case AF_CAP:
        f = af_flight();
        if (f == NULL) { msg_box("请先点击选择航班。", "修改乘员定额"); return; }
        {
            char date[11];
            Flight *inst;

            /* 定额和已售都记录在按日期的航班实例上，先选日期再修改 */
            if (!choose_flight_date(f, date, (int)sizeof date)) return;
            inst = flight_instance(f, date);
            if (inst == NULL) {
                msg_box("无法创建该日期的航班库存。", "修改乘员定额");
                return;
            }
            if (admin_set_capacity(inst, g_afCls, atoi(g_afCap), msg, (int)sizeof msg)) {
                char full[320];
                snprintf(full, sizeof full, "%s（%s）", msg, date);
                msg_box(full, "修改成功");
                snprintf(g_status, sizeof g_status, "%s", full);
                g_screen = SC_ADMIN;
            } else {
                msg_box(msg, "修改失败");
            }
        }
        break;

    case AF_PRICE:
        f = af_flight();
        if (f == NULL) { msg_box("请先点击选择航班。", "修改票价"); return; }
        f->price[g_afCls] = atof(g_afPrice[g_afCls]);
        if (f->price[g_afCls] <= 0.0) f->price[g_afCls] = 1.0;
        snprintf(g_status, sizeof g_status, "%s 的%s票价已改为 ￥%.0f",
                 f->flightNo, class_name(g_afCls), f->price[g_afCls]);
        msg_box(g_status, "修改票价");
        g_screen = SC_ADMIN;
        break;

    case AF_SCHED:
        f = af_flight();
        if (f == NULL) { msg_box("请先点击选择航班。", "修改航班"); return; }
        if (g_afWeek == 0) { msg_box("请至少选择一个飞行日。", "修改航班"); return; }
        if (strcmp(f->planeNo, g_afPlane) != 0) {
            Flight tmp = *f;
            int c2;

            if (f->bookings != NULL) {
                msg_box("该航班已有订票记录，不能更换机型（座位号会错乱）。请先办理退票。", "修改航班");
                return;
            }
            snprintf(tmp.planeNo, MAX_NO, "%.*s", MAX_NO - 1, g_afPlane);
            flight_apply_plane(&tmp);
            for (c2 = 0; c2 < CLASS_CNT; c2++) {
                if (f->cap[c2] > MAX_ROWS * tmp.cols[c2]) {
                    msg_box("该航班的定额超出新机型的座位上限，请先调小定额再换机型。", "修改航班");
                    return;
                }
            }
        }
        f->weekMask = (unsigned)g_afWeek;
        snprintf(f->planeNo, MAX_NO, "%.*s", MAX_NO - 1, g_afPlane);
        flight_apply_plane(f);
        msg_box("航班信息已更新。", "修改航班");
        g_screen = SC_ADMIN;
        break;

    case AF_DEMO:
        f = af_flight();
        if (f == NULL) { msg_box("请先点击选择航班。", "超售演示"); return; }
        {
            Flight *route = f;
            char date[11];
            if (!choose_flight_date(route, date, (int)sizeof date)) return;
            f = flight_instance(route, date);
            if (f == NULL) { msg_box("无法创建该日期的航班库存。", "超售演示"); return; }
        }
        demo_overbook(f, g_afCls, msg, (int)sizeof msg);
        msg_box(msg, "超售演示");
        snprintf(g_status, sizeof g_status, "%s", msg);
        g_screen = SC_ADMIN;
        break;

    default:
        break;
    }
}

/* ---------------- 按钮事件 ---------------- */
static void on_button(int id)
{
    switch (id) {
    case 0:  g_quit = 1; break;

    /* 主菜单 */
    case 1:
        g_screen = SC_QUERY;
        break;
    case 2:
        g_purpose = P_BOOK;
        g_startFilter[0] = g_destFilter[0] = '\0';
        g_listBack = SC_MENU;
        g_screen = SC_LIST;
        break;
    case 3:
        g_purpose = P_REFUND;
        g_startFilter[0] = g_destFilter[0] = '\0';
        g_listBack = SC_MENU;
        g_screen = SC_LIST;
        break;
    case 4:
        g_purpose = P_VIEW;
        g_startFilter[0] = g_destFilter[0] = '\0';
        g_listBack = SC_MENU;
        g_screen = SC_LIST;
        break;
    case 5:
        g_purpose = P_BOARD;
        g_startFilter[0] = g_destFilter[0] = '\0';
        g_listBack = SC_MENU;
        g_screen = SC_LIST;
        break;
    case 6: g_screen = SC_ADMIN;  break;

    /* 列表 */
    case 51: g_screen = g_listBack; break;
    case 52:
        g_listTop -= 15;
        if (g_listTop < 0) g_listTop = 0;
        break;
    case 53:
        g_listTop += 15;
        if (g_listTop > g_listCount - 15)
            g_listTop = (g_listCount > 15) ? g_listCount - 15 : 0;
        break;

    /* 查询页：不限 / 手动输入 */
    case 100: g_qStart[0] = '\0'; break;
    case 101:
        {
            char c[MAX_CITY];
            if (ask("请输入起飞城市：", "起飞城市", g_qStart, c, (int)sizeof c))
                snprintf(g_qStart, sizeof g_qStart, "%s", c);
        }
        break;
    case 200: g_qDest[0] = '\0'; break;
    case 201:
        {
            char c[MAX_CITY];
            if (ask("请输入到达城市：", "到达城市", g_qDest, c, (int)sizeof c))
                snprintf(g_qDest, sizeof g_qDest, "%s", c);
        }
        break;

    /* 查询页：执行查询 / 返回 */
    case 104:
        snprintf(g_startFilter, sizeof g_startFilter, "%s", g_qStart);
        snprintf(g_destFilter,  sizeof g_destFilter,  "%s", g_qDest);
        g_purpose = P_VIEW;
        g_listBack = SC_QUERY;
        g_screen = SC_LIST;
        break;
    case 105: g_screen = SC_MENU; break;

    /* 详情 */
    case 10: case 11: case 12:
        g_cls = id - 10;
        g_nsel = 0;
        break;
    case 20:
        if (g_purpose == P_BOOK)        do_book();
        else if (g_purpose == P_REFUND) do_refund();
        else if (g_purpose == P_BOARD)  do_board();
        break;
    case 21: g_screen = SC_LIST; break;

    /* 管理员 */
    case 30: af_open(AF_NEW);   break;
    case 31: af_open(AF_CAP);   break;
    case 32: af_open(AF_PRICE); break;
    case 33: af_open(AF_SCHED); break;
    case 34:
        build_records();
        g_textTop = 0;
        g_textBack = SC_ADMIN;
        g_screen = SC_TEXT;
        break;
    case 35: af_open(AF_DEMO);  break;
    case 36: g_screen = SC_MENU; break;

    /* 管理员表单：手动输入按钮 */
    case 3170: g_afWeek = 0x7F; break;
    case 3200:
        {
            char c[MAX_CITY];
            if (ask("起飞城市：", "新建航线", g_afStart, c, (int)sizeof c))
                snprintf(g_afStart, sizeof g_afStart, "%s", c);
        }
        break;
    case 3201:
        {
            char c[MAX_CITY];
            if (ask("到达城市：", "新建航线", g_afDest, c, (int)sizeof c))
                snprintf(g_afDest, sizeof g_afDest, "%s", c);
        }
        break;
    case 3202:
        {
            char s[MAX_NO];
            if (ask("航班号：", "新建航线", g_afNo, s, (int)sizeof s)) {
                int k, known = 0;

                if (g_afAirline[0] != '\0') {
                    for (k = 0; k < airline_total(); k++)
                        if (strncmp(s, airline_code_at(k), 2) == 0) { known = 1; break; }
                    if (!known) {
                        char tmp[MAX_NO];
                        snprintf(tmp, sizeof tmp, "%s%s", g_afAirline, s);
                        snprintf(s, sizeof s, "%s", tmp);
                    }
                }
                for (k = 0; k < airline_total(); k++) {
                    if (strncmp(s, airline_code_at(k), 2) == 0) {
                        snprintf(g_afAirline, sizeof g_afAirline, "%s", airline_code_at(k));
                        break;
                    }
                }
                snprintf(g_afNo, sizeof g_afNo, "%s", s);
            }
        }
        break;
    case 3203:
        {
            char s[MAX_NO];
            if (ask("飞机号：", "修改航班", g_afPlane, s, (int)sizeof s))
                snprintf(g_afPlane, sizeof g_afPlane, "%s", s);
        }
        break;
    case 3204:
        {
            char s[MAX_NO];
            Flight *f;
            if (!ask("请输入航班号：", "选择航班", g_afFlight, s, (int)sizeof s)) break;
            f = find_flight(s);
            if (f != NULL) af_load(f);
            else msg_box("航班号不存在。", "提示");
        }
        break;
    case 3205:
    case 3206:
    case 3207:
        {
            int i = id - 3205;
            char s[16], prompt[48];
            snprintf(prompt, sizeof prompt, "%s票价：", class_name(i));
            if (ask(prompt, "票价", g_afPrice[i], s, (int)sizeof s))
                snprintf(g_afPrice[i], sizeof g_afPrice[i], "%s", s);
        }
        break;
    case 3208:
        {
            char s[16];
            if (ask("新定额：", "修改乘员定额", g_afCap, s, (int)sizeof s))
                snprintf(g_afCap, sizeof g_afCap, "%s", s);
        }
        break;
    case 3210:
        {
            char s[16], prompt[48];
            snprintf(prompt, sizeof prompt, "新%s票价：", class_name(g_afCls));
            if (ask(prompt, "修改票价", g_afPrice[g_afCls], s, (int)sizeof s))
                snprintf(g_afPrice[g_afCls], sizeof g_afPrice[g_afCls], "%s", s);
        }
        break;
    case 3300: af_confirm(); break;
    case 3301: g_screen = SC_ADMIN; break;
    case 3400:
        if (g_afPage > 0) g_afPage--;
        break;
    case 3401:
        if ((g_afPage + 1) * 15 < flight_count()) g_afPage++;
        break;

    /* 文本屏 */
    case 40:
        if (g_textTop > 0) g_textTop--;
        break;
    case 41:
        if (g_textTop + 24 < text_lines()) g_textTop++;
        break;
    case 42:
        g_screen = g_textBack;
        break;

    default:
        /* 查询页城市按钮：起飞 1000+，到达 2000+ */
        if (id >= 1000 && id < 1010 && id - 1000 < g_cityCount) {
            int i = id - 1000;
            if (strcmp(g_qStart, g_cities[i]) == 0) g_qStart[0] = '\0';
            else snprintf(g_qStart, sizeof g_qStart, "%s", g_cities[i]);
        } else if (id >= 2000 && id < 2010 && id - 2000 < g_cityCount) {
            int i = id - 2000;
            if (strcmp(g_qDest, g_cities[i]) == 0) g_qDest[0] = '\0';
            else snprintf(g_qDest, sizeof g_qDest, "%s", g_cities[i]);
        }
        /* 管理员表单：航班 / 城市 / 飞机号 / 飞行日 / 舱位 */
        else if (id >= 3000 && id < 3015 && id - 3000 < flight_count()) {
            af_load(flight_at(id - 3000));
        } else if (id >= 3100 && id < 3110 &&
                   id - 3100 < g_cityCount) {
            snprintf(g_afStart, sizeof g_afStart, "%s", g_cities[id - 3100]);
        } else if (id >= 3120 && id < 3130 &&
                   id - 3120 < g_cityCount) {
            snprintf(g_afDest, sizeof g_afDest, "%s", g_cities[id - 3120]);
        } else if (id >= 3140 && id < 3145) {
            static const char *pl[] = { "B738", "A320", "A321", "A330", "B787" };
            snprintf(g_afPlane, sizeof g_afPlane, "%s", pl[id - 3140]);
        } else if (id >= 3160 && id < 3167) {
            g_afWeek ^= (1 << (id - 3160));
        } else if (id >= 3500 && id < 3507 && id - 3500 < airline_total()) {
            const char *code = airline_code_at(id - 3500);
            snprintf(g_afAirline, sizeof g_afAirline, "%s", code);
            if (g_afNo[0] == '\0') {
                snprintf(g_afNo, sizeof g_afNo, "%s", code);
            } else if (strncmp(g_afNo, code, 2) != 0) {
                int k, known = 0;
                char tmp[MAX_NO];
                for (k = 0; k < airline_total(); k++)
                    if (strncmp(g_afNo, airline_code_at(k), 2) == 0) { known = 1; break; }
                if (known) snprintf(tmp, sizeof tmp, "%s%s", code, g_afNo + 2);
                else       snprintf(tmp, sizeof tmp, "%s%s", code, g_afNo);
                snprintf(g_afNo, sizeof g_afNo, "%s", tmp);
            }
        } else if (id >= 3180 && id < 3183) {
            Flight *f;
            g_afCls = id - 3180;
            f = af_flight();
            if (f != NULL)
                snprintf(g_afCap, sizeof g_afCap, "%d", f->cap[g_afCls]);
        }
        break;
    }
}

/* ---------------- 鼠标 / 键盘 ---------------- */
static void handle_click(int x, int y)
{
    int id = hit_btn(x, y);

    if (id >= 0) {
        on_button(id);
        return;
    }

    if (g_screen == SC_LIST) {
        int row = (y - 110) / 36;
        int idx = g_listTop + row;

        if (y >= 110 && row >= 0 && row < 15 &&
            x >= 40 && x <= 1040 && idx >= 0 && idx < g_listCount) {
            Flight *route = flight_at(g_listIdx[idx]);
            char date[11];
            g_nsel = 0;
            if (!choose_flight_date(route, date, (int)sizeof date)) return;
            g_flight = flight_instance(route, date);
            if (g_flight == NULL) { msg_box("无法创建该日期的航班库存。", "日期选择"); return; }
            if (g_purpose == P_DEMO) {
                do_demo(g_flight);
            } else {
                g_screen = SC_DETAIL;
            }
        }
        return;
    }

    if (g_screen == SC_DETAIL && g_purpose == P_BOOK && g_flight != NULL) {
        int cls, idx = -1, hitCls = -1;

        /* 三个舱位的座位都可以点；点到哪个舱就自动切到哪个舱 */
        for (cls = 0; cls < CLASS_CNT; cls++) {
            int t = seat_view_hit(g_flight, cls, x, y);
            if (t >= 0) {
                idx = t;
                hitCls = cls;
                break;
            }
        }

        if (idx >= 0) {
            int cols = class_cols(g_flight, hitCls);

            if (hitCls != g_cls) {
                g_cls = hitCls;     /* 自动高光到被点击的舱位 */
                g_nsel = 0;         /* 一次订票只属于一个舱位 */
            }
            if (!g_flight->taken[hitCls][idx / cols][idx % cols])
                toggle_seat(idx);
        }
    }
}

static void handle_key(int vk)
{
    if (vk == VK_ESCAPE) {
        switch (g_screen) {
        case SC_MENU:   g_quit = 1; break;
        case SC_QUERY:  g_screen = SC_MENU; break;
        case SC_LIST:   g_screen = g_listBack; break;
        case SC_DETAIL: g_screen = SC_LIST; break;
        case SC_ADMIN:  g_screen = SC_MENU; break;
        case SC_AEDIT:  g_screen = SC_ADMIN; break;
        case SC_TEXT:   g_screen = g_textBack; break;
        }
        return;
    }

    if (g_screen == SC_LIST) {
        if (vk == VK_UP) {
            if (g_listTop > 0) g_listTop--;
        } else if (vk == VK_DOWN) {
            if (g_listTop + 15 < g_listCount) g_listTop++;
        } else if (vk == VK_PRIOR) {
            g_listTop -= 15;
            if (g_listTop < 0) g_listTop = 0;
        } else if (vk == VK_NEXT) {
            g_listTop += 15;
            if (g_listTop > g_listCount - 15)
                g_listTop = (g_listCount > 15) ? g_listCount - 15 : 0;
        }
        return;
    }

    if (g_screen == SC_TEXT) {
        if (vk == VK_UP && g_textTop > 0) g_textTop--;
        else if (vk == VK_DOWN && g_textTop + 24 < text_lines()) g_textTop++;
        else if (vk == VK_PRIOR) g_textTop -= 20;
        else if (vk == VK_NEXT)  g_textTop += 20;
        if (g_textTop < 0) g_textTop = 0;
    }
}

static void handle_wheel(int delta)
{
    int step = (delta > 0) ? -3 : 3;

    if (g_screen == SC_LIST) {
        g_listTop += step;
        if (g_listTop < 0) g_listTop = 0;
        if (g_listTop > g_listCount - 15)
            g_listTop = (g_listCount > 15) ? g_listCount - 15 : 0;
    } else if (g_screen == SC_TEXT) {
        int maxTop = text_lines() - 24;
        if (maxTop < 0) maxTop = 0;
        g_textTop += step;
        if (g_textTop < 0) g_textTop = 0;
        if (g_textTop > maxTop) g_textTop = maxTop;
    }
}

/* ---------------- 入口 ---------------- */
int main(void)
{
    airline_init();

    initgraph(WIN_W, WIN_H);
    raise_main_window();
    setbkcolor(C_BG);
    setbkmode(TRANSPARENT);
    BeginBatchDraw();

    draw_screen();
    FlushBatchDraw();

    while (!g_quit) {
        ExMessage m;

        getmessage(&m);
        if (m.message == WM_CLOSE) break;
        if (m.message == WM_MOUSEMOVE) continue;

        if (m.message == WM_LBUTTONDOWN)          { raise_main_window(); handle_click(m.x, m.y); }
        else if (m.message == WM_KEYDOWN)         handle_key(m.vkcode);
        else if (m.message == WM_MOUSEWHEEL)      handle_wheel((int)m.wheel);

        if (!g_quit) {
            draw_screen();
            FlushBatchDraw();
        }
    }

    EndBatchDraw();
    closegraph();
    return 0;
}
