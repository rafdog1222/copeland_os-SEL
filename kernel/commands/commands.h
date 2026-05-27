/* command registery, keeps the cm'ds split while maintiaing calls in shell.c */

#ifndef COMMANDS_H
#define COMMANDS_H

typedef struct {
    const char *name;
    void (*fn)(void);
    const char *desc;
} command_t;

extern command_t command_table[];
extern int       command_count;

void commands_init(void);

#endif
