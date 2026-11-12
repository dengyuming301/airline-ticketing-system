#include "airline.h"
#include <stdlib.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* ============================================================
 *  输入输出辅助
 * ============================================================ */
static int g_eof = 0;      /* 输入结束（例如重定向测试）时退出主循环 */

static void init_console(void)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

static void read_line(char *buf, int n)
{
    size_t len;

    if (fgets(buf, n, stdin) == NULL) {
        buf[0] = '\0';
        g_eof = 1;
        return;
    }
    len = strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
        buf[--len] = '\0';
}

static int read_int(const char *prompt)
{
    char buf[64];
    printf("%s", prompt);
    read_line(buf, (int)sizeof buf);
    return atoi(buf);
}

static int parse_seat(const Flight *f, int cls, const char *s, int *idx)
{
    int row, col;
    char c;

    if (sscanf(s, "%d%c", &row, &c) != 2) return 0;
    c = (char)toupper((unsigned char)c);
    col = c - 'A';
    if (row < 1 || col < 0 || col >= class_cols(f, cls)) return 0;
    *idx = (row - 1) * class_cols(f, cls) + col;
    return 1;
}

/* "135" -> 周一、周三、周五；"0" 或 "每天" -> 每天 */
static unsigned parse_weekmask(const char *s)
{
    unsigned m = 0;

    if (strstr(s, "每天") != NULL || strstr(s, "天天") != NULL || strcmp(s, "0") == 0)
        return 0x7Fu;

    for (; *s != '\0'; s++)
        if (*s >= '1' && *s <= '7')
            m |= 1u << (*s - '1');

    return (m != 0) ? m : 0x7Fu;
}

static void print_flight_summary(const Flight *f)
{
    char date[64], week[64];
    int days;

    if (f->serviceDate[0]) snprintf(date, sizeof date, "%s", f->serviceDate);
    else next_flight_date(f, &days, date, (int)sizeof date);
    weekmask_text(f->weekMask, week, (int)sizeof week);
    printf("%-7s %s→%s  机型 %-5s  飞行日 %s\n",
           f->flightNo, f->start, f->dest, f->planeNo, week);
    printf("        最近航班 %s\n", date);
    printf("        头等舱 余%3d ￥%6.0f | 商务舱 余%3d ￥%6.0f | 经济舱 余%3d ￥%6.0f\n",
           remaining(f, CLS_FIRST),    f->price[CLS_FIRST],
           remaining(f, CLS_BUSINESS), f->price[CLS_BUSINESS],
           remaining(f, CLS_ECONOMY),  f->price[CLS_ECONOMY]);
}

static Flight *read_flight_instance(Flight *route)
{
    char date[11];
    printf("航班日期（YYYY-MM-DD）："); read_line(date, (int)sizeof date);
    return flight_instance(route, date);
}

/* ============================================================
 *  乘客功能
 * ============================================================ */
static void ui_query(void)
{
    char key[MAX_CITY];
    int i, found = 0;

    printf("请输入起点站或终点站（直接回车显示全部航线）：");
    read_line(key, (int)sizeof key);

    printf("\n");
    for (i = 0; i < flight_count(); i++) {
        Flight *f = flight_at(i);
        if (key[0] != '\0' &&
            strcmp(key, f->start) != 0 && strcmp(key, f->dest) != 0)
            continue;
        print_flight_summary(f);
        found++;
    }
    if (found == 0) printf("没有找到与“%s”匹配的航线。\n", key);
}

