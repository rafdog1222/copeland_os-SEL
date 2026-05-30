#ifndef KERNEL_H
#define KERNEL_H
#define VGA_HEIGHT 25
#define VGA_WIDTH  80
#include <stdint.h>

void vga_putchar(char c, unsigned short color);
void vga_print(const char *str, unsigned short color);
void vga_backspace(void);
void vga_clear(void);
void vga_update_cursor(void);
void vga_enable_cursor(void);
void vga_save_prompt_pos(void); 
void vga_set_cursor_offset(uint32_t offset);
void vga_clear_raw(void);
void vga_put_raw(int col, int row, char c, uint8_t color);
void vga_set_write_pos(unsigned int x, unsigned int y);
unsigned int vga_get_prompt_offset(void);
void vga_scrollback_restore(void);
void vga_move_back(uint32_t n);
void vga_print_dec(uint32_t val);
void vga_print_hex(uint32_t val);


#endif
