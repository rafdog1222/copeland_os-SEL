#include "cmd_signal.h"
#include "../kernel.h"
#include "../drivers/ata.h"
#include "../fs/signal_format.h"
#include "../../signal.h"
#include "../mm/heap.h"

/* shared with shell.c */
extern char     cmd_buf[];
extern uint32_t cmd_len;

/* the one global filesystem instance */
signal_fs_t g_fs;
int         g_fs_mounted  = 0;
int         g_fs_inited   = 0;

/* helpers */

static int k_strcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

static uint32_t k_strlen(const char *s) {
    uint32_t n = 0; while (*s++) n++; return n;
}

static void print2d(uint32_t n, unsigned short color) {
    vga_putchar('0' + (n/10)%10, color);
    vga_putchar('0' + n%10, color);
}

static void print_timestamp(uint32_t ts) {
    if (ts == 0) {
        vga_print("--/--/----  --:--:--", 0x0800);
        return;
    }
    /* reverse the rtc_timestamp() math */
    uint32_t days    = ts / 86400;
    uint32_t tod     = ts % 86400;
    uint32_t hours   = tod / 3600;
    uint32_t minutes = (tod % 3600) / 60;
    uint32_t seconds = tod % 60;
    /* rough date from days since 2000-01-01 */
    uint32_t year  = 2000;
    while (days >= 365) {
        uint32_t leap = ((year % 4 == 0) && (year % 100 != 0)) ? 1 : 0;
        uint32_t ydays = 365 + leap;
        if (days < ydays) break;
        days -= ydays;
        year++;
    }
    static const uint8_t mdays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    uint32_t month = 1;
    while (month <= 12) {
        uint32_t md = mdays[month-1];
        if (month == 2 && (year%4==0) && (year%100!=0)) md = 29;
        if (days < md) break;
        days -= md;
        month++;
    }
    uint32_t day = days + 1;
    print2d(day,     0x0B00); vga_putchar('/', 0x0800);
    print2d(month,   0x0B00); vga_putchar('/', 0x0800);
    vga_print_dec(year);
    vga_print("  ", 0x0F00);
    print2d(hours,   0x0F00); vga_putchar(':', 0x0800);
    print2d(minutes, 0x0F00); vga_putchar(':', 0x0800);
    print2d(seconds, 0x0F00);
}

/* skip past the command word, return pointer to rest of line */
static char *arg_start(void) {
    char *p = cmd_buf;
    while (*p && *p != ' ') p++;   
    while (*p == ' ') p++;       
    return p;
}

/* add this helper */
static void trim_trailing(char *s) {
    int len = k_strlen(s);
    while (len > 0 && s[len-1] == ' ') {
        s[len-1] = '\0';
        len--;
    }
}

static int next_token(const char **src, char *dst, uint32_t dst_size) {
    const char *p = *src;   /* ← const char*, not char* */
    while (*p == ' ') p++;
    if (!*p) return 0;
    uint32_t i = 0;
    while (*p && *p != ' ' && i < dst_size - 1)
        dst[i++] = *p++;
    dst[i] = '\0';
    while (*p == ' ') p++;
    *src = p;
    return 1;
}

static void print_err(int err) {
    switch (err) {
        case SIGNAL_ERR_NOENT:      vga_print(" error: not found\n",          0x0C00); break;
        case SIGNAL_ERR_EXIST:      vga_print(" error: already exists\n",     0x0C00); break;
        case SIGNAL_ERR_NOSPACE:    vga_print(" error: no space left\n",      0x0C00); break;
        case SIGNAL_ERR_NOTDIR:     vga_print(" error: not a directory\n",    0x0C00); break;
        case SIGNAL_ERR_ISDIR:      vga_print(" error: is a directory\n",     0x0C00); break;
        case SIGNAL_ERR_NAMETOOLONG:vga_print(" error: name too long\n",      0x0C00); break;
        case SIGNAL_ERR_CORRUPT:    vga_print(" error: fs corrupt\n",         0x0C00); break;
        case SIGNAL_ERR_IO:         vga_print(" error: I/O failure\n",        0x0C00); break;
        case SIGNAL_ERR_INVAL:      vga_print(" error: invalid argument\n",   0x0C00); break;
        case SIGNAL_ERR_NOTEMPTY:   vga_print(" error: directory not empty\n",0x0C00); break;
        default:                    vga_print(" error: unknown\n",            0x0C00); break;
    }
}

void ensure_inited(void) {
    if (!g_fs_inited) {
        ata_init();
        /* partition offset = 0 until user sets it via 'mount <lba>' */
        ata_set_partition_offset(0);
        signal_init(&g_fs, ata_read_block, ata_write_block);
        g_fs_inited = 1;
    }
}

