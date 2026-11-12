#include "airline.h"
#include <stdlib.h>
#include <stdarg.h>

/* ============================================================
 *  全局数据
 * ============================================================ */
static Flight g_flights[MAX_FLIGHTS];
static int    g_count = 0;      /* 当前航线数 */
static int    g_seq   = 0;      /* 全局订票序号 */
#define MAX_FLIGHT_INSTANCES 6000
static Flight g_instances[MAX_FLIGHT_INSTANCES];
static int g_instanceCount;

/* ============================================================
 *  小工具
 * ============================================================ */
static void copy_s(char *dst, int n, const char *src)
{
    if (n <= 0) return;
    snprintf(dst, (size_t)n, "%s", src);
}

static int valid_flight_date(const Flight *f, const char *s)
{
    int y, m, d, bit;
    struct tm t;
    time_t stamp;
    if (s == NULL || sscanf(s, "%4d-%2d-%2d", &y, &m, &d) != 3 ||
        strlen(s) != 10 || s[4] != '-' || s[7] != '-') return 0;
    memset(&t, 0, sizeof t);
    t.tm_year = y - 1900; t.tm_mon = m - 1; t.tm_mday = d;
    t.tm_isdst = -1;
    stamp = mktime(&t);
    if (stamp == (time_t)-1 || t.tm_year != y - 1900 || t.tm_mon != m - 1 || t.tm_mday != d) return 0;
    bit = (t.tm_wday == 0) ? 6 : t.tm_wday - 1;
    return (f->weekMask & (1u << bit)) != 0;
}

const char *class_name(int cls)
{
    switch (cls) {
    case CLS_FIRST:    return "头等舱";
    case CLS_BUSINESS: return "商务舱";
    case CLS_ECONOMY:  return "经济舱";
    default:           return "未知舱位";
    }
}

int class_cols(const Flight *f, int cls)
{
    if (f == NULL || cls < 0 || cls >= CLASS_CNT) return 6;
    return (f->cols[cls] > 0) ? f->cols[cls] : ((cls == CLS_FIRST) ? 4 : 6);
}

/* ---------------- 机型座位布局 ---------------- */
typedef struct {
    const char *plane;
    int cols[CLASS_CNT];
    int grp[CLASS_CNT][4];
} PlaneLayout;

static const PlaneLayout kPlanes[] = {
    { "B738", { 4, 6, 6 }, { { 2, 2, 0, 0 }, { 3, 3, 0, 0 }, { 3, 3, 0, 0 } } },
    { "A320", { 4, 6, 6 }, { { 2, 2, 0, 0 }, { 3, 3, 0, 0 }, { 3, 3, 0, 0 } } },
    { "A321", { 4, 6, 6 }, { { 2, 2, 0, 0 }, { 3, 3, 0, 0 }, { 3, 3, 0, 0 } } },
    { "A330", { 4, 6, 8 }, { { 1, 2, 1, 0 }, { 2, 2, 2, 0 }, { 2, 4, 2, 0 } } },
    { "B787", { 4, 6, 9 }, { { 1, 2, 1, 0 }, { 2, 2, 2, 0 }, { 3, 3, 3, 0 } } },
    { "B777", { 4, 8, 9 }, { { 1, 2, 1, 0 }, { 2, 4, 2, 0 }, { 3, 3, 3, 0 } } }
};

void flight_apply_plane(Flight *f)
{
    size_t i;
    int c, k;
    const PlaneLayout *pl = NULL;

    if (f == NULL) return;

    for (i = 0; i < sizeof kPlanes / sizeof kPlanes[0]; i++) {
        if (strcmp(f->planeNo, kPlanes[i].plane) == 0) {
            pl = &kPlanes[i];
            break;
        }
    }

    for (c = 0; c < CLASS_CNT; c++) {
        if (pl != NULL) {
            f->cols[c] = pl->cols[c];
            for (k = 0; k < 4; k++) f->grp[c][k] = pl->grp[c][k];
        } else {
            f->cols[c] = (c == CLS_FIRST) ? 4 : 6;
            if (c == CLS_FIRST) {
                f->grp[c][0] = 2; f->grp[c][1] = 2;
            } else {
                f->grp[c][0] = 3; f->grp[c][1] = 3;
            }
            f->grp[c][2] = 0;
            f->grp[c][3] = 0;
        }
    }
}

/* ---------------- 航空公司 ---------------- */
typedef struct {
    const char *code;
    const char *name;
    unsigned    color;      /* 0xRRGGBB */
} AirlineInfo;

