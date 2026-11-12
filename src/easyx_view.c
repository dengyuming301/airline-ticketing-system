#include "airline.h"
#include <stdio.h>

#ifndef USE_EASYX
/* ============================================================
 *  没有启用 EasyX 时的占位实现
 * ============================================================ */
int ui_show_seat_window(Flight *f)
{
    (void)f;
    printf("本版本未编译 EasyX 图形界面。\n");
    printf("请使用 build_easyx.bat 编译（需要 EasyX for MinGW + MSVCRT 版 MinGW）。\n");
    return 0;
}

int ui_gui_available(void)
{
    return 0;
}

int ui_pick_seats(Flight *f, int cls, int qty, int *seats)
{
    (void)f; (void)cls; (void)qty; (void)seats;
    return 0;
}

#else
/* ============================================================
 *  EasyX 图形界面：座位图显示 / 点选座位
 * ============================================================ */
#include <graphics.h>
#include <tchar.h>
#include <windows.h>

/* ---------------- 亮色主题 ---------------- */
#define V_BG        RGB(240, 245, 251)
#define V_TITLE     RGB(26, 42, 62)
#define V_SUB       RGB(92, 110, 132)
#define V_PLANE     RGB(255, 255, 255)
#define V_PLANE_L   RGB(168, 190, 214)
#define V_HDR       RGB(224, 235, 248)
#define V_HDR_TXT   RGB(30, 50, 75)
#define V_ACCENT    RGB(244, 196, 84)
#define V_ACCENT_L  RGB(206, 150, 20)
#define V_ACCENT_T  RGB(70, 48, 0)

#define WIN_W        1080
#define WIN_H        720
#define PLANE_LEFT   24
#define PLANE_TOP    92
#define PLANE_RIGHT  600
#define PLANE_BOTTOM (WIN_H - 20)
#define TOP_Y        120
#define CLASS_HDR    26
#define SEAT_X       150
#define SEAT_RIGHT   (PLANE_RIGHT - 24)

typedef struct {
    int rowH;
    int rowsY[CLASS_CNT];   /* 每个舱位第一排的 y 坐标，用于点击命中判断 */
    int classTop[CLASS_CNT];/* 每个舱位标题条的 y 坐标，用于高亮边框 */
    int cols[CLASS_CNT];    /* 每排座位数 */
    int seatX[CLASS_CNT];   /* 第一列座位的 x */
    int seatW[CLASS_CNT];   /* 单个座位宽度 */
    int gap[CLASS_CNT];     /* 座位间距 */
    int aisle[CLASS_CNT];   /* 过道额外宽度 */
    int grp[CLASS_CNT][4];  /* 座位分组，如 2-4-2 */
} Layout;

static Layout g_lay;

/* 把 2-4-2 之类的分组写成文字 */
static void layout_text(const Flight *f, int cls, char *buf, int n)
{
    int k;
    size_t len = 0;

    buf[0] = '\0';
    for (k = 0; k < 4 && f->grp[cls][k] > 0; k++)
        len += (size_t)snprintf(buf + len, (size_t)n - len,
                                (k == 0) ? "%d" : "-%d", f->grp[cls][k]);
}

/* 按机型计算某舱位的座位几何：列数、座位宽度、过道位置 */
static void layout_class(const Flight *f, int cls)
{
    int cols = class_cols(f, cls);
    int avail = SEAT_RIGHT - SEAT_X;
    int k, ngroups = 0, aisles, w, total;

    for (k = 0; k < 4; k++) {
        g_lay.grp[cls][k] = f->grp[cls][k];
        if (f->grp[cls][k] > 0) ngroups++;
    }
    if (ngroups < 1) ngroups = 1;
    aisles = (ngroups > 1) ? ngroups - 1 : 0;

    w = (avail - 6 * (cols - 1) - 16 * aisles) / cols;
    if (w > 48) w = 48;
    if (w < 24) w = 24;

    total = cols * w + (cols - 1) * 6 + aisles * 16;
    g_lay.cols[cls]  = cols;
    g_lay.seatX[cls] = SEAT_X + (avail - total) / 2;
    g_lay.seatW[cls] = w;
    g_lay.gap[cls]   = 6;
    g_lay.aisle[cls] = 16;
}

/* 某舱位第 col 列座位的左边界 x（自动跳过过道） */
static int seat_x_of(int cls, int col)
{
    int k, cum = 0, aisles = 0;

    for (k = 0; k < 4 && g_lay.grp[cls][k] > 0; k++) {
        cum += g_lay.grp[cls][k];
        if (col >= cum) aisles++;
        else break;
    }
    return g_lay.seatX[cls] + col * (g_lay.seatW[cls] + g_lay.gap[cls])
           + aisles * g_lay.aisle[cls];
}

