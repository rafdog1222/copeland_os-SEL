#include "idt.h"
#include <stdint.h>

#define IDT_ENTRIES 256

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idtp;

extern void idt_load(uint32_t);
extern void isr_stub_asm(void);   /* qn proper iret stub */

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low  = (base & 0xFFFF);
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].sel       = sel;
    idt[num].always0   = 0;
    idt[num].flags     = flags;
}

void idt_install(void) {
    idtp.limit = (sizeof(struct idt_entry) * IDT_ENTRIES) - 1;
    idtp.base  = (uint32_t)&idt;

    for (int i = 0; i < IDT_ENTRIES; i++)
        idt_set_gate(i, (uint32_t)isr_stub_asm, 0x08, 0x8E);

    idt_load((uint32_t)&idtp);
}