static const AirlineInfo kAirlines[] = {
    { "CZ", "南方航空", 0x00529Bu },
    { "CA", "中国国航", 0xC41230u },
    { "MU", "东方航空", 0x8E1F3Fu },
    { "3U", "四川航空", 0xC05A0Cu },
    { "ZH", "深圳航空", 0x006E78u },
    { "HU", "海南航空", 0xA81458u },
    { "MF", "厦门航空", 0x004696u },
    { "GS", "天津航空", 0x2E5E9Eu },
    { "KN", "联合航空", 0x1F6F5Cu },
    { "G5", "华夏航空", 0x6A4C93u }
};

const char *airline_name(const char *flightNo)
{
    size_t i;

    if (flightNo == NULL) return "航空公司";
    for (i = 0; i < sizeof kAirlines / sizeof kAirlines[0]; i++)
        if (strncmp(flightNo, kAirlines[i].code, 2) == 0)
            return kAirlines[i].name;
    return "航空公司";
}

int airline_total(void)
{
    return (int)(sizeof kAirlines / sizeof kAirlines[0]);
}

const char *airline_code_at(int i)
{
    if (i < 0 || i >= airline_total()) return "";
    return kAirlines[i].code;
}

const char *airline_name_at(int i)
{
    if (i < 0 || i >= airline_total()) return "";
    return kAirlines[i].name;
}

unsigned airline_color(const char *flightNo)
{
    size_t i;

    if (flightNo == NULL) return 0x5A6472u;
    for (i = 0; i < sizeof kAirlines / sizeof kAirlines[0]; i++)
        if (strncmp(flightNo, kAirlines[i].code, 2) == 0)
            return kAirlines[i].color;
    return 0x5A6472u;
}

int flight_count(void) { return g_count; }

Flight *flight_at(int i)
{
    return (i >= 0 && i < g_count) ? &g_flights[i] : NULL;
}

Flight *find_flight(const char *flightNo)
{
    int i;
    for (i = 0; i < g_count; i++)
        if (strcmp(g_flights[i].flightNo, flightNo) == 0)
            return &g_flights[i];
    return NULL;
}

Flight *flight_instance(Flight *route, const char *date)
{
    int i;
    if (route == NULL || !valid_flight_date(route, date)) return NULL;
    for (i = 0; i < g_instanceCount; i++)
        if (strcmp(g_instances[i].flightNo, route->flightNo) == 0 && strcmp(g_instances[i].serviceDate, date) == 0)
            return &g_instances[i];
    if (g_instanceCount >= MAX_FLIGHT_INSTANCES) return NULL;
    {
        Flight *f = &g_instances[g_instanceCount++];
        memset(f, 0, sizeof *f);
        copy_s(f->start, MAX_CITY, route->start); copy_s(f->dest, MAX_CITY, route->dest);
        copy_s(f->flightNo, MAX_NO, route->flightNo); copy_s(f->planeNo, MAX_NO, route->planeNo);
        copy_s(f->serviceDate, sizeof f->serviceDate, date);
        f->weekMask = route->weekMask;
        memcpy(f->cap, route->cap, sizeof f->cap); memcpy(f->overbook, route->overbook, sizeof f->overbook);
        memcpy(f->price, route->price, sizeof f->price); flight_apply_plane(f);
        return f;
    }
}

int flight_instance_count(void) { return g_instanceCount; }
Flight *flight_instance_at(int index)
{
    return index >= 0 && index < g_instanceCount ? &g_instances[index] : NULL;
}

int remaining(const Flight *f, int cls)
{
    int r = f->cap[cls] - f->sold[cls];
    return (r > 0) ? r : 0;
}

int capacity_total(const Flight *f)
{
    return f->cap[0] + f->cap[1] + f->cap[2];
}

int class_rows(const Flight *f, int cls)
{
    int cols = class_cols(f, cls);
    return (f->cap[cls] + cols - 1) / cols;
}

int seat_label(const Flight *f, int cls, int idx, char *buf)
{
    int cols = class_cols(f, cls);
    if (idx == SEAT_NONE) { copy_s(buf, 16, "无座"); return 0; }
    sprintf(buf, "%d%c", idx / cols + 1, 'A' + idx % cols);
    return 1;
}

/* ============================================================
 *  初始化：15 条航线
 *  机型统一按 头等 8 / 商务 24 / 经济 60 配置，便于演示超售
 * ============================================================ */
typedef struct {
    const char *start;
    const char *dest;
    const char *no;
    const char *plane;
    unsigned    mask;
    double      p1, p2, p3;
} InitRow;