/* UTF-8 转成 EasyX 需要的编码 */
static void putU8(int x, int y, const char *s)
{
    wchar_t w[512];

    MultiByteToWideChar(CP_UTF8, 0, s, -1, w, 512);
#ifdef UNICODE
    outtextxy(x, y, w);
#else
    {
        char a[1536];
        WideCharToMultiByte(CP_ACP, 0, w, -1, a, (int)sizeof a, NULL, NULL);
        outtextxy(x, y, a);
    }
#endif
}

static void draw_legend(int x, int y)
{
    settextstyle(18, 0, _T("Microsoft YaHei"));

    setfillcolor(RGB(126, 200, 160));
    solidrectangle(x, y, x + 22, y + 16);
    settextcolor(V_TITLE);
    putU8(x + 30, y, "空闲座位");

    setfillcolor(RGB(235, 140, 140));
    solidrectangle(x, y + 30, x + 22, y + 46);
    putU8(x + 30, y + 30, "已订座位");

    setfillcolor(V_ACCENT);
    solidrectangle(x, y + 60, x + 22, y + 76);
    putU8(x + 30, y + 60, "已选座位");

    setfillcolor(RGB(200, 210, 220));
    solidrectangle(x, y + 90, x + 22, y + 106);
    putU8(x + 30, y + 90, "超出定额(超售)");
}

