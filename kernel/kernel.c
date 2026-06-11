#define VGA_BUFFER ((volatile unsigned short *)0xB8000)
#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define WHITE_ON_BLACK 0x0F00
#define CYAN_ON_BLACK  0x0B00

#include "kernel.h"
#include "arch/gdt.h"
#include "arch/idt.h"
#include "arch/keyboard.h"
#include <stdint.h>
#include "shell.h"
#include "mm/pmm.h"
#include "mm/heap.h"
#include "drivers/ata.h"
#include "fs/signal_format.h"
#include "../signal.h"


extern signal_fs_t g_fs;
extern int g_fs_mounted;
extern int  g_fs_inited;

static void vga_scroll(void);
static unsigned int cursor_x = 0;
static unsigned int cursor_y = 0;
static unsigned int prompt_offset = 0;
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void vga_putchar(char c, unsigned short color)
{
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= VGA_HEIGHT) {
            vga_scroll();
        }
        vga_update_cursor();
        return;
    }
    VGA_BUFFER[cursor_y * VGA_WIDTH + cursor_x] =
        color | (unsigned char)c;
    cursor_x++;
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= VGA_HEIGHT) {
            vga_scroll();
        }
    }
    vga_update_cursor();
}

static void vga_scroll(void) {
    for (unsigned int y = 1; y < VGA_HEIGHT; y++) {
        for (unsigned int x = 0; x < VGA_WIDTH; x++) {
            VGA_BUFFER[(y - 1) * VGA_WIDTH + x] =
                VGA_BUFFER[y * VGA_WIDTH + x];
        }
    }
    for (unsigned int x = 0; x < VGA_WIDTH; x++) {
        VGA_BUFFER[(VGA_HEIGHT - 1) * VGA_WIDTH + x] =
            WHITE_ON_BLACK | ' ';
    }
    cursor_y = VGA_HEIGHT - 1;
}

void vga_backspace(void) {
    if (cursor_x > 0) {
        cursor_x--;
        VGA_BUFFER[cursor_y * VGA_WIDTH + cursor_x] = 0x0F00 | ' ';
        vga_update_cursor();
    }
}

void vga_print(const char *str, unsigned short color) {
    while (*str)
        vga_putchar(*str++, color);
}

void vga_print_dec(uint32_t val) {
    if (val == 0) { vga_putchar('0', 0x0F00); return; }
    char buf[12]; int i = 0;
    while (val) { buf[i++] = '0' + (val % 10); val /= 10; }
    while (i--) vga_putchar(buf[i], 0x0F00);
}

void vga_print_hex(uint32_t val) {
    const char *h = "0123456789ABCDEF";
    vga_print("0x", 0x0F00);
    for (int i = 28; i >= 0; i -= 4)
    vga_putchar(h[(val >> i) & 0xF], 0x0F00);
}

void vga_clear_raw(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA_BUFFER[i] = 0x0700 | ' ';
}

void vga_put_raw(int col, int row, char c, uint8_t color) {
    VGA_BUFFER[row * VGA_WIDTH + col] = ((uint16_t)color << 8) | (uint8_t)c;
}

unsigned int vga_get_prompt_offset(void) {
    return prompt_offset;
}

void vga_set_write_pos(unsigned int x, unsigned int y) {
    cursor_x = x;
    cursor_y = y;
}

void vga_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA_BUFFER[i] = WHITE_ON_BLACK | ' ';
    cursor_x = 0;
    cursor_y = 0;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

void vga_update_cursor(void) {
    unsigned int pos = cursor_y * VGA_WIDTH + cursor_x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_enable_cursor(void) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | 13);
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | 15);
}

static void k_delay(uint32_t count) {
    while (count--) {
        __asm__ volatile ("pause");  /* x86 pause hint, can't be optimized away */
    }
}

void vga_save_prompt_pos(void) {
    prompt_offset = cursor_y * VGA_WIDTH + cursor_x;
}