static const InitRow kInit[] = {
    { "广州", "北京", "CZ3101", "B738", 0x15u, 3280.0, 1860.0,  980.0 },
    { "广州", "北京", "CA1391", "A330", 0x2Au, 3380.0, 1920.0, 1020.0 },
    { "广州", "上海", "MU5301", "A320", 0x7Fu, 1300.0,  760.0,  420.0 },
    { "广州", "成都", "3U8731", "A321", 0x15u, 1500.0,  880.0,  520.0 },
    { "广州", "西安", "CZ3211", "B738", 0x0Au, 1600.0,  900.0,  540.0 },
    { "北京", "上海", "CA1501", "B787", 0x7Fu, 1400.0,  820.0,  450.0 },
    { "北京", "上海", "MU5102", "A330", 0x2Au, 1350.0,  780.0,  430.0 },
    { "北京", "深圳", "ZH9101", "A320", 0x15u, 2400.0, 1350.0,  760.0 },
    { "北京", "成都", "CA4101", "A321", 0x0Au, 1900.0, 1100.0,  620.0 },
    { "北京", "广州", "CA1301", "B738", 0x55u, 3200.0, 1800.0,  980.0 },
    { "北京", "西安", "HU7135", "B738", 0x3Fu, 1500.0,  860.0,  480.0 },
    { "上海", "深圳", "MU5331", "A320", 0x15u, 1600.0,  920.0,  520.0 },
    { "上海", "成都", "MU5401", "A321", 0x0Au, 1800.0, 1000.0,  560.0 },
    { "深圳", "成都", "ZH9201", "B738", 0x55u, 1500.0,  850.0,  470.0 },
    { "成都", "西安", "3U8951", "A320", 0x7Fu,  900.0,  520.0,  300.0 }
};

int airline_init(void)
{
    int i, c;
    int n = (int)(sizeof kInit / sizeof kInit[0]);

    g_count = 0;
    g_seq   = 0;
    g_instanceCount = 0;

    for (i = 0; i < n && g_count < MAX_FLIGHTS; i++) {
        Flight *f = &g_flights[g_count++];
        memset(f, 0, sizeof *f);
        copy_s(f->start,    MAX_CITY, kInit[i].start);
        copy_s(f->dest,     MAX_CITY, kInit[i].dest);
        copy_s(f->flightNo, MAX_NO,   kInit[i].no);
        copy_s(f->planeNo,  MAX_NO,   kInit[i].plane);
        flight_apply_plane(f);
        f->weekMask = kInit[i].mask;
        f->cap[CLS_FIRST]    = 8;
        f->cap[CLS_BUSINESS] = 24;
        f->cap[CLS_ECONOMY]  = 60;
        f->price[CLS_FIRST]    = kInit[i].p1;
        f->price[CLS_BUSINESS] = kInit[i].p2;
        f->price[CLS_ECONOMY]  = kInit[i].p3;
        for (c = 0; c < CLASS_CNT; c++) {
            f->overbook[c] = f->cap[c] / 10;      /* 超售阈值：定额的 10% */
            if (f->overbook[c] < 1) f->overbook[c] = 1;
        }
    }
    return g_count;
}

/* ============================================================
 *  日期：计算最近一班航班的日期
 * ============================================================ */
int next_flight_date(const Flight *f, int *daysLater, char *desc, int descLen)
{
    static const char *wk[] = { "日", "一", "二", "三", "四", "五", "六" };
    time_t now = time(NULL);
    struct tm base = *localtime(&now);
    int i;

    for (i = 0; i <= 7; i++) {
        struct tm t = base;
        int w, bit;

        t.tm_mday += i;
        mktime(&t);                     /* 规范化日期（自动进位） */
        w   = t.tm_wday;                /* 0=周日 */
        bit = (w == 0) ? 6 : w - 1;     /* 周一=bit0 ... 周日=bit6 */

        if (f->weekMask & (1u << bit)) {
            snprintf(desc, (size_t)descLen, "%04d-%02d-%02d 星期%s%s",
                     t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, wk[w],
                     (i == 0) ? "（今天）" : "");
            *daysLater = i;
            return 1;
        }
    }
    snprintf(desc, (size_t)descLen, "未来一周无航班");
    *daysLater = -1;
    return 0;
}

void weekmask_text(unsigned mask, char *buf, int bufLen)
{
    static const char *w[] = { "一", "二", "三", "四", "五", "六", "日" };
    int i, first = 1;
    size_t len = 0;

    buf[0] = '\0';
    if ((mask & 0x7Fu) == 0x7Fu) {
        snprintf(buf, (size_t)bufLen, "每天");
        return;
    }
    for (i = 0; i < 7; i++) {
        if (mask & (1u << i)) {
            len += (size_t)snprintf(buf + len, (size_t)bufLen - len,
                                    first ? "周%s" : "、周%s", w[i]);
            first = 0;
        }
    }
    if (first) snprintf(buf, (size_t)bufLen, "无");
}

/* ============================================================
 *  座位操作
 * ============================================================ */
static int seat_is_free(const Flight *f, int cls, int idx)
{
    int cols = class_cols(f, cls);
    if (idx < 0 || idx >= f->cap[cls]) return 0;
    return !f->taken[cls][idx / cols][idx % cols];
}

static int count_free(const Flight *f, int cls)
{
    int i, cols = class_cols(f, cls), cnt = 0;
    for (i = 0; i < f->cap[cls]; i++)
        if (!f->taken[cls][i / cols][i % cols]) cnt++;
    return cnt;
}