static void ui_book(void)
{
    char no[MAX_NO], name[MAX_NAME], id[MAX_ID], date[11];
    char line[256], yes[16], msg[512];
    Flight *f, *rec[3];
    int cls, qty, r, i, nrec, action;
    int want[MAX_SEATS], nwant = 0;

    printf("航班号：");
    read_line(no, (int)sizeof no);
    f = find_flight(no);
    if (f == NULL) { printf("航班 %s 不存在。\n", no); return; }

    print_flight_summary(f);
    printf("航班日期（YYYY-MM-DD）："); read_line(date, (int)sizeof date);
    f = flight_instance(f, date);
    if (f == NULL) { printf("日期不合法或该航班当天不飞。\n"); return; }

    cls = read_int("舱位（0 头等舱 / 1 商务舱 / 2 经济舱）：");
    if (cls < 0 || cls >= CLASS_CNT) { printf("舱位无效。\n"); return; }

    qty = read_int("订票张数：");

    printf("姓名：");
    read_line(name, (int)sizeof name);
    printf("身份证号：");
    read_line(id, (int)sizeof id);

    /* --- 可选：自选座位 --- */
    printf("是否自己选座（y/n）：");
    read_line(yes, (int)sizeof yes);
    if (yes[0] == 'y' || yes[0] == 'Y') {
        int useGui = 0;

        if (ui_gui_available()) {
            useGui = (read_int("选座方式（1 输入座位号 / 2 图形界面点选）：") == 2);
        }

        if (useGui) {
            if (ui_pick_seats(f, cls, qty, want)) {
                nwant = qty;
            } else {
                printf("已取消图形选座，按不选座处理。\n");
                nwant = 0;
            }
        } else {
            print_seatmap(f);
            printf("请输入 %d 个座位号（空格分隔，例如 3A 3B）：", qty);
            read_line(line, (int)sizeof line);
            {
                char *tok = strtok(line, " \t");
                while (tok != NULL && nwant < MAX_SEATS) {
                    int idx;
                    if (!parse_seat(f, cls, tok, &idx)) {
                        printf("座位号 “%s” 格式不正确。\n", tok);
                        return;
                    }
                    want[nwant++] = idx;
                    tok = strtok(NULL, " \t");
                }
            }
            if (nwant != qty) {
                printf("输入的座位数（%d）与订票张数（%d）不一致。\n", nwant, qty);
                return;
            }
        }
    }

    for (;;) {
        r = book_ticket(f, name, id, cls, qty, date, want, nwant, msg, (int)sizeof msg);
        printf("%s\n", msg);
        if (r == 1) return;
        if (r != -1) return;
        nrec = recommend_flights(f, date, cls, qty, rec, 3);
        if (nrec > 0) {
            printf("以下航班在 %s 也飞往 %s 且该舱位有足够余票：\n", date, f->dest);
            for (i = 0; i < nrec; i++) print_flight_summary(rec[i]);
        } else printf("没有找到同目的地且同日有足够余票的其它航班。\n");
        action = read_int("1 修改订票张数并重试；2 按当前要求登记候补；0 结束：");
        if (action == 1) {
            do { qty = read_int("新的订票张数（1~8）："); } while (!g_eof && (qty < 1 || qty > MAX_SEATS));
            if (g_eof) return;
            nwant = 0; /* 数量改变后清除原选座，重试时自动分配 */
            continue;
        }
        if (action == 2) {
            if (add_waitlist(f, name, id, cls, qty, date)) printf("已按顺序登记候补。\n");
            else printf("候补登记失败，请检查数量、日期或内存状态。\n");
        }
        return;
    }
}

static void ui_refund(void)
{
    char no[MAX_NO], name[MAX_NAME], id[MAX_ID], date[11];
    char msg[256], wmsg[1024];
    Flight *f;
    int q = 0, n, cls = -1, cursor = 0, candidate, candidateQty, remainingRefund;

    printf("航班号：");
    read_line(no, (int)sizeof no);
    f = find_flight(no);
    if (f == NULL) { printf("航班 %s 不存在。\n", no); return; }

    print_flight_summary(f);
    printf("退票日期（YYYY-MM-DD）："); read_line(date, (int)sizeof date);
    f = flight_instance(f, date);
    if (f == NULL) { printf("日期不合法或该航班当天不飞。\n"); return; }
    printf("姓名：");
    read_line(name, (int)sizeof name);
    printf("身份证号：");
    read_line(id, (int)sizeof id);

    if (!refund_ticket(f, name, id, date, &q, &cls, msg, (int)sizeof msg)) {
        printf("%s\n", msg);
        return;
    }
    printf("%s\n", msg);

    remainingRefund = q;
    while ((n = process_waitlist(f, date, cls, remainingRefund, cursor, &candidate, &candidateQty, wmsg, (int)sizeof wmsg)) > 0) {
        char answer[16], result[256];
        printf("%s，是否办理（y/n）：", wmsg); read_line(answer, (int)sizeof answer);
        if (answer[0] == 'y' || answer[0] == 'Y') remainingRefund -= candidateQty;
        confirm_waiter(f, candidate, answer[0] == 'y' || answer[0] == 'Y', result, (int)sizeof result);
        printf("%s\n", result);
        cursor = candidate;
    }
    if (wmsg[0]) printf("%s\n", wmsg);
    printf("退票后的余票：头等舱 %d，商务舱 %d，经济舱 %d。\n",
           remaining(f, CLS_FIRST), remaining(f, CLS_BUSINESS), remaining(f, CLS_ECONOMY));
}

