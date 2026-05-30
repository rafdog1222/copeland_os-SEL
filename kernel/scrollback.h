#ifndef SCROLLBACK_H
#define SCROLLBACK_H 

#include <stdint.h>

#define SCROLLBACK_LINES 200
#define SCROLLBACK_WIDTH 80

void scrollback_init(void);
void scrollback_push(const char *line, const uint8_t *colors);
void scrollback_page_up(void);
void scrollback_page_down(void);
int scrollback_is_active(void);
void scrollback_start(void);

#endif
