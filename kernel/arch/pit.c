#include "pit.h"
#include "idt.h"
#include "../kernel.h"
#include <stdint.h>

#define PIT_CHANNEL0  0x40   /* channel 0 data port */
#define PIT_CMD       0x43   /* command register */
#define PIC1_CMD      0x20
#define TIMER_IRQ     32     /* IRQ0 remapped to interrupt 32 */

static volatile uint32_t tick_count = 0;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

void pit_handler(void) {
    tick_count++;
    outb(PIC1_CMD, 0x20);   /* EOI, tell pic we handled it */
}

uint32_t pit_ticks(void) {
    return tick_count;
}

extern void pit_handler_asm(void);   /* in pit_asm.asm */

void pit_install(void) {
    uint32_t divisor = 1193182 / PIT_HZ;
    /* command byte, channel 0, lobyte/hibyte, rate generator mode */
    outb(PIT_CMD, 0x36);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
    /* wire IRQ0 (interrupt 32) to my handler */
    idt_set_gate(TIMER_IRQ, (uint32_t)pit_handler_asm, 0x08, 0x8E);
    /* unmask IRQ0 on PIC, currently we only unmask IRQ1 (keyboard)
     * need to unmask IRQ0 as well */
    uint8_t mask;
    __asm__ volatile ("inb $0x21, %0" : "=a"(mask));
    mask &= ~0x01;   /* clear bit 0 = unmask IRQ0 */
    __asm__ volatile ("outb %0, $0x21" : : "a"(mask));
}