static void take_seat(Flight *f, int cls, int idx)
{
    int cols = class_cols(f, cls);
    if (idx >= 0 && idx < f->cap[cls])
        f->taken[cls][idx / cols][idx % cols] = 1;
}

static void free_seat(Flight *f, int cls, int idx)
{
    int cols = class_cols(f, cls);
    if (idx >= 0 && idx < f->cap[cls])
        f->taken[cls][idx / cols][idx % cols] = 0;
}

/* 把座位下标数组转成 "座位 3A、3B" 之类的文本 */
static void seats_to_text(const Flight *f, int cls, const int *seats, int n, char *buf, int nbuf)
{
    int i, first = 1;
    size_t len = 0;
    char lab[16];

    buf[0] = '\0';
    for (i = 0; i < n; i++) {
        if (seats[i] == SEAT_NONE) continue;
        seat_label(f, cls, seats[i], lab);
        if (first) len += (size_t)snprintf(buf + len, (size_t)nbuf - len, "座位 %s", lab);
        else       len += (size_t)snprintf(buf + len, (size_t)nbuf - len, "、%s", lab);
        first = 0;
        if (len >= (size_t)nbuf) break;
    }
    if (first) snprintf(buf, (size_t)nbuf, "无固定座位");
}

/* ============================================================
 *  订票
 *  返回 1 成功；0 参数/座位错误；-1 余票不足（可候补）
 * ============================================================ */
int book_ticket(Flight *f, const char *name, const char *id, int cls, int qty,
                const char *date, const int *wantSeats, int nWant, char *out, int outLen)
{
    Booking *b, *tail;
    int i, seats[MAX_SEATS], nseat = 0, sellable, noSeat = 0;
    char seatbuf[160], lab[16];

    if (f == NULL) { snprintf(out, (size_t)outLen, "航班不存在"); return 0; }
    if (cls < 0 || cls >= CLASS_CNT) { snprintf(out, (size_t)outLen, "舱位无效"); return 0; }
    if (qty < 1 || qty > MAX_SEATS) {
        snprintf(out, (size_t)outLen, "一次最多预订 %d 张", MAX_SEATS);
        return 0;
    }
    if (name[0] == '\0' || id[0] == '\0' || !valid_flight_date(f, date) ||
        (f->serviceDate[0] != '\0' && strcmp(f->serviceDate, date) != 0)) {
        snprintf(out, (size_t)outLen, "姓名、身份证号不能为空，且日期须为该航线实际飞行日（YYYY-MM-DD）");
        return 0;
    }

    sellable = f->cap[cls] + f->overbook[cls] - f->sold[cls];
    if (qty > sellable) {
        snprintf(out, (size_t)outLen,
                 "%s余票不足：当前余票 %d 张，超售上限 %d 张",
                 class_name(cls), remaining(f, cls), f->overbook[cls]);
        return -1;
    }

    /* --- 分配座位 --- */
    if (nWant > 0) {
        if (nWant != qty) {
            snprintf(out, (size_t)outLen, "选座数量与订票张数不一致");
            return 0;
        }
        for (i = 0; i < nWant; i++) {
            if (!seat_is_free(f, cls, wantSeats[i])) {
                seat_label(f, cls, wantSeats[i], lab);
                snprintf(out, (size_t)outLen, "座位 %s 不可选（已占用或超出该舱位范围）", lab);
                return 0;
            }
        }
        for (i = 0; i < nWant; i++) {
            take_seat(f, cls, wantSeats[i]);
            seats[nseat++] = wantSeats[i];
        }
    } else {
        int cols = class_cols(f, cls);
        int need = qty;
        for (i = 0; i < f->cap[cls] && need > 0; i++) {
            if (!f->taken[cls][i / cols][i % cols]) {
                take_seat(f, cls, i);
                seats[nseat++] = i;
                need--;
            }
        }
        while (need > 0) {          /* 超出定额的部分按超售处理，无固定座位 */
            seats[nseat++] = SEAT_NONE;
            noSeat++;
            need--;
        }
    }

    /* --- 生成订票记录 --- */
    b = (Booking *)malloc(sizeof *b);
    if (b == NULL) {
        for (i = 0; i < nseat; i++) free_seat(f, cls, seats[i]);
        snprintf(out, (size_t)outLen, "内存不足，订票失败");
        return 0;
    }
    memset(b, 0, sizeof *b);
    b->seq = ++g_seq;
    copy_s(b->name, MAX_NAME, name);
    copy_s(b->id,   MAX_ID,   id);
    copy_s(b->date, sizeof b->date, date);
    b->cls  = cls;
    b->qty  = qty;
    b->nseat = nseat;
    for (i = 0; i < nseat; i++) b->seats[i] = seats[i];
    b->next = NULL;

    if (f->bookings == NULL) f->bookings = b;
    else {
        tail = f->bookings;
        while (tail->next != NULL) tail = tail->next;
        tail->next = b;
    }
    f->sold[cls] += qty;

    seats_to_text(f, cls, seats, nseat, seatbuf, (int)sizeof seatbuf);
    if (noSeat > 0)
        snprintf(out, (size_t)outLen, "订票成功：%s %d 张，%s；其中 %d 张为超售无座",
                 class_name(cls), qty, seatbuf, noSeat);
    else
        snprintf(out, (size_t)outLen, "订票成功：%s %d 张，%s",
                 class_name(cls), qty, seatbuf);
    return 1;
}