void vga_set_cursor_offset(uint32_t offset_from_prompt) {
    unsigned int pos = prompt_offset + offset_from_prompt;
    cursor_x = pos % VGA_WIDTH;
    cursor_y = pos / VGA_WIDTH;
    vga_update_cursor();
}

void vga_move_back(uint32_t n) {
    if (cursor_x >= n) {
        cursor_x -= n;
    } else {
        cursor_y--;
        cursor_x =  VGA_WIDTH - (n - cursor_x);
    }
}



static void boot_splash(void) {
    vga_clear();

    vga_print("\n\n\n", 0x0F00);
    vga_print("        ======================\n", 0x0B00);
    vga_print("        -   CopeLand_os-SEL  -\n", 0x0B00);
    vga_print("        ======================\n", 0x0B00);
    k_delay(2000000);

    vga_print("        present day, present time...\n", 0x0800);
    k_delay(4000000);
    vga_print("\n\n", 0x0F00);

    vga_print("\n", 0x0F00);
    vga_print("        system", 0x0800);
    k_delay(3000000);
    vga_print(".", 0x0800);
    k_delay(3700000);
    vga_print(".", 0x0800);
    k_delay(3500000);
    vga_print(". ", 0x0800);
    k_delay(1200000);
    vga_print("ok\n", 0x0A00);
    k_delay(200000);

    vga_print("        memory", 0x0800);
    k_delay(3000000);
    vga_print(".", 0x0800);
    k_delay(3700000);
    vga_print(".", 0x0800);
    k_delay(3500000);
    vga_print(". ", 0x0800);
    k_delay(1200000);
    vga_print("ok\n", 0x0A00);
    k_delay(200000);

    vga_print("        signal", 0x0800);
    k_delay(3000000);
    vga_print(".", 0x0800);
    k_delay(3700000);
    vga_print(".", 0x0800);
    k_delay(3500000);
    vga_print(". ", 0x0800);
}

static void boot_splash_signal_ok(void) {
    vga_print("ok\n", 0x0A00);
    k_delay(200000);
    vga_print("        wired", 0x0800);
    k_delay(3000000);
    vga_print(".", 0x0800);
    k_delay(3700000);
    vga_print(".", 0x0800);
    k_delay(3500000);
    vga_print(". ", 0x0800);
    k_delay(400000);
    vga_print("connected\n", 0x0A00);
    k_delay(800000);
    vga_clear();
}

static void boot_splash_signal_fail(void) {
    vga_print("failed\n", 0x0C00);
    k_delay(2000000);
    vga_print("        run mkfs to initialise signal\n", 0x0800);
    k_delay(8000000);
    vga_clear();
}

struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;   /* kb below 1MB */
    uint32_t mem_upper;   /* kb above 1MB */
    /* we don't need the rest for now~ */
};

void kernel_main(uint32_t magic, struct multiboot_info *mbi) {
    vga_clear();
    vga_enable_cursor();
    gdt_install();
    idt_install();
    keyboard_install();
    if (magic == 0x2BADB002 && mbi)
        pmm_init(mbi->mem_upper);
    heap_init();
    /* boot splash start, signal line stays open */
    boot_splash();
    ata_init();
    ata_set_partition_offset(0);
    signal_init(&g_fs, ata_read_block, ata_write_block);
    g_fs_inited = 1;
    int mnt = signal_mount(&g_fs);
    if (mnt == SIGNAL_OK) {
        g_fs_mounted = 1;
        boot_splash_signal_ok();
    } else {
        boot_splash_signal_fail();
    }
    /* shell takes over on cl'ing the screen */
    vga_print("copeland_os-SEL\n", 0x0B00);
    vga_print("present day, present time...\n\n", 0x0800);
    vga_print("Welcome to the Wired.\n\n", 0x0B00);
    shell_init();
    __asm__ volatile ("sti");
    for (;;) { __asm__ volatile ("hlt"); }
}
