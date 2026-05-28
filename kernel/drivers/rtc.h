#ifndef RTC_H
#define RTC_H

#include <stdint.h>

typedef struct {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day;
    uint8_t month;
    uint8_t year; /* two digit's 2000 */
} rtc_time_t;

void     rtc_read(rtc_time_t *t);
uint32_t rtc_timestamp(void); /* seconds since 2000-01-01, good enough */

#endif
