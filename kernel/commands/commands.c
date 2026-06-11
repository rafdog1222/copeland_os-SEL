/* assembles the full command table from all command files here  */

#include "commands.h"
#include "cmd_system.h"
#include "cmd_wired.h"
#include "cmd_mem.h"
#include "cmd_signal.h"

command_t command_table[] = {
    /* system */
    { "help",    cmd_help,    "list all commands"           },
    { "cl",      cmd_clear,   "clear the screen"            },
    { "about",   cmd_about,   "about copeland_os-SEL"       },
    { "fetch",   cmd_fetch,   "system info (copeland-fetch)"},
    { "echo",    cmd_echo,    "repeat your words back"      },
    { "tick",    cmd_tick,    "show timer ticks since boot" },
    { "time",    cmd_time,    "show current time"           },    
    /* wired */
    { "wired",   cmd_wired,   "connect to the Wired"        },
    { "connect", cmd_connect, "establish Wired connection"  },
    { "knights", cmd_knights, "..."                         },
    /* memory */
    { "mem",     cmd_mem,     "show memory layout"          },
    /* signal filesystem */
    { "mkfs",    cmd_fs_mkfs,  "format signal partition"    },
    { "mount",   cmd_fs_mount, "mount signal filesystem"    },
    { "umount",  cmd_fs_umount,"unmount signal filesystem"  },
    { "ls",      cmd_fs_ls,    "list directory"             },
    { "mkdir",   cmd_fs_mkdir, "create directory"           },
    { "touch",   cmd_fs_touch, "create empty file"          },
    { "write",   cmd_fs_write, "write data to file"         },
    { "cat",     cmd_fs_cat,   "print file contents"        },
    { "rm",      cmd_fs_rm,    "delete file or empty dir"   },
    { "stat",    cmd_fs_stat,  "show inode info"            },
    { "fsinfo",  cmd_fs_info,  "filesystem stats"           },
};

int command_count = sizeof(command_table) / sizeof(command_table[0]);

void commands_init(void) {
    /* future use for dynamic registration, planning ahead */
}
