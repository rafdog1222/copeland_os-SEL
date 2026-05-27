#include "shell.h"
#include "kernel.h"
#include "commands/commands.h"
#include "commands/cmd_system.h"
#include <stdint.h>

#define CMD_BUFFER_SIZE 256
#define CMD_COUNT       (sizeof(commands) / sizeof(commands[0]))
#define HISTORY_SIZE    16

/* expo for cmd_echo */
char     cmd_buf[CMD_BUFFER_SIZE];
uint32_t cmd_len = 0;

static char history[HISTORY_SIZE][CMD_BUFFER_SIZE];
static int  history_count = 0;
static int  history_pos   = -1;

/* stringy helpers */
static int k_strcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a - *b;
}

static int k_strncmp(const char *a, const char *b, uint32_t n) {
    while (n-- && *a && *b && *a == *b) { a++; b++; }
    return n == (uint32_t)-1 ? 0 : *a - *b;
}

static uint32_t k_strlen(const char *s) {
    uint32_t n = 0;
    while (*s++) n++;
    return n;
}

/* history */
static void history_push(void) {
    if (cmd_len == 0) return;
    for (int i = HISTORY_SIZE - 1; i > 0; i--)
        for (int j = 0; j < CMD_BUFFER_SIZE; j++)
            history[i][j] = history[i-1][j];
    for (uint32_t i = 0; i < cmd_len; i++)
        history[0][i] = cmd_buf[i];
    history[0][cmd_len] = 0;
    if (history_count < HISTORY_SIZE) history_count++;
    history_pos = -1;
}

static void shell_clear_input(void) {
    while (cmd_len > 0) { cmd_len--; vga_backspace(); }
    vga_update_cursor();
}

static void shell_load_history(int idx) {
    shell_clear_input();
    const char *src = history[idx];
    while (*src && cmd_len < CMD_BUFFER_SIZE - 1) {
        cmd_buf[cmd_len++] = *src;
        vga_putchar(*src, 0x0F00);
        src++;
    }
    vga_update_cursor();
}

void shell_history_up(void) {
    if (history_count == 0) return;
    if (history_pos + 1 < history_count) {
        history_pos++;
        shell_load_history(history_pos);
    }
}

void shell_history_down(void) {
    if (history_pos <= 0) {
        history_pos = -1;
        shell_clear_input();
        return;
    }
    history_pos--;
    shell_load_history(history_pos);
}

/* ask ask ask */
static void shell_prompt(void) {
    vga_print("\n> ", 0x0B00);
}

/* execute = exe's are cute = execute */
static void shell_execute(void) {
    if (cmd_len == 0) return;
    cmd_buf[cmd_len] = '\0';
    history_push();
    /* echo is cool & special so it needs args */
    if (k_strncmp(cmd_buf, "echo", 4) == 0) {
        cmd_echo();
        return;
    }
    for (int i = 0; i < command_count; i++) {
        uint32_t nlen = k_strlen(command_table[i].name);
        if (k_strncmp(cmd_buf, command_table[i].name, nlen) == 0
            && (cmd_buf[nlen] == ' ' || cmd_buf[nlen] == '\0')) {
            command_table[i].fn();
            return;
        }
    }
    /* not found, */
    vga_print("\n  unknown action: ", 0x0C00);
    for (uint32_t i = 0; i < cmd_len; i++)
        vga_putchar(cmd_buf[i], 0x0C00);
    vga_print("\n  type 'help' for commands\n", 0x0800);
}

/* key handler */
void shell_handle_key(char c) {
    if (c == 27) {
        shell_clear_input();
        vga_print("^cancel\n", 0x0800);
        shell_prompt();
        vga_update_cursor();
        return;
    }
    if (c == '\n') {
        shell_execute();
        cmd_len = 0;
        for (int i = 0; i < CMD_BUFFER_SIZE; i++) cmd_buf[i] = 0;
        shell_prompt();
        vga_update_cursor();
        return;
    }
    if (c == '\b') {
        if (cmd_len > 0) {
            cmd_len--;
            cmd_buf[cmd_len] = 0;
            vga_backspace();
            vga_update_cursor();
        }
        return;
    }
    if (cmd_len < CMD_BUFFER_SIZE - 1) {
        cmd_buf[cmd_len++] = c;
        vga_putchar(c, 0x0F00);
        vga_update_cursor();
    }
}

void shell_init(void) {
    commands_init();
    shell_prompt();
    vga_update_cursor();
}
