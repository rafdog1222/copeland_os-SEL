#include "cmd_wired.h"
#include "../kernel.h"

void cmd_wired(void) {
    vga_print("\n", 0x0F00);
    vga_print("  the Wired is not a separate world.\n", 0x0D00);
    vga_print("  it is an extension of the real.\n",    0x0800);
    vga_print("  you are already connected.\n\n",       0x0B00);
}

void cmd_connect(void) {
    vga_print("\n", 0x0F00);
    vga_print("  establishing connection", 0x0B00);
    vga_print(" . . .\n",                 0x0800);
    vga_print("  protocol : Wired/0.1\n", 0x0700);
    vga_print("  node     : copeland_os-SEL\n", 0x0700);
    vga_print("  status   : ", 0x0700);
    vga_print("CONNECTED\n\n", 0x0A00);
}

void cmd_knights(void) {
    vga_print("\n", 0x0F00);
    vga_print("  KIDS RETURN\n",                        0x0C00);
    vga_print("  Knights of the Eastern Calculus\n",    0x0D00);
    vga_print("  were here.\n\n",                       0x0800);
}