/* ============================================================
 *  退票：按姓名 + 身份证号查找（同一人多次订票时退最近一次）
 * ============================================================ */
int refund_ticket(Flight *f, const char *name, const char *id,
                  const char *date, int *refundedQty, int *refundedClass, char *out, int outLen)
{
    Booking *b = f->bookings, *prev = NULL, *target = NULL, *targetPrev = NULL;
    int i, qty, cls, seq;
    char pname[MAX_NAME];

    while (b != NULL) {
        if (strcmp(b->name, name) == 0 && strcmp(b->id, id) == 0 && strcmp(b->date, date) == 0) {
            target = b;
            targetPrev = prev;
        }
        prev = b;
        b = b->next;
    }

    if (target == NULL) {
        snprintf(out, (size_t)outLen,
                 "未找到 %s（%s）在航班 %s 的订票记录", name, id, f->flightNo);
        return 0;
    }

    qty = target->qty;
    cls = target->cls;
    seq = target->seq;
    if (!valid_flight_date(f, date) || (f->serviceDate[0] != '\0' && strcmp(f->serviceDate, date) != 0)) {
        snprintf(out, (size_t)outLen, "退票日期不是该航班的飞行日");
        return 0;
    }
    copy_s(pname, MAX_NAME, target->name);

    for (i = 0; i < target->nseat; i++) free_seat(f, cls, target->seats[i]);
    f->sold[cls] -= qty;

    if (targetPrev != NULL) targetPrev->next = target->next;
    else                    f->bookings = target->next;
    free(target);

    if (refundedQty != NULL) *refundedQty = qty;
    if (refundedClass != NULL) *refundedClass = cls;
    snprintf(out, (size_t)outLen, "退票成功：%s %s %d 张（订票序号 %d）",
             pname, class_name(cls), qty, seq);
    return 1;
}

/* ============================================================
 *  候补队列
 * ============================================================ */
int add_waitlist(Flight *f, const char *name, const char *id, int cls, int qty, const char *date)
{
    Waiter *w = (Waiter *)malloc(sizeof *w);
    Waiter *tail;

    if (w == NULL) return 0;
    memset(w, 0, sizeof *w);
    w->seq = ++g_seq;
    copy_s(w->name, MAX_NAME, name);
    if (f == NULL || cls < 0 || cls >= CLASS_CNT || qty < 1 || qty > MAX_SEATS || !valid_flight_date(f, date)) { free(w); return 0; }
    copy_s(w->id,   MAX_ID,   id);
    copy_s(w->date, sizeof w->date, date);
    w->cls = cls;
    w->qty = qty;
    w->next = NULL;

    if (f->waitlist == NULL) f->waitlist = w;
    else {
        tail = f->waitlist;
        while (tail->next != NULL) tail = tail->next;
        tail->next = w;
    }
    return 1;
}

/* 退票后按候补顺序自动补位：能补一个补一个 */
int process_waitlist(Flight *f, const char *date, int cls, int refundedQty,
                     int afterSeq, int *candidateSeq, int *candidateQty, char *out, int outLen)
{
    Waiter *w;
    if (candidateSeq) *candidateSeq = 0;
    if (candidateQty) *candidateQty = 0;
    if (f == NULL) { snprintf(out, (size_t)outLen, "航班无效"); return 0; }
    for (w = f->waitlist; w != NULL; w = w->next) {
        if (w->seq <= afterSeq || strcmp(w->date, date) != 0 || w->cls != cls || w->qty > refundedQty) continue;
        if (w->qty > remaining(f, cls) + f->overbook[cls]) continue;
        if (candidateSeq) *candidateSeq = w->seq;
        if (candidateQty) *candidateQty = w->qty;
        snprintf(out, (size_t)outLen, "候补旅客：%s（%s），%s，%d 张；本次退票释放 %d 张",
                 w->name, w->id, class_name(w->cls), w->qty, refundedQty);
        return 1;
    }
    snprintf(out, (size_t)outLen, "没有符合本次退票数量与舱位的后续候补旅客");
    return 0;
}