/* mkfs */
void cmd_fs_mkfs(void) {
    const char *args = arg_start();
    char tok[64];
    uint32_t total_blocks = 0;
    uint64_t lba_offset = 0;
    char label[32];

    if (!next_token(&args, tok, sizeof(tok))) {
        vga_print("\n usage: mkfs <total_blocks> <lba_offset> [label]\n", 0x0E00);
        return;
    }
    for (uint32_t i = 0; tok[i]; i++)
        total_blocks = total_blocks * 10 + (tok[i] - '0');

    /* lba_offset */
    if (!next_token(&args, tok, sizeof(tok))) {
        vga_print("\n usage: mkfs <total_blocks> <lba_offset> [label]\n", 0x0E00);
        return;
    }
    for (uint32_t i = 0; tok[i]; i++)
        lba_offset = lba_offset * 10 + (tok[i] - '0');

    /* optional label */
    if (!next_token(&args, label, sizeof(label)))
        label[0] = '\0';

    ata_set_partition_offset(lba_offset);   /* seting before format */

    vga_print("\n formatting signal filesystem...\n", 0x0E00);
    int r = signal_format(&g_fs, total_blocks, label[0] ? label : "signal");
    if (r != SIGNAL_OK) { vga_print(" mkfs failed:", 0x0C00); print_err(r); return; }
    vga_print(" done. run 'mount <lba>' to mount.\n", 0x0A00);
    g_fs_mounted = 0;
}

/* mount */
void cmd_fs_mount(void) {
    const char *args = arg_start();
    char tok[32];
    if (next_token(&args, tok, sizeof(tok))) {
        uint64_t lba = 0;
        for (uint32_t i = 0; tok[i]; i++) {
            if (tok[i] < '0' || tok[i] > '9') { vga_print("\n bad lba\n", 0x0C00); return; }
            lba = lba * 10 + (tok[i] - '0');
        }
        ata_set_partition_offset(lba);
    }
    if (g_fs_mounted) {
        vga_print("\n already mounted\n", 0x0A00);
        return;
    }
    /* only call signal_init if not already inited by kernel_main */
    if (!g_fs_inited) {
        ata_init();
        ata_set_partition_offset(0);
        signal_init(&g_fs, ata_read_block, ata_write_block);
        g_fs_inited = 1;
    }
    int r = signal_mount(&g_fs);
    if (r != SIGNAL_OK) { vga_print("\n mount failed:", 0x0C00); print_err(r); return; }
    g_fs_mounted = 1;
    vga_print("\n signal mounted.\n", 0x0A00);
}


void cmd_fs_umount(void) {
    if (!g_fs_mounted) { vga_print("\n not mounted\n", 0x0E00); return; }
    int r = signal_unmount(&g_fs);
    g_fs_mounted = 0;
    if (r != SIGNAL_OK) { vga_print("\n umount error:", 0x0C00); print_err(r); return; }
    vga_print("\n signal unmounted.\n", 0x0A00);
}

static void ls_cb(const char *name, uint8_t type, uint32_t ino, void *ud) {
    (void)ud; (void)ino;
    vga_print("  ", 0x0F00);
    if (type == SIGNAL_TYPE_DIR)
        vga_print("[", 0x0B00);
    vga_print(name, type == SIGNAL_TYPE_DIR ? 0x0B00 : 0x0F00);
    if (type == SIGNAL_TYPE_DIR)
        vga_print("]", 0x0B00);
    vga_print("\n", 0x0F00);
}

void cmd_fs_ls(void) {
    if (!g_fs_mounted) { vga_print("\n not mounted\n", 0x0E00); return; }
    char path[64];
    char *raw = arg_start();
    if (!raw[0]) {
        path[0]='/'; path[1]='\0';
    } else {
        uint32_t i=0;
        while(raw[i] && i<63){ path[i]=raw[i]; i++; }
        path[i]='\0';
        trim_trailing(path);
    }
    vga_print("\n", 0x0F00);
    int r = signal_list(&g_fs, path, ls_cb, 0);
    if (r < 0) { vga_print(" ls failed:", 0x0C00); print_err(r); return; }
    if (r == 0) vga_print("  (empty)\n", 0x0800);
}

void cmd_fs_mkdir(void) {
    if (!g_fs_mounted) { vga_print("\n not mounted\n", 0x0E00); return; }
    char *path = arg_start();
    if (!path[0]) { vga_print("\n usage: mkdir <path>\n", 0x0E00); return; }
    trim_trailing(path);
    int r = signal_create(&g_fs, path, SIGNAL_TYPE_DIR);
    if (r != SIGNAL_OK) { vga_print("\n mkdir failed:", 0x0C00); print_err(r); return; }
    vga_print("\n created.\n", 0x0A00);
}

void cmd_fs_touch(void) {
    if (!g_fs_mounted) { vga_print("\n not mounted\n", 0x0E00); return; }
    char *path = arg_start();
    if (!path[0]) { vga_print("\n usage: touch <path>\n", 0x0E00); return; }
    trim_trailing(path);
    int r = signal_create(&g_fs, path, SIGNAL_TYPE_FILE);
    if (r != SIGNAL_OK) { vga_print("\n touch failed:", 0x0C00); print_err(r); return; }
    vga_print("\n created.\n", 0x0A00);
}

