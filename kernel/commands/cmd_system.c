#include "cmd_system.h"
#include "commands.h"
#include "../kernel.h"
#include "../shell.h"
#include "../drivers/rtc.h"
#include <stdint.h>

/* shell.h for cmd_buf access in echo */
extern char    cmd_buf[];
extern uint32_t cmd_len;

void cmd_help(void) {
    vga_print("\n", 0x0F00);
    vga_print("  copeland_os-SEL commands\n", 0x0D00);
    vga_print("  -------------------------\n", 0x0800);
    for (int i = 0; i < command_count; i++) {
        vga_print("  ", 0x0F00);
        vga_print(command_table[i].name, 0x0B00);
        vga_print(" - ", 0x0800);
        vga_print(command_table[i].desc, 0x0700);
        vga_print("\n", 0x0F00);
    }
    vga_print("\n", 0x0F00);
}

void cmd_clear(void) {
    vga_clear();
    vga_print("copeland_os-SEL\n", 0x0B00);
    vga_print("present day, present time...\n\n", 0x0800);
}

void cmd_about(void) {
    vga_print("\n", 0x0F00);
    vga_print("  copeland_os-SEL\n", 0x0D00);
    vga_print("  -------------------------\n", 0x0800);
    vga_print("  arch    : x86 32-bit bare metal\n", 0x0700);
    vga_print("  kernel  : monolithic, hand-rolled\n", 0x0700);
    vga_print("  boot    : GRUB multiboot\n", 0x0700);
    vga_print("  spirit  : Copeland OS + Serial Experiments Lain\n", 0x0700);
    vga_print("  author  : rafdog\n", 0x0700);
    vga_print("  license : GNU GPL v2\n", 0x0700);
    vga_print("  status  : v0.2.0 - the Wired has memory\n\n", 0x0B00);
}

void cmd_fetch(void) {
    vga_print("\n", 0x0F00);
    vga_print("  .+*+.     copeland_os-SEL\n",           0x0D00);
    vga_print("  |   |     -------------------------\n",  0x0800);
    vga_print("  |SEL|     os      : copeland_os-SEL\n", 0x0B00);
    vga_print("  |   |     arch    : x86 32-bit\n",      0x0B00);
    vga_print("  '+*+'     kernel  : hand-rolled C\n",   0x0B00);
    vga_print("            shell   : copeland-shell\n",  0x0B00);
    vga_print("            spirit  : SEL / Copeland\n",  0x0D00);
    vga_print("            author  : rafdog\n\n",    0x0700);
}

void cmd_echo(void) {
    if (cmd_len > 5) {
        vga_print("\n  ", 0x0F00);
        for (uint32_t i = 5; i < cmd_len; i++)
            vga_putchar(cmd_buf[i], 0x0F00);
        vga_print("\n\n", 0x0F00);
    } else {
        vga_print("\n  echo: nothing to say\n\n", 0x0800);
    }
}

static void print2(uint8_t n, unsigned short color) {
    vga_putchar('0' + n/10, color);
    vga_putchar('0' + n%10, color);
}

void cmd_time(void) {
    rtc_time_t t;
    rtc_read(&t);
    vga_print("\n  ", 0x0F00);
    print2(t.day,     0x0B00); vga_putchar('/', 0x0800);
    print2(t.month,   0x0B00); vga_putchar('/', 0x0800);
    vga_putchar('2',  0x0B00);
    vga_putchar('0',  0x0B00);
    print2(t.year,    0x0B00);
    vga_print("  ", 0x0F00);
    print2(t.hours,   0x0F00); vga_putchar(':', 0x0800);
    print2(t.minutes, 0x0F00); vga_putchar(':', 0x0800);
    print2(t.seconds, 0x0F00);
    vga_print("\n\n", 0x0F00);
}















