#include "keyboard.h"
#include "idt.h"
#include "../kernel.h"
#include <stdint.h>
#include "../shell.h"
#include "../scrollback.h"

#define KEYBOARD_IRQ  33
#define PIC1_CMD      0x20
#define PIC1_DATA     0x21
#define PIC2_CMD      0xA0
#define PIC2_DATA     0xA1
#define KB_DATA_PORT  0x60
#define SC_UP         0x48
#define SC_DOWN       0x50
#define SC_LEFT       0x4B
#define SC_RIGHT      0x4D
#define SC_PGUP       0x49
#define SC_PGDN       0x51

/* scancodes for shift keys */
#define SC_LSHIFT      0x2A
#define SC_RSHIFT      0x36
#define SC_LSHIFT_REL  0xAA  /* 0x2A | 0x80 */
#define SC_RSHIFT_REL  0xB6  /* 0x36 | 0x80 */

static int e0_prefix = 0;
static int shift_held = 0;

static const char scancode_map[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,  '\\','z','x','c','v','b','n','m',',','.','/',0,
    '*', 0, ' '
};

static const char scancode_map_shift[128] = {
    0,  27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,  'A','S','D','F','G','H','J','K','L',':','"','~',
    0,  '|','Z','X','C','V','B','N','M','<','>','?',0,
    '*', 0, ' '
};

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void pic_remap(void) {
    outb(PIC1_CMD,  0x11);
    outb(PIC2_CMD,  0x11);
    outb(PIC1_DATA, 0x20);
    outb(PIC2_DATA, 0x28);
    outb(PIC1_DATA, 0x04);
    outb(PIC2_DATA, 0x02);
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);
    outb(PIC1_DATA, 0xFD);
    outb(PIC2_DATA, 0xFF);
}

extern void keyboard_handler_asm(void);

void keyboard_handler(void) {
    uint8_t scancode = inb(KB_DATA_PORT);
    outb(PIC1_CMD, 0x20);
    /* extend key prefix */
    if (scancode == 0xE0) {
        e0_prefix = 1;
        return;
    }
    if (e0_prefix) {
        e0_prefix = 0;
        if (scancode == SC_UP) {
            if (shift_held) scrollback_page_up();
            else shell_history_up();
            return;
        }
        if (scancode == SC_DOWN) {
            if (shift_held)  scrollback_page_down();
            else shell_history_down();
            return;
        }
        if (scancode == SC_LEFT)  { shell_cursor_left();    return; }
        if (scancode == SC_RIGHT) { shell_cursor_right();   return; }
        return;
    }
    if (scancode == SC_LSHIFT || scancode == SC_RSHIFT) { shift_held = 1; return; }
    if (scancode == SC_LSHIFT_REL || scancode == SC_RSHIFT_REL) { shift_held = 0; return; }
    if (scancode & 0x80) return;
    if (scancode >= 128) return;
    char c = shift_held ? scancode_map_shift[scancode] : scancode_map[scancode];
    if (c == 0) return;
    shell_handle_key(c);
}

void keyboard_install(void) {
    pic_remap();
    idt_set_gate(KEYBOARD_IRQ, (uint32_t)keyboard_handler_asm, 0x08, 0x8E);
}