/* 画整幅画面；selCls < 0 表示纯查看，不显示选择态 */
void seat_view_draw(Flight *f, int selCls, const int *sel, int nsel, int pickQty,
                       const char *hint)
{
    int totalRows, rowH, y, cls, row, col, cols, idx, i;
    char buf[256], date[64];
    int days;

    totalRows = class_rows(f, 0) + class_rows(f, 1) + class_rows(f, 2);
    if (totalRows < 1) totalRows = 1;

    rowH = (PLANE_BOTTOM - TOP_Y - CLASS_CNT * (CLASS_HDR + 16)) / totalRows;
    if (rowH > 34) rowH = 34;
    if (rowH < 16) rowH = 16;
    g_lay.rowH = rowH;

    setbkcolor(V_BG);
    cleardevice();
    setbkmode(TRANSPARENT);

    /* ---------- 航空公司标识 + 航线 ---------- */
    {
        unsigned ac = airline_color(f->flightNo);
        COLORREF col = RGB((int)((ac >> 16) & 0xFF),
                           (int)((ac >> 8) & 0xFF),
                           (int)(ac & 0xFF));
        char an[128];

        setfillcolor(col);
        setlinecolor(col);
        fillroundrect(28, 10, 242, 46, 10, 10);
        settextcolor(RGB(255, 255, 255));
        settextstyle(19, 0, _T("Microsoft YaHei"));
        snprintf(an, sizeof an, "%s %s", airline_name(f->flightNo), f->flightNo);
        putU8(44, 18, an);
    }

    settextcolor(V_TITLE);
    settextstyle(30, 0, _T("Microsoft YaHei"));
    snprintf(buf, sizeof buf, "%s → %s", f->start, f->dest);
    putU8(258, 14, buf);

    /* 当前选购舱位徽章 */
    if (selCls >= 0) {
        setfillcolor(V_ACCENT);
        setlinecolor(V_ACCENT_L);
        fillroundrect(820, 10, 1050, 48, 10, 10);
        settextcolor(V_ACCENT_T);
        settextstyle(20, 0, _T("Microsoft YaHei"));
        snprintf(buf, sizeof buf, "当前舱位：%s", class_name(selCls));
        putU8(838, 17, buf);
    }

    next_flight_date(f, &days, date, (int)sizeof date);
    settextcolor(V_SUB);
    settextstyle(19, 0, _T("Microsoft YaHei"));
    snprintf(buf, sizeof buf, "机型 %s（%s）    最近航班 %s    总座位 %d",
             f->planeNo, (f->cols[CLS_ECONOMY] >= 8) ? "宽体机" : "窄体机",
             date, capacity_total(f));
    putU8(30, 52, buf);

    /* ---------- 机身 ---------- */
    setfillcolor(V_PLANE);
    setlinecolor(V_PLANE_L);
    fillroundrect(PLANE_LEFT, PLANE_TOP, PLANE_RIGHT, PLANE_BOTTOM, 60, 60);

    /* ---------- 座位 ---------- */
    y = TOP_Y;
    for (cls = 0; cls < CLASS_CNT; cls++) {
        int rows = class_rows(f, cls);
        int active = (selCls == cls);

        cols = class_cols(f, cls);
        layout_class(f, cls);
        g_lay.classTop[cls] = y;

        /* 选中的舱位标题条变成金色，其他舱位保持深蓝 */
        setfillcolor(active ? V_ACCENT : V_HDR);
        solidrectangle(PLANE_LEFT + 16, y, PLANE_RIGHT - 16, y + CLASS_HDR - 4);
        settextcolor(active ? V_ACCENT_T : V_HDR_TXT);
        settextstyle(18, 0, _T("Microsoft YaHei"));
        {
            char lt[16];
            layout_text(f, cls, lt, (int)sizeof lt);
            snprintf(buf, sizeof buf, "%s %s   定额 %d  已售 %d  余票 %d   ￥%.0f",
                     class_name(cls), lt, f->cap[cls], f->sold[cls],
                     remaining(f, cls), f->price[cls]);
        }
        putU8(PLANE_LEFT + 26, y + 3, buf);
        y += CLASS_HDR;

        /* 列标 A B C ... */
        settextcolor(RGB(120, 140, 165));
        settextstyle(14, 0, _T("Microsoft YaHei"));
        for (col = 0; col < cols; col++) {
            char lab[4];
            lab[0] = (char)('A' + col);
            lab[1] = '\0';
            putU8(seat_x_of(cls, col) + g_lay.seatW[cls] / 2 - 4, y, lab);
        }
        y += 16;

        g_lay.rowsY[cls] = y;

        for (row = 0; row < rows; row++) {
            for (col = 0; col < cols; col++) {
                int sx = seat_x_of(cls, col);
                int sy = y;
                int sh = rowH - 6;
                int selected = 0;

                idx = row * cols + col;

                if (cls == selCls) {
                    for (i = 0; i < nsel; i++)
                        if (sel[i] == idx) selected = 1;
                }

                if (idx >= f->cap[cls]) {
                    setfillcolor(RGB(236, 240, 245));
                    setlinecolor(RGB(220, 226, 234));
                } else if (selected) {
                    setfillcolor(V_ACCENT);
                    setlinecolor(V_ACCENT_L);
                } else if (f->taken[cls][row][col]) {
                    setfillcolor(RGB(235, 140, 140));
                    setlinecolor(RGB(200, 92, 92));
                } else {
                    setfillcolor(RGB(126, 200, 160));
                    setlinecolor(RGB(74, 158, 118));
                }
                fillroundrect(sx, sy, sx + g_lay.seatW[cls], sy + sh, 8, 8);

                if (idx < f->cap[cls]) {
                    if (selected) settextcolor(V_ACCENT_T);
                    else          settextcolor(RGB(28, 42, 56));
                    settextstyle(13, 0, _T("Microsoft YaHei"));
                    snprintf(buf, sizeof buf, "%d%c", row + 1, 'A' + col);
                    putU8(sx + 6, sy + 2, buf);
                }
            }
            y += rowH;
        }

        /* 给选中的舱位加金色描边，一眼能看出当前在哪个舱 */
        if (active) {
            setlinecolor(V_ACCENT);
            rectangle(PLANE_LEFT + 13, g_lay.classTop[cls] - 3,
                      PLANE_RIGHT - 13, y - 3);
            rectangle(PLANE_LEFT + 14, g_lay.classTop[cls] - 2,
                      PLANE_RIGHT - 14, y - 4);
        }
        y += 4;
    }

    /* ---------- 右侧信息栏 ---------- */
    settextcolor(V_TITLE);
    settextstyle(24, 0, _T("Microsoft YaHei"));
    putU8(640, 108, "订票情况");

    settextstyle(18, 0, _T("Microsoft YaHei"));
    settextcolor(RGB(70, 88, 110));
    snprintf(buf, sizeof buf, "已订票旅客：%d 人",
             f->sold[CLS_FIRST] + f->sold[CLS_BUSINESS] + f->sold[CLS_ECONOMY]);
    putU8(640, 144, buf);

    snprintf(buf, sizeof buf, "头等舱：%d / %d", f->sold[CLS_FIRST], f->cap[CLS_FIRST]);
    putU8(640, 172, buf);
    snprintf(buf, sizeof buf, "商务舱：%d / %d", f->sold[CLS_BUSINESS], f->cap[CLS_BUSINESS]);
    putU8(640, 196, buf);
    snprintf(buf, sizeof buf, "经济舱：%d / %d", f->sold[CLS_ECONOMY], f->cap[CLS_ECONOMY]);
    putU8(640, 220, buf);

    {
        int waitCount = 0;
        Waiter *w = f->waitlist;
        while (w != NULL) { waitCount++; w = w->next; }
        snprintf(buf, sizeof buf, "候补旅客：%d 人", waitCount);
        putU8(640, 252, buf);
    }

    {
        int total = capacity_total(f);
        int sold = f->sold[0] + f->sold[1] + f->sold[2];
        int rate = (total > 0) ? sold * 100 / total : 0;
        snprintf(buf, sizeof buf, "座位占用率：%d%%", rate);
        putU8(640, 284, buf);
    }

    if (selCls >= 0) {
        settextcolor(RGB(176, 120, 12));
        if (pickQty > 0)
            snprintf(buf, sizeof buf, "已选座位：%d / %d", nsel, pickQty);
        else
            snprintf(buf, sizeof buf, "已选座位：%d 个", nsel);
        putU8(640, 316, buf);
    }

    /* ---------- 图例 ---------- */
    settextcolor(V_TITLE);
    settextstyle(22, 0, _T("Microsoft YaHei"));
    putU8(640, 360, "图例");
    draw_legend(640, 396);

    /* ---------- 底部提示 ---------- */
    settextcolor(V_SUB);
    settextstyle(17, 0, _T("Microsoft YaHei"));
    putU8(640, WIN_H - 56, hint);

    FlushBatchDraw();
}

