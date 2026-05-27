#include "cmd_mem.h"
#include "../kernel.h"
#include "../mm/pmm.h"
#include "../mm/heap.h"

static void vga_print_num(uint32_t n, unsigned short color) {
    if (n == 0) { vga_putchar('0', color); return; }
    char buf[12];
    int i = 0;
    while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    while (--i >= 0) vga_putchar(buf[i], color);
}

void cmd_mem(void) {
    uint32_t heap_used, heap_free;
    heap_stats(&heap_used, &heap_free);
    vga_print("\n  memory map\n", 0x0D00);
    vga_print("  -------------------------\n", 0x0800);
    vga_print("  total pages : ", 0x0700);
    vga_print_num(pmm_total_pages(), 0x0F00);
    vga_print("\n  free pages  : ", 0x0700);
    vga_print_num(pmm_free_pages(), 0x0A00);
    vga_print("\n  used pages  : ", 0x0700);
    vga_print_num(pmm_total_pages() - pmm_free_pages(), 0x0C00);
    vga_print("\n  page size   : 4096 bytes\n", 0x0700);
    vga_print("  free RAM    : ", 0x0700);
    vga_print_num(pmm_free_pages() * 4, 0x0A00);
    vga_print(" KB\n", 0x0700);
    vga_print("\n  heap used   : ", 0x0700);
    vga_print_num(heap_used, 0x0C00);
    vga_print(" B\n  heap free   : ", 0x0700);
    vga_print_num(heap_free, 0x0A00);
    vga_print(" B\n\n", 0x0700);
}
