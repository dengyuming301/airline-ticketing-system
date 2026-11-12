#ifndef AIRLINE_H
#define AIRLINE_H

#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_CITY     24      /* 城市名长度 */
#define MAX_NO       16      /* 航班号/飞机号长度 */
#define MAX_NAME     32      /* 姓名长度 */
#define MAX_ID       24      /* 身份证号长度 */
#define MAX_SEATS    8       /* 单次最多订几张票 */
#define MAX_FLIGHTS  200     /* 航线容量 */
#define CLASS_CNT    3       /* 舱位种类数 */
#define MAX_ROWS     20      /* 每个舱位最多行数 */
#define MAX_COLS     9       /* 每排最多座位数（宽体机 3-3-3） */
#define SEAT_NONE    (-1)    /* 超售无座 */

/* 舱位：0 头等舱，1 商务舱，2 经济舱 */
typedef enum {
    CLS_FIRST    = 0,
    CLS_BUSINESS = 1,
    CLS_ECONOMY  = 2
} SeatClass;

/* 已订票客户 */
typedef struct Booking {
    int  seq;                       /* 全局订票序号，越大表示订票越晚 */
    char name[MAX_NAME];
    char id[MAX_ID];
    char date[11];                   /* YYYY-MM-DD */
    int  cls;                       /* 舱位 */
    int  qty;                       /* 订票量 */
    int  seats[MAX_SEATS];          /* 座位下标；SEAT_NONE 表示超售无座 */
    int  nseat;
    struct Booking *next;
} Booking;

/* 候补客户 */
typedef struct Waiter {
    int seq;
    char name[MAX_NAME];
    char id[MAX_ID];
    char date[11];
    int  cls;
    int  qty;
    struct Waiter *next;
} Waiter;

/* 航线 */
typedef struct Flight {
    char start[MAX_CITY];           /* 起点站名 */
    char dest[MAX_CITY];            /* 终点站名 */
    char flightNo[MAX_NO];          /* 航班号 */
    char planeNo[MAX_NO];           /* 飞机号 */
    char serviceDate[11];            /* 具体航班日期；空串表示航线模板 */
    unsigned weekMask;              /* bit0=周一 ... bit6=周日 */
    int  cap[CLASS_CNT];            /* 各舱位乘员定额 */
    int  sold[CLASS_CNT];           /* 各舱位已售(含超售) */
    int  overbook[CLASS_CNT];       /* 各舱位超售阈值 */
    double price[CLASS_CNT];        /* 各舱位票价 */
    int  cols[CLASS_CNT];           /* 各舱位每排座位数（按机型） */
    int  grp[CLASS_CNT][4];         /* 每排座位分组，如 2-4-2；0 结尾 */
    unsigned char taken[CLASS_CNT][MAX_ROWS][MAX_COLS];
    Booking *bookings;              /* 已订票客户链表(按订票顺序) */
    Waiter  *waitlist;              /* 候补队列 */
} Flight;

/* ---------------- 初始化与查询 ---------------- */
int    airline_init(void);                       /* 建立 15 条初始航线，返回条数 */
int    flight_count(void);
Flight *flight_at(int i);
Flight *find_flight(const char *flightNo);
Flight *flight_instance(Flight *route, const char *date); /* 获取该航班日期独立库存 */
int flight_instance_count(void);
Flight *flight_instance_at(int index);
int    remaining(const Flight *f, int cls);      /* 余票量 = 定额 - 已售，最小 0 */
int    capacity_total(const Flight *f);
const char *class_name(int cls);
const char *airline_name(const char *flightNo);  /* 按航班号前缀取航空公司名 */
unsigned    airline_color(const char *flightNo); /* 返回 0xRRGGBB 品牌色 */
int         airline_total(void);
const char *airline_code_at(int i);
const char *airline_name_at(int i);
int    class_cols(const Flight *f, int cls);     /* 该航班该舱位每排座位数 */
int    class_rows(const Flight *f, int cls);
int    seat_label(const Flight *f, int cls, int idx, char *buf);  /* 下标 -> "3A" */

/* 根据飞机号设置座位布局（窄体 3-3，宽体 2-4-2 / 3-3-3） */
void   flight_apply_plane(Flight *f);

/* 返回 1 并写入最近一天航班日期；0 表示一周内无航班 */
int    next_flight_date(const Flight *f, int *daysLater, char *desc, int descLen);

/* 把 weekMask 转成 "周一、周三、周五" / "每天" */
void   weekmask_text(unsigned mask, char *buf, int bufLen);

/* ---------------- 乘客业务 ---------------- */
/* 返回值：1 成功；0 参数错误；-1 余票不足(可候补) */
int  book_ticket(Flight *f, const char *name, const char *id, int cls, int qty,
                 const char *date, const int *wantSeats, int nWant, char *out, int outLen);

/* 按姓名+身份证号退票，refundedQty 返回退票张数 */
int  refund_ticket(Flight *f, const char *name, const char *id,
                   const char *date, int *refundedQty, int *refundedClass, char *out, int outLen);

int  add_waitlist(Flight *f, const char *name, const char *id, int cls, int qty, const char *date);
int  process_waitlist(Flight *f, const char *date, int cls, int refundedQty,
                      int afterSeq, int *candidateSeq, int *candidateQty, char *out, int outLen);
int  confirm_waiter(Flight *f, int candidateSeq, int accept, char *out, int outLen);

/* 查找与 src 同目的地且有余票的其它航班，返回条数 */
int  recommend_flights(const Flight *src, const char *date, int cls, int qty,
                       Flight **out, int max);

/* ---------------- 管理员业务 ---------------- */
Flight *admin_new_flight(const char *start, const char *dest, const char *flightNo,
                         const char *planeNo, unsigned weekMask,
                         const int cap[CLASS_CNT], const double price[CLASS_CNT]);

int  admin_set_capacity(Flight *f, int cls, int newCap, char *out, int outLen);

void print_bookings(const Flight *f);
void print_seatmap(const Flight *f);
void print_all_records(void);

/* 模拟登机：超员时先升舱，仍超员则强制退票；结果写入 out */
void simulate_boarding(Flight *f, char *out, int outLen);

/* 演示用：把某舱位自动订到 定额+超售阈值；结果写入 out */
void demo_overbook(Flight *f, int cls, char *out, int outLen);

/* ---------------- 图形界面 ---------------- */
/* easyx_view.c 中实现；未启用 EasyX 时给出提示并返回 0 */
int  ui_show_seat_window(Flight *f);

/* 当前可执行文件是否带 EasyX 图形界面 */
int  ui_gui_available(void);

/* 图形界面选座：点击 qty 个空闲座位后回车确认；返回 1 确认，0 取消 */
int  ui_pick_seats(Flight *f, int cls, int qty, int *seats);

#ifdef USE_EASYX
/* 供图形主界面复用的座位图绘制与命中判断 */
void seat_view_draw(Flight *f, int selCls, const int *sel, int nsel,
                    int pickQty, const char *hint);
int  seat_view_hit(Flight *f, int cls, int mx, int my);
#endif

#ifdef __cplusplus
}
#endif

#endif /* AIRLINE_H */