int confirm_waiter(Flight *f, int candidateSeq, int accept, char *out, int outLen)
{
    Waiter *w, *prev = NULL;
    int r = 0;
    if (f == NULL) return 0;
    for (w = f->waitlist; w != NULL && w->seq != candidateSeq; w = w->next) prev = w;
    if (w == NULL) { snprintf(out, (size_t)outLen, "候补记录已不存在"); return 0; }
    if (accept) r = book_ticket(f, w->name, w->id, w->cls, w->qty, w->date, NULL, 0, out, outLen);
    if (!accept || r == 1) {
        if (prev) prev->next = w->next; else f->waitlist = w->next;
        free(w); return accept ? 1 : 2;
    }
    return r;
}

/* ============================================================
 *  推荐同目的地的其它航班
 * ============================================================ */
int recommend_flights(const Flight *src, const char *date, int cls, int qty, Flight **out, int max)
{
    int i, n = 0;

    for (i = 0; i < g_count && n < max; i++) {
        Flight *f = &g_flights[i];
        int total;

        if (strcmp(f->flightNo, src->flightNo) == 0) continue;
        if (strcmp(f->dest, src->dest) != 0) continue;

        {
            Flight *inst = flight_instance(f, date);
            if (inst == NULL || cls < 0 || cls >= CLASS_CNT || remaining(inst, cls) < qty) continue;
            total = remaining(inst, cls);
        }
        if (total <= 0) continue;
        out[n++] = flight_instance(f, date);
    }
    return n;
}

/* ============================================================
 *  管理员：新建 / 修改航线
 * ============================================================ */
Flight *admin_new_flight(const char *start, const char *dest, const char *flightNo,
                         const char *planeNo, unsigned weekMask,
                         const int cap[CLASS_CNT], const double price[CLASS_CNT])
{
    Flight *f;
    int c;

    if (find_flight(flightNo) != NULL) return NULL;   /* 航班号不能重复 */
    if (g_count >= MAX_FLIGHTS) return NULL;

    f = &g_flights[g_count];
    memset(f, 0, sizeof *f);
    copy_s(f->start,    MAX_CITY, start);
    copy_s(f->dest,     MAX_CITY, dest);
    copy_s(f->flightNo, MAX_NO,   flightNo);
    copy_s(f->planeNo,  MAX_NO,   planeNo);
    flight_apply_plane(f);
    f->weekMask = weekMask;

    for (c = 0; c < CLASS_CNT; c++) {
        int maxCap = MAX_ROWS * class_cols(f, c);
        if (cap[c] < 1 || cap[c] > maxCap) return NULL;
        f->cap[c]   = cap[c];
        f->price[c] = price[c];
        f->overbook[c] = cap[c] / 10;
        if (f->overbook[c] < 1) f->overbook[c] = 1;
    }
    g_count++;
    return f;
}

int admin_set_capacity(Flight *f, int cls, int newCap, char *out, int outLen)
{
    int i, cols, maxCap;

    if (f == NULL || cls < 0 || cls >= CLASS_CNT) {
        snprintf(out, (size_t)outLen, "参数错误");
        return 0;
    }
    cols   = class_cols(f, cls);
    maxCap = MAX_ROWS * cols;

    if (newCap < 1 || newCap > maxCap) {
        snprintf(out, (size_t)outLen, "%s定额必须在 1~%d 之间", class_name(cls), maxCap);
        return 0;
    }
    if (newCap < f->sold[cls]) {
        snprintf(out, (size_t)outLen,
                 "不允许修改：%s已售出 %d 张，定额不能小于已售数量（否则余票为负）",
                 class_name(cls), f->sold[cls]);
        return 0;
    }
    /* 缩容时不能把已经占用的座位挤出座位图 */
    for (i = newCap; i < f->cap[cls]; i++) {
        if (f->taken[cls][i / cols][i % cols]) {
            snprintf(out, (size_t)outLen,
                     "不允许修改：第 %d 排及之后的座位仍有人预订，请先办理退票", newCap / cols + 1);
            return 0;
        }
    }

    {
        int oldCap = f->cap[cls];
        f->cap[cls] = newCap;
        f->overbook[cls] = newCap / 10;
        if (f->overbook[cls] < 1) f->overbook[cls] = 1;

        /* 扩容后，把原来超售无座的旅客补上座位 */
        if (newCap > oldCap) {
            Booking *b;
            int k, cols2 = class_cols(f, cls), cand = 0;
            for (b = f->bookings; b != NULL; b = b->next) {
                if (b->cls != cls) continue;
                for (k = 0; k < b->nseat; k++) {
                    if (b->seats[k] != SEAT_NONE) continue;
                    while (cand < f->cap[cls] &&
                           f->taken[cls][cand / cols2][cand % cols2]) cand++;
                    if (cand >= f->cap[cls]) break;
                    take_seat(f, cls, cand);
                    b->seats[k] = cand;
                    cand++;
                }
            }
        }
    }
    snprintf(out, (size_t)outLen, "%s定额已改为 %d，余票 %d",
             class_name(cls), newCap, remaining(f, cls));
    return 1;
}