/* ============================================================
 *  管理员功能
 * ============================================================ */
static void ui_admin_new(void)
{
    char start[MAX_CITY], dest[MAX_CITY], no[MAX_NO], plane[MAX_NO];
    char wd[64], line[64];
    int cap[CLASS_CNT] = { 8, 24, 60 };
    double price[CLASS_CNT];
    unsigned mask;
    Flight *f;
    int c;

    printf("起点站：");
    read_line(start, (int)sizeof start);
    printf("终点站：");
    read_line(dest, (int)sizeof dest);
    printf("航班号：");
    read_line(no, (int)sizeof no);
    printf("飞机号：");
    read_line(plane, (int)sizeof plane);
    printf("飞行日（输入 1-7，如 135 表示周一三五；0 表示每天）：");
    read_line(wd, (int)sizeof wd);
    mask = parse_weekmask(wd);

    for (c = 0; c < CLASS_CNT; c++) {
        printf("%s票价：", class_name(c));
        read_line(line, (int)sizeof line);
        price[c] = atof(line);
        if (price[c] <= 0.0) price[c] = 1.0;
    }

    f = admin_new_flight(start, dest, no, plane, mask, cap, price);
    if (f == NULL) {
        printf("新建失败：航班号重复、航线数已满或参数不合法。\n");
        return;
    }
    printf("已新建航线：%s %s→%s，定额 头等 8 / 商务 24 / 经济 60。\n",
           f->flightNo, f->start, f->dest);
}

static void ui_admin_modify(void)
{
    char no[MAX_NO], msg[256], line[64];
    Flight *f;
    int choice, cls, cap;

    printf("要修改的航班号：");
    read_line(no, (int)sizeof no);
    f = find_flight(no);
    if (f == NULL) { printf("航班 %s 不存在。\n", no); return; }

    print_flight_summary(f);
    printf("\n1. 修改乘员定额\n2. 修改票价\n3. 修改飞行日\n4. 修改飞机号\n0. 返回\n");
    choice = read_int("请选择：");

    switch (choice) {
    case 1:
        cls = read_int("舱位（0/1/2）：");
        if (cls < 0 || cls >= CLASS_CNT) { printf("舱位无效。\n"); return; }
        cap = read_int("新定额：");
        {
            char date[11];
            Flight *inst;
            printf("航班日期（YYYY-MM-DD）："); read_line(date, (int)sizeof date);
            inst = flight_instance(f, date);
            if (inst == NULL) { printf("日期不合法或该航班当天不飞。\n"); return; }
            if (admin_set_capacity(inst, cls, cap, msg, (int)sizeof msg)) printf("%s（%s）\n", msg, date);
            else printf("修改失败：%s\n", msg);
        }
        break;

    case 2:
        cls = read_int("舱位（0/1/2）：");
        if (cls < 0 || cls >= CLASS_CNT) { printf("舱位无效。\n"); return; }
        printf("新票价：");
        read_line(line, (int)sizeof line);
        f->price[cls] = atof(line);
        printf("%s票价已改为 ￥%.0f。\n", class_name(cls), f->price[cls]);
        break;

    case 3:
        printf("新的飞行日（1-7，如 135；0 表示每天）：");
        read_line(line, (int)sizeof line);
        f->weekMask = parse_weekmask(line);
        printf("飞行日已更新。\n");
        break;

    case 4:
        printf("新飞机号：");
        read_line(line, (int)sizeof line);
        if (strcmp(f->planeNo, line) != 0) {
            Flight tmp = *f;
            int c2, bad = 0;

            if (f->bookings != NULL) {
                printf("该航班已有订票记录，不能更换机型（座位号会错乱）。请先办理退票。\n");
                break;
            }
            snprintf(tmp.planeNo, MAX_NO, "%.*s", MAX_NO - 1, line);
            flight_apply_plane(&tmp);
            for (c2 = 0; c2 < CLASS_CNT; c2++)
                if (f->cap[c2] > MAX_ROWS * tmp.cols[c2]) bad = 1;
            if (bad) {
                printf("该航班的定额超出新机型的座位上限，请先调小定额再换机型。\n");
                break;
            }
        }
        snprintf(f->planeNo, MAX_NO, "%.*s", MAX_NO - 1, line);
        flight_apply_plane(f);
        printf("飞机号已改为 %s。\n", f->planeNo);
        break;

    default:
        break;
    }
}

