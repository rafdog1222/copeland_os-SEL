#ifndef KERNEL_H
#define KERNEL_H
#include <stdint.h>

void vga_putchar(char c, unsigned short color);
void vga_print(const char *str, unsigned short color);
void vga_backspace(void);
void vga_clear(void);
void vga_update_cursor(void);
void vga_enable_cursor(void);
void vga_save_prompt_pos(void); 
void vga_set_cursor_offset(uint32_t offset);
void vga_move_back(uint32_t n);
void vga_print_dec(uint32_t val);
void vga_print_hex(uint32_t val);


#endif