/* ============================================================
 *  打印
 * ============================================================ */
void print_seatmap(const Flight *f)
{
    int cls, row, col, cols, rows, idx;
    char date[64];
    int days;

    next_flight_date(f, &days, date, (int)sizeof date);
    printf("\n航班 %s  %s → %s  机型 %s  %s\n",
           f->flightNo, f->start, f->dest, f->planeNo, date);

    for (cls = 0; cls < CLASS_CNT; cls++) {
        cols = class_cols(f, cls);
        rows = class_rows(f, cls);
        printf("\n【%s】定额 %d  已售 %d  余票 %d  票价 ￥%.0f\n",
               class_name(cls), f->cap[cls], f->sold[cls],
               remaining(f, cls), f->price[cls]);
        printf("        ");
        for (col = 0; col < cols; col++) printf("%c     ", 'A' + col);
        printf("\n");

        for (row = 0; row < rows; row++) {
            printf("  %2d  ", row + 1);
            for (col = 0; col < cols; col++) {
                idx = row * cols + col;
                if (idx >= f->cap[cls])        printf("      ");
                else if (f->taken[cls][row][col]) printf("[ X ] ");
                else                            printf("[   ] ");
            }
            printf("\n");
        }
    }
    printf("\n说明：[ X ] 表示该座位已订，[   ] 表示空闲。\n");
}

void print_bookings(const Flight *f)
{
    Booking *b;
    Waiter *w;
    char seatbuf[160];

    printf("\n----- 航班 %s（%s → %s）-----\n",
           f->flightNo, f->start, f->dest);

    if (f->bookings == NULL) {
        printf("  暂无订票记录。\n");
    } else {
        printf("  订票记录：\n");
        for (b = f->bookings; b != NULL; b = b->next) {
            seats_to_text(f, b->cls, b->seats, b->nseat, seatbuf, (int)sizeof seatbuf);
            printf("   #%-3d %-10s %-20s %-10s %-8s %d 张  %s\n",
                   b->seq, b->name, b->id, b->date, class_name(b->cls), b->qty, seatbuf);
        }
    }

    if (f->waitlist != NULL) {
        printf("  候补队列：\n");
        for (w = f->waitlist; w != NULL; w = w->next)
            printf("   %-10s %-20s %-8s %d 张\n",
                   w->name, w->id, class_name(w->cls), w->qty);
    }
    printf("\n");
}

void print_all_records(void)
{
    int i;
    for (i = 0; i < g_count; i++) print_bookings(&g_flights[i]);
    for (i = 0; i < g_instanceCount; i++) print_bookings(&g_instances[i]);
}

/* ============================================================
 *  超卖与模拟登机
 * ============================================================ */
static int has_noseat(const Booking *b)
{
    int i;
    for (i = 0; i < b->nseat; i++)
        if (b->seats[i] == SEAT_NONE) return 1;
    return 0;
}

/* 选择升舱对象：优先“已无座”的旅客；若都有座，则取最后订票的旅客 */
static Booking *pick_upgrade(const Flight *f, int cls, int target)
{
    Booking *b, *noseat = NULL, *latest = NULL;
    int freeCnt = count_free(f, target);

    for (b = f->bookings; b != NULL; b = b->next) {
        if (b->cls != cls) continue;
        if (b->qty > freeCnt) continue;          /* 高舱位空位不够，跳过 */
        if (has_noseat(b)) {
            if (noseat == NULL || b->seq < noseat->seq) noseat = b;
        }
        if (latest == NULL || b->seq > latest->seq) latest = b;
    }
    return (noseat != NULL) ? noseat : latest;
}

static int upgrade_booking(Flight *f, Booking *b, int target, char *out, int outLen)
{
    int old = b->cls;
    int i, k = 0, cols = class_cols(f, target);
    int seats[MAX_SEATS];

    if (count_free(f, target) < b->qty) return 0;

    for (i = 0; i < f->cap[target] && k < b->qty; i++) {
        if (!f->taken[target][i / cols][i % cols]) {
            take_seat(f, target, i);
            seats[k++] = i;
        }
    }
    if (k < b->qty) return 0;                    /* 理论上不会发生 */

    for (i = 0; i < b->nseat; i++) free_seat(f, old, b->seats[i]);
    for (i = 0; i < k; i++) b->seats[i] = seats[i];
    b->nseat = k;
    b->cls   = target;

    f->sold[old]    -= b->qty;
    f->sold[target] += b->qty;

    snprintf(out, (size_t)outLen, "%s（订票序号 %d）由%s调整为%s",
             b->name, b->seq, class_name(old), class_name(target));
    return 1;
}

