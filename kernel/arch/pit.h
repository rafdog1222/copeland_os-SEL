/* kernel/arch/pit.h */
#ifndef PIT_H
#define PIT_H

#include <stdint.h>

#define PIT_HZ      100     /* fire 100 times per second = 10ms intervals */

void     pit_install(void);
uint32_t pit_ticks(void);   /* total ticks since boot */

#endif
