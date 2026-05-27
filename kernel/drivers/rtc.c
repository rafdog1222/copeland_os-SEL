/*  Ports 0x70\0x71, values in BCD format */

#include "rtc.h"

#define CMOS_ADDR  0x70
#define CMOS_DATA  0x71

#define RTC_SECONDS  0x00
#define RTC_MINUTES  0x02
#define RTC_HOURS    0x04
#define RTC_DAY      0x07
#define RTC_MONTH    0x08
#define RTC_YEAR     0x09
#define RTC_STATUS_B 0x0B   /* bit 2's = binary mode, bit 1 = 24hrs */

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t r;
    __asm__ volatile ("inb %1, %0" : "=a"(r) : "Nd"(port));
    return r;
}

static uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

static int rtc_updating(void) {
    outb(CMOS_ADDR, 0x0A);
    return inb(CMOS_DATA) & 0x80;  /* bit 7 = updating in progress */
}

/* BCD to binary */
static uint8_t bcd(uint8_t val) {
    return (val & 0x0F) + ((val >> 4) * 10);
}

void rtc_read(rtc_time_t *t) {
    /* wait until RTC is not updating */
    while (rtc_updating());

    uint8_t status_b = cmos_read(RTC_STATUS_B);
    int is_binary = status_b & 0x04;
    int is_24hr   = status_b & 0x02;

    t->seconds = cmos_read(RTC_SECONDS);
    t->minutes = cmos_read(RTC_MINUTES);
    t->hours   = cmos_read(RTC_HOURS);
    t->day     = cmos_read(RTC_DAY);
    t->month   = cmos_read(RTC_MONTH);
    t->year    = cmos_read(RTC_YEAR);

    /* convert BCD to binary if needed */
    if (!is_binary) {
        t->seconds = bcd(t->seconds);
        t->minutes = bcd(t->minutes);
        t->hours   = bcd(t->hours);
        t->day     = bcd(t->day);
        t->month   = bcd(t->month);
        t->year    = bcd(t->year);
    }

    /* convert 12hr to 24hr if needed */
    if (!is_24hr && (t->hours & 0x80)) {
        t->hours = ((t->hours & 0x7F) + 12) % 24;
    }
}

/* rough seconds since 2000-01-01, good enough for signal timestamps */
uint32_t rtc_timestamp(void) {
    rtc_time_t t;
    rtc_read(&t);

    /* days per month ignoring leap years they don't exist lol */
    static const uint16_t days_before[13] = {
        0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };

    uint32_t years = t.year;                          /* years since 2000 */
    uint32_t leap_days = (years + 3) / 4;             /* okay fine, rough leap years */
    uint32_t days = years * 365 + leap_days;
    days += days_before[t.month < 13 ? t.month : 0];
    days += t.day - 1;

    return days * 86400
         + t.hours   * 3600
         + t.minutes * 60
         + t.seconds;
}