/* 强制退掉该舱位最后订票的旅客 */
static int force_drop(Flight *f, int cls, char *out, int outLen)
{
    Booking *b, *best = NULL, *prev = NULL, *bestPrev = NULL;
    int i;

    for (b = f->bookings; b != NULL; b = b->next) {
        if (b->cls == cls && (best == NULL || b->seq > best->seq)) {
            best = b;
            bestPrev = prev;
        }
        prev = b;
    }
    if (best == NULL) return 0;

    snprintf(out, (size_t)outLen, "%s（订票序号 %d，%s %d 张）",
             best->name, best->seq, class_name(cls), best->qty);

    for (i = 0; i < best->nseat; i++) free_seat(f, cls, best->seats[i]);
    f->sold[cls] -= best->qty;

    if (bestPrev != NULL) bestPrev->next = best->next;
    else                  f->bookings = best->next;
    {
        int qty = best->qty;
        free(best);
        return qty;                 /* 返回强退的旅客人数 */
    }
}

/* 往字符串缓冲区追加内容（用于把结果交给图形界面显示） */
static void app(char *buf, int n, size_t *len, const char *fmt, ...)
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

void simulate_boarding(Flight *f, char *out, int outLen)
{
    int cls, moved = 0, dropped = 0;
    size_t len = 0;

    out[0] = '\0';
    app(out, outLen, &len, "===== 航班 %s（%s → %s）模拟登机 =====\n",
        f->flightNo, f->start, f->dest);
    app(out, outLen, &len, "规则：经济舱超员先升商务舱，商务舱超员再升头等舱；\n");
    app(out, outLen, &len, "      仍未解决时，按“最后订票先退票”的原则强制退票。\n\n");

    for (cls = CLS_ECONOMY; cls >= CLS_FIRST; cls--) {
        if (f->sold[cls] <= f->cap[cls]) continue;

        app(out, outLen, &len, "-- %s：定额 %d，已售 %d，超员 %d --\n",
            class_name(cls), f->cap[cls], f->sold[cls],
            f->sold[cls] - f->cap[cls]);

        while (f->sold[cls] > f->cap[cls]) {
            Booking *b = NULL;
            char msg[160];
            int target = cls - 1;

            if (target >= 0) {
                b = pick_upgrade(f, cls, target);
                if (b != NULL && upgrade_booking(f, b, target, msg, (int)sizeof msg)) {
                    app(out, outLen, &len, "  [升舱] %s\n", msg);
                    moved += b->qty;        /* 按实际旅客人数统计 */
                    continue;
                }
            }

            {
                int n = force_drop(f, cls, msg, (int)sizeof msg);
                if (n > 0) {
                    app(out, outLen, &len, "  [强退] %s\n", msg);
                    dropped += n;
                } else {
                    break;
                }
            }
        }
        app(out, outLen, &len, "\n");
    }

    app(out, outLen, &len, "模拟登机结束：升舱调整 %d 人次，强制退票 %d 人次。\n",
        moved, dropped);
    app(out, outLen, &len, "登机后各舱位实际人数：头等舱 %d/%d，商务舱 %d/%d，经济舱 %d/%d。\n",
        f->sold[0], f->cap[0], f->sold[1], f->cap[1], f->sold[2], f->cap[2]);
}

/* ============================================================
 *  演示：把某舱位自动订到“定额 + 超售阈值”
 * ============================================================ */
void demo_overbook(Flight *f, int cls, char *out, int outLen)
{
    int target;
    char name[MAX_NAME], id[MAX_ID], msg[256];

    if (f == NULL || cls < 0 || cls >= CLASS_CNT || f->serviceDate[0] == '\0') {
        snprintf(out, (size_t)outLen, "超售演示失败：请先选择航班日期。");
        return;
    }
    target = f->cap[cls] + f->overbook[cls];

    while (f->sold[cls] < target) {
        int qty = target - f->sold[cls];
        if (qty > 3) qty = 3;

        snprintf(name, sizeof name, "演示旅客%d", f->sold[cls] + 1);
        snprintf(id,   sizeof id,   "DEMO%06d", f->sold[cls] + 1);

        if (book_ticket(f, name, id, cls, qty, f->serviceDate, NULL, 0, msg, (int)sizeof msg) != 1) {
            snprintf(out, (size_t)outLen, "超售演示中断：%s；当前已售 %d/%d（超售上限 %d）。",
                     msg, f->sold[cls], f->cap[cls], f->overbook[cls]);
            break;
        }
    }
    if (f->sold[cls] >= target)
        snprintf(out, (size_t)outLen,
                 "航班 %s（%s）%s已完成超售演示：当前已售 %d 张（定额 %d，超售上限 %d）。",
                 f->flightNo, f->serviceDate, class_name(cls), f->sold[cls], f->cap[cls], f->overbook[cls]);
}