static void ui_admin_records(void)
{
    char no[MAX_NO];
    Flight *f;

    printf("航班号（直接回车打印全部航班）：");
    read_line(no, (int)sizeof no);

    if (no[0] == '\0') {
        print_all_records();
        return;
    }
    f = find_flight(no);
    if (f == NULL) { printf("航班 %s 不存在。\n", no); return; }
    print_bookings(f);
}

static void ui_seatmap(void)
{
    char no[MAX_NO];
    Flight *f;

    printf("航班号：");
    read_line(no, (int)sizeof no);
    f = find_flight(no);
    if (f == NULL) { printf("航班 %s 不存在。\n", no); return; }
    f = read_flight_instance(f);
    if (f == NULL) { printf("日期不合法或该航班当天不飞。\n"); return; }
    print_seatmap(f);
}

static void ui_graph(void)
{
    char no[MAX_NO];
    Flight *f;

    printf("航班号：");
    read_line(no, (int)sizeof no);
    f = find_flight(no);
    if (f == NULL) { printf("航班 %s 不存在。\n", no); return; }
    f = read_flight_instance(f);
    if (f == NULL) { printf("日期不合法或该航班当天不飞。\n"); return; }

    if (ui_show_seat_window(f) == 0) {
        printf("（当前为控制台版本，改用字符座位图显示）\n");
        print_seatmap(f);
    }
}

static void ui_board(void)
{
    char no[MAX_NO], log[8192];
    Flight *f;

    printf("航班号：");
    read_line(no, (int)sizeof no);
    f = find_flight(no);
    if (f == NULL) { printf("航班 %s 不存在。\n", no); return; }
    f = read_flight_instance(f);
    if (f == NULL) { printf("日期不合法或该航班当天不飞。\n"); return; }

    print_bookings(f);
    simulate_boarding(f, log, (int)sizeof log);
    printf("%s", log);
}

static void ui_demo(void)
{
    char no[MAX_NO], log[512];
    Flight *f;
    int cls;

    printf("航班号：");
    read_line(no, (int)sizeof no);
    f = find_flight(no);
    if (f == NULL) { printf("航班 %s 不存在。\n", no); return; }
    f = read_flight_instance(f);
    if (f == NULL) { printf("日期不合法或该航班当天不飞。\n"); return; }

    cls = read_int("要把哪个舱位订满并超售（0/1/2）：");
    if (cls < 0 || cls >= CLASS_CNT) { printf("舱位无效。\n"); return; }

    demo_overbook(f, cls, log, (int)sizeof log);
    printf("%s\n", log);
    printf("现在可以执行 9 号功能“模拟登机”查看超售调整结果。\n");
}

/* ============================================================
 *  主菜单
 * ============================================================ */
static void show_menu(void)
{
    printf("\n============================== 航空客运订票系统 ==============================\n");
    printf("  【乘客】 1. 查询航线    2. 订票    3. 退票\n");
    printf("  【管理员】4. 新建航线    5. 修改航线    6. 查看订票记录\n");
    printf("           7. 座位图(字符) 8. 座位图(EasyX) 9. 模拟登机   10. 超售演示\n");
    printf("  0. 退出\n");
    printf("=============================================================================\n");
}

int main(void)
{
    int choice;

    init_console();
    airline_init();

    printf("欢迎使用航空客运订票系统（已载入 %d 条航线）。\n", flight_count());

    for (;;) {
        show_menu();
        choice = read_int("请选择操作：");
        if (g_eof) break;

        switch (choice) {
        case 1:  ui_query();        break;
        case 2:  ui_book();         break;
        case 3:  ui_refund();       break;
        case 4:  ui_admin_new();    break;
        case 5:  ui_admin_modify(); break;
        case 6:  ui_admin_records();break;
        case 7:  ui_seatmap();      break;
        case 8:  ui_graph();        break;
        case 9:  ui_board();        break;
        case 10: ui_demo();         break;
        case 0:  printf("谢谢使用，再见！\n"); return 0;
        default: printf("无效选项，请重新输入。\n"); break;
        }
    }
    printf("\n输入结束，程序退出。\n");
    return 0;
}