void cmd_fs_write(void) {
    if (!g_fs_mounted) { vga_print("\n not mounted\n", 0x0E00); return; }
    const char *args = arg_start();
    char path[64];
    if (!next_token(&args, path, sizeof(path))) {
        vga_print("\n usage: write <path> <data>\n", 0x0E00);
        return;
    }
    if (!args[0]) { vga_print("\n no data given\n", 0x0E00); return; }
    trim_trailing(path);
    int r = signal_write(&g_fs, path, args, k_strlen(args));
    if (r < 0) { vga_print("\n write failed:", 0x0C00); print_err(r); return; }
    vga_print("\n wrote ", 0x0A00);
    vga_print_dec((uint32_t)r);
    vga_print(" bytes.\n", 0x0A00);
}

void cmd_fs_cat(void) {
    if (!g_fs_mounted) { vga_print("\n not mounted\n", 0x0E00); return; }
    char *path = arg_start();
    if (!path[0]) { vga_print("\n usage: cat <path>\n", 0x0E00); return; }

    signal_inode_t st;
    trim_trailing(path);
    int r = signal_stat(&g_fs, path, &st);
    if (r != SIGNAL_OK) { vga_print("\n cat failed:", 0x0C00); print_err(r); return; }
    if (st.type == SIGNAL_TYPE_DIR) { vga_print("\n is a directory\n", 0x0E00); return; }

    uint32_t sz = st.size < 4096 ? st.size : 4096;
    char *buf = (char *)kmalloc(sz + 1);
    if (!buf) { vga_print("\n out of memory\n", 0x0C00); return; }

    r = signal_read(&g_fs, path, buf, sz);
    if (r < 0) { kfree(buf); vga_print("\n read failed:", 0x0C00); print_err(r); return; }
    buf[r] = '\0';
    vga_print("\n", 0x0F00);
    vga_print(buf, 0x0F00);
    vga_print("\n", 0x0F00);
    kfree(buf);
}

void cmd_fs_rm(void) {
    if (!g_fs_mounted) { vga_print("\n not mounted\n", 0x0E00); return; }
    char *path = arg_start();
    if (!path[0]) { vga_print("\n usage: rm <path>\n", 0x0E00); return; }
    trim_trailing(path);
    int r = signal_delete(&g_fs, path);
    if (r != SIGNAL_OK) { vga_print("\n rm failed:", 0x0C00); print_err(r); return; }
    vga_print("\n deleted.\n", 0x0A00);
}

void cmd_fs_stat(void) {
    if (!g_fs_mounted) { vga_print("\n not mounted\n", 0x0E00); return; }
    const char *raw = arg_start();
    char path[64];
    if (!raw[0]) { vga_print("\n usage: stat <path>\n", 0x0E00); return; }
    uint32_t i=0;
    while(raw[i] && i<63){ path[i]=raw[i]; i++; }
    path[i]='\0';
    trim_trailing(path);

    signal_inode_t st;
    int r = signal_stat(&g_fs, path, &st);
    if (r != SIGNAL_OK) { vga_print("\n stat failed:", 0x0C00); print_err(r); return; }

    vga_print("\n", 0x0F00);
    vga_print("  inode    : ", 0x0800); vga_print_dec(st.inode_num);    vga_print("\n", 0x0F00);
    vga_print("  type     : ", 0x0800);
    vga_print(st.type == SIGNAL_TYPE_DIR ? "directory" : "file", 0x0F00);
    vga_print("\n", 0x0F00);
    vga_print("  size     : ", 0x0800); vga_print_dec(st.size);         vga_print(" bytes\n", 0x0F00);
    vga_print("  links    : ", 0x0800); vga_print_dec(st.link_count);   vga_print("\n", 0x0F00);
    vga_print("  created  : ", 0x0800); print_timestamp(st.created_at);  vga_print("\n", 0x0F00);
    vga_print("  modified : ", 0x0800); print_timestamp(st.modified_at); vga_print("\n\n", 0x0F00);
}

void cmd_fs_info(void) {
    if (!g_fs_mounted) { vga_print("\n not mounted\n", 0x0E00); return; }
    signal_superblock_t *sb = &g_fs.sb;
    vga_print("\n signal filesystem info\n", 0x0B00);
    vga_print(" label:        ", 0x0800); vga_print(sb->volume_label, 0x0F00); vga_print("\n", 0x0F00);
    vga_print(" total blocks: ", 0x0800); vga_print_dec(sb->total_blocks);     vga_print("\n", 0x0F00);
    vga_print(" free blocks:  ", 0x0800); vga_print_dec(sb->free_block_count); vga_print("\n", 0x0F00);
    vga_print(" total inodes: ", 0x0800); vga_print_dec(sb->total_inodes);     vga_print("\n", 0x0F00);
    vga_print(" free inodes:  ", 0x0800); vga_print_dec(sb->free_inode_count); vga_print("\n", 0x0F00);
    vga_print(" mount count:  ", 0x0800); vga_print_dec(sb->mount_count);      vga_print("\n", 0x0F00);
    vga_print(" data start:   ", 0x0800); vga_print_dec(sb->data_start);       vga_print("\n", 0x0F00);
}
