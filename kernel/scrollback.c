#include "scrollback.h"
#include "kernel.h"
#include "../kernel/mm/heap.h"
#include <stdint.h>

typedef struct {
    char    text[SCROLLBACK_WIDTH];
    uint8_t colors[SCROLLBACK_WIDTH];  
} sb_line_t;

static sb_line_t *sb_buf   = 0;   
static int        sb_head  = 0;
static int        sb_count = 0;   
static int        sb_view  = 0; 
static int       sb_active = 0;

void scrollback_start(void) { sb_active = 1; }

void scrollback_init(void) {
    sb_buf = (sb_line_t *)kmalloc(SCROLLBACK_LINES * sizeof(sb_line_t));
    if (!sb_buf) return;
    for (int i = 0; i < SCROLLBACK_LINES; i++) {
        for (int j = 0; j < SCROLLBACK_WIDTH; j++) {
            sb_buf[i].text[j]   = ' ';
            sb_buf[i].colors[j] = 0x07;
        }
    }
}

void scrollback_push(const char *line, const uint8_t *colors) {
    if (!sb_buf || !sb_active) return;
    int idx = (sb_head + sb_count) % SCROLLBACK_LINES;
    if (sb_count == SCROLLBACK_LINES) {
        sb_head = (sb_head + 1) % SCROLLBACK_LINES;
    } else {
        sb_count++;
    }
    for (int i = 0; i < SCROLLBACK_WIDTH; i++) {
        sb_buf[idx].text[i]   = line[i];
        sb_buf[idx].colors[i] = colors ? colors[i] : 0x07;
    }
}

int scrollback_is_active(void) {
    return sb_view > 0;
}

static void scrollback_redraw(void) {
    vga_clear_raw();   
    int lines_to_show = VGA_HEIGHT - 1;
    int start = sb_count - (sb_view * lines_to_show);
    if (start < 0) start = 0;

    for (int row = 0; row < lines_to_show && (start + row) < sb_count; row++) {
        int idx = (sb_head + start + row) % SCROLLBACK_LINES;
        for (int col = 0; col < SCROLLBACK_WIDTH; col++) {
            vga_put_raw(col, row,
                sb_buf[idx].text[col],
                sb_buf[idx].colors[col]);
        }
    }
}

void scrollback_page_up(void) {
    if (!sb_buf) return;
    int max_pages = sb_count / (VGA_HEIGHT - 1);
    if (sb_view < max_pages) {
        sb_view++;
        scrollback_redraw();
    }
}

void scrollback_page_down(void) {
    if (!sb_buf || sb_view == 0) return;
    sb_view--;
    if (sb_view == 0) {
        vga_scrollback_restore();  
    } else {
        scrollback_redraw();
    }
}