/* 点击坐标 -> 座位下标，未点中返回 -1 */
int seat_view_hit(Flight *f, int cls, int mx, int my)
{
    int rows = class_rows(f, cls);
    int cols = class_cols(f, cls);
    int relY = my - g_lay.rowsY[cls];
    int row, col, idx;

    if (relY < 0) return -1;
    row = relY / g_lay.rowH;
    if (row < 0 || row >= rows) return -1;
    if (relY % g_lay.rowH > g_lay.rowH - 7) return -1;   /* 排间空隙 */

    for (col = 0; col < cols; col++) {
        int x0 = seat_x_of(cls, col);
        if (mx >= x0 && mx < x0 + g_lay.seatW[cls]) {
            idx = row * cols + col;
            if (idx >= f->cap[cls]) return -1;
            return idx;
        }
    }
    return -1;
}

int ui_gui_available(void)
{
    return 1;
}

int ui_show_seat_window(Flight *f)
{
    ExMessage msg;

    if (f == NULL) return 0;

    initgraph(WIN_W, WIN_H);
    BeginBatchDraw();
    seat_view_draw(f, -1, NULL, 0, 0, "按任意键或单击关闭窗口");

    for (;;) {
        getmessage(&msg);
        if (msg.message == WM_KEYDOWN ||
            msg.message == WM_LBUTTONDOWN ||
            msg.message == WM_CLOSE)
            break;
    }

    closegraph();
    return 1;
}

int ui_pick_seats(Flight *f, int cls, int qty, int *seats)
{
    int nsel = 0, ok = 0, done = 0, cols;
    ExMessage msg;

    if (f == NULL || cls < 0 || cls >= CLASS_CNT || qty < 1 || qty > MAX_SEATS)
        return 0;

    cols = class_cols(f, cls);

    initgraph(WIN_W, WIN_H);
    BeginBatchDraw();
    seat_view_draw(f, cls, seats, nsel, qty, "点击空闲座位选择；Enter 确认，Esc 取消");

    while (!done) {
        getmessage(&msg);

        if (msg.message == WM_LBUTTONDOWN) {
            int idx = seat_view_hit(f, cls, msg.x, msg.y);
            if (idx >= 0 && !f->taken[cls][idx / cols][idx % cols]) {
                int k, found = -1;
                for (k = 0; k < nsel; k++)
                    if (seats[k] == idx) found = k;

                if (found >= 0) {                 /* 再点一次取消选择 */
                    for (k = found; k < nsel - 1; k++) seats[k] = seats[k + 1];
                    nsel--;
                } else if (nsel < qty) {
                    seats[nsel++] = idx;
                }

                seat_view_draw(f, cls, seats, nsel, qty,
                           "点击空闲座位选择；Enter 确认，Esc 取消");
            }
        } else if (msg.message == WM_KEYDOWN) {
            if (msg.vkcode == VK_RETURN && nsel == qty) {
                ok = 1;
                done = 1;
            } else if (msg.vkcode == VK_ESCAPE) {
                done = 1;
            }
        } else if (msg.message == WM_CLOSE) {
            done = 1;
        }
    }

    closegraph();
    return ok;
}
#endif /* USE_EASYX */
