#include "signal.h"
#include "kernel/drivers/rtc.h"

extern void *kmalloc(uint32_t size);
extern void  kfree(void *ptr);
extern void vga_print(const char *str, unsigned short color);
extern void vga_print_hex(uint32_t val);
extern void vga_print_dec(uint32_t val);


/* *sigh* this is going to be a while... */

typedef char _assert_inode_size
    [1 - 2*!!( sizeof(signal_inode_t)  != SIGNAL_INODE_SIZE )];
typedef char _assert_dirent_size
    [1 - 2*!!( sizeof(signal_dirent_t) != SIGNAL_DIRENT_SIZE )];
typedef char _assert_journal_hdr
    [1 - 2*!!( sizeof(signal_journal_header_t) != 32 )];
typedef char _assert_journal_cmt
    [1 - 2*!!( sizeof(signal_journal_commit_t) != 32 )];

static void sig_memset(void *dst, u8 val, u32 n)
{
    u8 *d = (u8 *)dst;
    while (n--) *d++ = val;
}

static void sig_memcpy(void *dst, const void *src, u32 n)
{
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    while (n--) *d++ = *s++;
}

static u32 sig_strlen(const char *s)
{
    u32 n = 0;
    while (*s++) n++;
    return n;
}

static int sig_strcmp(const char *a, const char *b)
{
    while (*a && (*a == *b)) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}


static int sig_strncmp(const char *a, const char *b, u32 n)
{
    while (n && *a && (*a == *b)) { a++; b++; n--; }
    if (!n) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}


static u32 sig_strcpy(char *dst, const char *src, u32 dst_size)
{
    u32 i = 0;
    while (i < dst_size - 1 && src[i]) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
    return i;
}


static u32 sig_checksum(const void *data, u32 n)
{
    const u32 *p = (const u32 *)data;
    u32 n32 = n / 4;
    u32 cs = 0;
    while (n32--) cs ^= *p++;
    return cs;
}


u32 signal_timestamp(void) {
    return rtc_timestamp();
}


static int blk_read(signal_fs_t *fs, u32 blk, void *buf)
{
    int r = fs->read_block(blk, buf);
    if (r < 0) return SIGNAL_ERR_IO;
    return SIGNAL_OK;
}

static int blk_write(signal_fs_t *fs, u32 blk, const void *buf)
{
    int r = fs->write_block(blk, buf);
    if (r < 0) return SIGNAL_ERR_IO;
    return SIGNAL_OK;
}


int signal_journal_write(signal_fs_t *fs, u32 target_block,
                         const void *block_data)
{
    signal_journal_header_t hdr;
    u32 head = fs->sb.journal_head;
    u32 jbase = fs->sb.journal_start;
    u32 jlen  = fs->sb.journal_blocks;
    u32 slot0 = jbase + (head % jlen);
    u32 slot1 = jbase + ((head + 1) % jlen);
    sig_memset(&hdr, 0, sizeof(hdr));
    hdr.magic       = SIGNAL_JOURNAL_MAGIC;
    hdr.seq         = fs->journal_seq;
    hdr.target_block= target_block;
    hdr.block_count = 1;
    hdr.checksum    = sig_checksum(block_data, SIGNAL_BLOCK_SIZE);
    sig_memset(fs->journal_buf, 0, SIGNAL_BLOCK_SIZE);
    sig_memcpy(fs->journal_buf, &hdr, sizeof(hdr));
    if (blk_write(fs, slot0, fs->journal_buf) != SIGNAL_OK)
        return SIGNAL_ERR_IO;
    if (blk_write(fs, slot1, block_data) != SIGNAL_OK)
        return SIGNAL_ERR_IO;
    fs->sb.journal_head = (head + 2) % jlen;
    return SIGNAL_OK;
}

int signal_journal_commit(signal_fs_t *fs)
{
    signal_journal_commit_t cmt;
    u32 head  = fs->sb.journal_head;
    u32 jbase = fs->sb.journal_start;
    u32 jlen  = fs->sb.journal_blocks;
    u32 slot  = jbase + (head % jlen);
    sig_memset(&cmt, 0, sizeof(cmt));
    cmt.magic = SIGNAL_JOURNAL_COMMIT;
    cmt.seq   = fs->journal_seq;
    sig_memset(fs->journal_buf, 0, SIGNAL_BLOCK_SIZE);
    sig_memcpy(fs->journal_buf, &cmt, sizeof(cmt));
    if (blk_write(fs, slot, fs->journal_buf) != SIGNAL_OK)
        return SIGNAL_ERR_IO;
    fs->sb.journal_head = (head + 1) % jlen;
    fs->journal_seq++;
    return SIGNAL_OK;
}


int signal_journal_replay(signal_fs_t *fs)
{
    u32 jbase = fs->sb.journal_start;
    u32 jlen  = fs->sb.journal_blocks;
    u32 i;
    for (i = 0; i < jlen - 2; i++) {
        signal_journal_header_t hdr;
        signal_journal_commit_t cmt;
        if (blk_read(fs, jbase + i, fs->scratch) != SIGNAL_OK)
            continue;
        sig_memcpy(&hdr, fs->scratch, sizeof(hdr));
        if (hdr.magic != SIGNAL_JOURNAL_MAGIC)
            continue;
        if (blk_read(fs, jbase + i + 1, fs->scratch) != SIGNAL_OK)
            continue;
        if (sig_checksum(fs->scratch, SIGNAL_BLOCK_SIZE) != hdr.checksum)
            continue;
        sig_memcpy(fs->journal_buf, fs->scratch, SIGNAL_BLOCK_SIZE);
        /* read commit block */
        if (blk_read(fs, jbase + i + 2, fs->scratch) != SIGNAL_OK)
            continue;
        sig_memcpy(&cmt, fs->scratch, sizeof(cmt));
        if (cmt.magic != SIGNAL_JOURNAL_COMMIT)
            continue;
        if (cmt.seq != hdr.seq)
            continue;
        blk_write(fs, hdr.target_block, fs->journal_buf);
        i += 2;
    }
    fs->sb.journal_head = 0;
    return SIGNAL_OK;
}

static int bitmap_is_free(signal_fs_t *fs, u32 blk)
{
    u32 byte = blk / 8;
    u32 bit  = blk % 8;
    if (byte >= SIGNAL_BLOCK_SIZE) return 0;
    return !((fs->bitmap[byte] >> bit) & 1);  /* 0 = free */
}

static void bitmap_set_used(signal_fs_t *fs, u32 blk)
{
    u32 byte = blk / 8;
    u32 bit  = blk % 8;
    if (byte >= SIGNAL_BLOCK_SIZE) return;
    fs->bitmap[byte] |= (1 << bit);   /* set bit = used */
}

static void bitmap_set_free(signal_fs_t *fs, u32 blk)
{
    u32 byte = blk / 8;
    u32 bit  = blk % 8;
    if (byte >= SIGNAL_BLOCK_SIZE) return;
    fs->bitmap[byte] &= ~(1 << bit);  /* clear bit = free */
}


static int bitmap_flush(signal_fs_t *fs)
{
    int r;
    r = signal_journal_write(fs, fs->sb.bitmap_block, fs->bitmap);
    if (r != SIGNAL_OK) return r;
    r = signal_journal_commit(fs);
    if (r != SIGNAL_OK) return r;
    return blk_write(fs, fs->sb.bitmap_block, fs->bitmap);
}

u32 signal_alloc_block(signal_fs_t *fs)
{
    u32 total = fs->sb.total_blocks;
    u32 start = fs->sb.data_start;
    u32 blk;

    for (blk = start; blk < total; blk++) {
        if (bitmap_is_free(fs, blk)) {
            bitmap_set_used(fs, blk);
            fs->sb.free_block_count--;
            if (bitmap_flush(fs) != SIGNAL_OK)
                return 0;
            return blk;
        }
    }
    return 0; 
}

int signal_free_block(signal_fs_t *fs, u32 blk)
{
    if (blk < fs->sb.data_start || blk >= fs->sb.total_blocks)
        return SIGNAL_ERR_INVAL;
    bitmap_set_free(fs, blk);
    fs->sb.free_block_count++;
    return bitmap_flush(fs);
}

static void inode_location(signal_fs_t *fs,
                            u32 ino,
                            u32 *out_block,
                            u32 *out_offset)
{
    u32 slot  = ino; 
    u32 tbase = fs->sb.inode_table_start;
    *out_block  = tbase + (slot / SIGNAL_INODES_PER_BLOCK);
    *out_offset = (slot % SIGNAL_INODES_PER_BLOCK) * SIGNAL_INODE_SIZE;
}

int signal_read_inode(signal_fs_t *fs, u32 ino, signal_inode_t *out)
{
    u32 blk, off;
    int r;
    if (ino == SIGNAL_INO_INVALID || ino >= SIGNAL_MAX_INODES)
        return SIGNAL_ERR_INVAL;

    inode_location(fs, ino, &blk, &off);
    r = blk_read(fs, blk, fs->scratch);
    if (r != SIGNAL_OK) return r;
    sig_memcpy(out, fs->scratch + off, sizeof(signal_inode_t));
    if (out->magic != SIGNAL_MAGIC && out->type != SIGNAL_TYPE_FREE)
        return SIGNAL_ERR_CORRUPT;
    return SIGNAL_OK;
}

int signal_write_inode(signal_fs_t *fs, u32 ino,
                       const signal_inode_t *in)
{
    u32 blk, off;
    int r;
    if (ino == SIGNAL_INO_INVALID || ino >= SIGNAL_MAX_INODES)
        return SIGNAL_ERR_INVAL;
    inode_location(fs, ino, &blk, &off);
    r = blk_read(fs, blk, fs->scratch);
    if (r != SIGNAL_OK) return r;
    sig_memcpy(fs->scratch + off, in, sizeof(signal_inode_t));
    r = signal_journal_write(fs, blk, fs->scratch);
    if (r != SIGNAL_OK) return r;
    r = signal_journal_commit(fs);
    if (r != SIGNAL_OK) return r;
    return blk_write(fs, blk, fs->scratch);
}

u32 signal_alloc_inode(signal_fs_t *fs)
{
    u32 ino;
    signal_inode_t node;
    if (fs->sb.free_inode_count == 0) return 0;
    for (ino = 1; ino < SIGNAL_MAX_INODES; ino++) {
        if (signal_read_inode(fs, ino, &node) != SIGNAL_OK) continue;
        if (node.type == SIGNAL_TYPE_FREE) {
            fs->sb.free_inode_count--;
            return ino;
        }
    }
    return 0;
}

int signal_free_inode(signal_fs_t *fs, u32 ino)
{
    signal_inode_t node;
    int r;
    r = signal_read_inode(fs, ino, &node);
    if (r != SIGNAL_OK) return r;
    sig_memset(&node, 0, sizeof(node));
    node.type = SIGNAL_TYPE_FREE;
    r = signal_write_inode(fs, ino, &node);
    if (r != SIGNAL_OK) return r;
    fs->sb.free_inode_count++;
    return SIGNAL_OK;
}

static u32 get_block_for_offset(signal_fs_t *fs,
                                const signal_inode_t *node,
                                u32 offset)
{
    u32 logical_blk = offset >> SIGNAL_BLOCK_SIZE_BITS;
    u32 blk_cursor  = 0;
    u32 i;
    for (i = 0; i < SIGNAL_EXTENTS_PER_INODE; i++) {
        const signal_extent_t *e = &node->extents[i];
        if (e->start_block == 0 && e->block_count == 0) break;
        if (logical_blk < blk_cursor + e->block_count)
            return e->start_block + (logical_blk - blk_cursor);
        blk_cursor += e->block_count;
    }
    if (node->overflow_block != 0) {
        signal_extent_t ovf[SIGNAL_BLOCK_SIZE / sizeof(signal_extent_t)];
        u32 n_extents = SIGNAL_BLOCK_SIZE / sizeof(signal_extent_t);
        u32 j;
        if (blk_read(fs, node->overflow_block, ovf) != SIGNAL_OK)
            return 0;
        for (j = 0; j < n_extents; j++) {
            if (ovf[j].start_block == 0 && ovf[j].block_count == 0)
                break;
            if (logical_blk < blk_cursor + ovf[j].block_count)
                return ovf[j].start_block + (logical_blk - blk_cursor);
            blk_cursor += ovf[j].block_count;
        }
    }
    return 0; 
}

static int append_extent(signal_fs_t *fs,
                          signal_inode_t *node,
                          u32 new_block)
{
    u32 i;
    for (i = 0; i < SIGNAL_EXTENTS_PER_INODE; i++) {
        signal_extent_t *e = &node->extents[i];
        if (e->start_block == 0 && e->block_count == 0) {
            e->start_block = new_block;
            e->block_count = 1;
            return SIGNAL_OK;
        }
        if (i + 1 < SIGNAL_EXTENTS_PER_INODE) {
            signal_extent_t *ne = &node->extents[i + 1];
            if (ne->start_block == 0 && ne->block_count == 0) {
                if (e->start_block + e->block_count == new_block) {
                    e->block_count++;
                    return SIGNAL_OK;
                }
            }
        }
    }
    if (node->overflow_block == 0) {
        u32 ovf_blk = signal_alloc_block(fs);
        if (!ovf_blk) return SIGNAL_ERR_NOSPACE;
        node->overflow_block = ovf_blk;
        sig_memset(fs->scratch, 0, SIGNAL_BLOCK_SIZE);
        blk_write(fs, ovf_blk, fs->scratch);
    }
    {
        signal_extent_t ovf[SIGNAL_BLOCK_SIZE / sizeof(signal_extent_t)];
        u32 n_extents = SIGNAL_BLOCK_SIZE / sizeof(signal_extent_t);
        u32 j;
        if (blk_read(fs, node->overflow_block, ovf) != SIGNAL_OK)
            return SIGNAL_ERR_IO;
        for (j = 0; j < n_extents; j++) {
            if (ovf[j].start_block == 0 && ovf[j].block_count == 0) {
                if (j > 0 &&
                    ovf[j-1].start_block + ovf[j-1].block_count == new_block) {
                    ovf[j-1].block_count++;
                } else {
                    ovf[j].start_block = new_block;
                    ovf[j].block_count = 1;
                }
                return blk_write(fs, node->overflow_block, ovf);
            }
        }
    }
    return SIGNAL_ERR_NOSPACE;
}

static const char *path_next_component(const char *path,
                                       char *comp,
                                       u32   comp_size)
{
    u32 i = 0;
    while (*path == '/') path++;
    while (*path && *path != '/' && i < comp_size - 1)
        comp[i++] = *path++;
    comp[i] = '\0';
    return path;
}

static int lookup_in_dir(signal_fs_t *fs,
                          u32          dir_ino,
                          const char  *name,
                          u32         *out_ino,
                          u8          *out_type)
{
    signal_inode_t dir;
    u32 name_len = sig_strlen(name);
    u32 i;
    int r;
    r = signal_read_inode(fs, dir_ino, &dir);
    if (r != SIGNAL_OK) return r;
    if (dir.type != SIGNAL_TYPE_DIR) return SIGNAL_ERR_NOTDIR;
    for (i = 0; i < 32; i++) {
    u32 blk_offset = i * SIGNAL_BLOCK_SIZE;
    u32 phys_blk;
    signal_dirent_t *dirents;
    u32 j;

    phys_blk = get_block_for_offset(fs, &dir, blk_offset);
    if (!phys_blk) break;
    if (blk_read(fs, phys_blk, fs->scratch) != SIGNAL_OK) continue;
    dirents = (signal_dirent_t *)fs->scratch;
    for (j = 0; j < SIGNAL_DIRENTS_PER_BLOCK; j++) {
        signal_dirent_t *de = &dirents[j];
        if (de->inode_num == SIGNAL_INO_INVALID) continue;
        if (de->name_len != name_len) continue;
        if (sig_strncmp(de->name, name, name_len) == 0) {
            *out_ino  = de->inode_num;
            *out_type = de->type;
            return SIGNAL_OK;
            }
        }
    }
    return SIGNAL_ERR_NOENT;
}

static int path_resolve(signal_fs_t *fs,
                         const char  *path,
                         u32         *out_ino,
                         int          stop_at_parent,
                         u32         *out_parent_ino,
                         char        *out_last_name,
                         u32          last_name_size)
{
    char comp[SIGNAL_NAME_MAX + 1];
    u32  cur_ino = SIGNAL_INO_ROOT;
    u32  prev_ino = SIGNAL_INO_ROOT;
    const char *p = path;
    if (!path || path[0] != '/') return SIGNAL_ERR_INVAL;
    /* strip leading slashes */
    while (*p == '/') p++;
    /* root itself */
    if (*p == '\0') {
        if (out_ino)        *out_ino        = SIGNAL_INO_ROOT;
        if (out_parent_ino) *out_parent_ino = SIGNAL_INO_ROOT;
        if (out_last_name && last_name_size > 0)
            out_last_name[0] = '\0';
        return SIGNAL_OK;
    }
    while (*p) {
        const char *next;
        u32  child_ino;
        u8   child_type;
        int  r;
        next = path_next_component(p, comp, sizeof(comp));
        if (comp[0] == '\0') { p = next; continue; }
        /* skip trailing slashes after this component */
        const char *lookahead = next;
        while (*lookahead == '/') lookahead++;
        int is_last = (*lookahead == '\0');
        if (stop_at_parent && is_last) {
            /* cur_ino is the parent, comp is the filename */
            if (out_parent_ino) *out_parent_ino = cur_ino;
            if (out_last_name)  sig_strcpy(out_last_name, comp, last_name_size);
            if (out_ino)        *out_ino = cur_ino;
            return SIGNAL_OK;
        }
        prev_ino = cur_ino;
        r = lookup_in_dir(fs, cur_ino, comp, &child_ino, &child_type);
        if (r != SIGNAL_OK) return r;
        cur_ino = child_ino;
        p = next;
    }
    if (out_ino)        *out_ino        = cur_ino;
    if (out_parent_ino) *out_parent_ino = prev_ino;
    return SIGNAL_OK;
}

static int dir_add_entry(signal_fs_t *fs,
                          u32          dir_ino,
                          const char  *name,
                          u32          child_ino,
                          u8           child_type)
{
    signal_inode_t dir;
    u32 name_len = sig_strlen(name);
    u32 i;
    int r;
    if (name_len > SIGNAL_NAME_MAX) return SIGNAL_ERR_NAMETOOLONG;
    r = signal_read_inode(fs, dir_ino, &dir);
    if (r != SIGNAL_OK) return r;
    for (i = 0; ; i++) {
        u32 blk_offset = i * SIGNAL_BLOCK_SIZE;
        u32 phys_blk;
        signal_dirent_t *dirents;
        u32 j;
        if (i >= 32) break; 
        phys_blk = get_block_for_offset(fs, &dir, blk_offset);
        if (!phys_blk) break;
        if (blk_read(fs, phys_blk, fs->scratch) != SIGNAL_OK) continue;
        dirents = (signal_dirent_t *)fs->scratch;
        for (j = 0; j < SIGNAL_DIRENTS_PER_BLOCK; j++) {
            if (dirents[j].inode_num == SIGNAL_INO_INVALID) {
                dirents[j].inode_num = child_ino;
                dirents[j].type      = child_type;
                dirents[j].name_len  = (u8)name_len;
                sig_memset(dirents[j].name, 0, SIGNAL_NAME_MAX);
                sig_memcpy(dirents[j].name, name, name_len);
                r = signal_journal_write(fs, phys_blk, fs->scratch);
                /* my head hurts... */
                if (r != SIGNAL_OK) return r;
                r = signal_journal_commit(fs);
                if (r != SIGNAL_OK) return r;
                r = blk_write(fs, phys_blk, fs->scratch);
                if (r != SIGNAL_OK) return r;
                dir.size++;
                return signal_write_inode(fs, dir_ino, &dir);
            }
        }
    }
    {
        u32 new_blk = signal_alloc_block(fs);
        if (!new_blk) return SIGNAL_ERR_NOSPACE;
        sig_memset(fs->scratch, 0, SIGNAL_BLOCK_SIZE);
        {
            signal_dirent_t *de = (signal_dirent_t *)fs->scratch;
            de->inode_num = child_ino;
            de->type      = child_type;
            de->name_len  = (u8)name_len;
            sig_memcpy(de->name, name, name_len);
        }
        r = append_extent(fs, &dir, new_blk);
        if (r != SIGNAL_OK) return r;
        r = signal_journal_write(fs, new_blk, fs->scratch);
        if (r != SIGNAL_OK) return r;
        r = signal_journal_commit(fs);
        if (r != SIGNAL_OK) return r;
        r = blk_write(fs, new_blk, fs->scratch);
        if (r != SIGNAL_OK) return r;
        dir.size++;
        return signal_write_inode(fs, dir_ino, &dir);
    }
}

static int dir_remove_entry(signal_fs_t *fs,
                             u32          dir_ino,
                             const char  *name)
{
    signal_inode_t dir;
    u32 name_len = sig_strlen(name);
    u32 i;
    int r;
    r = signal_read_inode(fs, dir_ino, &dir);
    if (r != SIGNAL_OK) return r;
    for (i = 0; i < 32; i++) {
        u32 blk_offset = i * SIGNAL_BLOCK_SIZE;
        u32 phys_blk;
        signal_dirent_t *dirents;
        u32 j;
        phys_blk = get_block_for_offset(fs, &dir, blk_offset);
        if (!phys_blk) break;
        if (blk_read(fs, phys_blk, fs->scratch) != SIGNAL_OK) continue;
        dirents = (signal_dirent_t *)fs->scratch;
        for (j = 0; j < SIGNAL_DIRENTS_PER_BLOCK; j++) {
            signal_dirent_t *de = &dirents[j];
            if (de->inode_num == SIGNAL_INO_INVALID) continue;
            if (de->name_len != name_len) continue;
            if (sig_strncmp(de->name, name, name_len) == 0) {
                sig_memset(de, 0, sizeof(signal_dirent_t));
                r = signal_journal_write(fs, phys_blk, fs->scratch);
                if (r != SIGNAL_OK) return r;
                r = signal_journal_commit(fs);
                if (r != SIGNAL_OK) return r;
                r = blk_write(fs, phys_blk, fs->scratch);
                if (r != SIGNAL_OK) return r;

                dir.size--;
                return signal_write_inode(fs, dir_ino, &dir);
            }
        }
    }
    return SIGNAL_ERR_NOENT;
}

static int superblock_write(signal_fs_t *fs)
{
    sig_memset(fs->scratch, 0, SIGNAL_BLOCK_SIZE);
    sig_memcpy(fs->scratch, &fs->sb, sizeof(signal_superblock_t));
    return blk_write(fs, SIGNAL_SUPERBLOCK_LBA, fs->scratch);
}

static int superblock_read(signal_fs_t *fs)
{
    int r = blk_read(fs, SIGNAL_SUPERBLOCK_LBA, fs->scratch);
    if (r != SIGNAL_OK) return r;
    sig_memcpy(&fs->sb, fs->scratch, sizeof(signal_superblock_t));
    /* i should proplerly be naming stuff? right.. opps, to late now, i guss it's a blind file */
    return SIGNAL_OK;
}

int signal_init(signal_fs_t *fs,
                signal_read_fn  read_fn,
                signal_write_fn write_fn)
{
    sig_memset(fs, 0, sizeof(signal_fs_t));
    fs->read_block  = read_fn;
    fs->write_block = write_fn;
    return SIGNAL_OK;
}

int signal_mount(signal_fs_t *fs)
{
    int r;
    r = superblock_read(fs);
    if (r != SIGNAL_OK) return r;
    if (fs->sb.magic != SIGNAL_MAGIC)   return SIGNAL_ERR_CORRUPT;
    if (fs->sb.version != SIGNAL_VERSION) return SIGNAL_ERR_CORRUPT;
    r = blk_read(fs, fs->sb.bitmap_block, fs->bitmap);
    if (r != SIGNAL_OK) { kfree(fs->bitmap); return r; }
    r = signal_journal_replay(fs);
    if (r != SIGNAL_OK) { kfree(fs->bitmap); return r; }
    fs->sb.last_mount_at = signal_timestamp();
    fs->sb.mount_count++;
    superblock_write(fs);
    return SIGNAL_OK;
}

int signal_unmount(signal_fs_t *fs) {
    return superblock_write(fs);
}

int signal_create(signal_fs_t *fs, const char *path, u8 type)
{
    char last_name[SIGNAL_NAME_MAX + 1];
    u32  parent_ino, new_ino;
    signal_inode_t new_node;
    u32  now = signal_timestamp();
    int  r;
    if (!path || path[0] != '/') return SIGNAL_ERR_INVAL;
    if (type != SIGNAL_TYPE_FILE && type != SIGNAL_TYPE_DIR)
        return SIGNAL_ERR_INVAL;
    r = path_resolve(fs, path, &parent_ino, 1,
                     &parent_ino, last_name, sizeof(last_name));
    if (r != SIGNAL_OK) return r;
    if (last_name[0] == '\0') return SIGNAL_ERR_INVAL;
    if (sig_strlen(last_name) > SIGNAL_NAME_MAX)
        return SIGNAL_ERR_NAMETOOLONG;
    {
        u32 tmp_ino; u8 tmp_type;
        if (lookup_in_dir(fs, parent_ino, last_name,
                          &tmp_ino, &tmp_type) == SIGNAL_OK)
            return SIGNAL_ERR_EXIST;
    }
    new_ino = signal_alloc_inode(fs);
    if (!new_ino) return SIGNAL_ERR_NOSPACE;
    sig_memset(&new_node, 0, sizeof(new_node));
    new_node.magic       = SIGNAL_MAGIC;
    new_node.inode_num   = new_ino;
    new_node.type        = type;
    new_node.size        = 0;
    new_node.created_at  = now;
    new_node.modified_at = now;
    new_node.link_count  = 1;
    r = signal_write_inode(fs, new_ino, &new_node);
    if (r != SIGNAL_OK) { signal_free_inode(fs, new_ino); return r; }
    r = dir_add_entry(fs, parent_ino, last_name, new_ino, type);
    if (r != SIGNAL_OK) { signal_free_inode(fs, new_ino); return r; }
    superblock_write(fs);
    return SIGNAL_OK;
}

int signal_write(signal_fs_t *fs, const char *path,
                 const void *buf, u32 len)
{
    signal_inode_t node;
    u32  ino;
    u32  bytes_written = 0;
    u32  now = signal_timestamp();
    const u8 *src = (const u8 *)buf;
    int  r;
    r = path_resolve(fs, path, &ino, 0, 0, 0, 0);
    if (r != SIGNAL_OK) return r;
    r = signal_read_inode(fs, ino, &node);
    if (r != SIGNAL_OK) return r;
    if (node.type == SIGNAL_TYPE_DIR) return SIGNAL_ERR_ISDIR;
    {
        u32 i;
        for (i = 0; i < SIGNAL_EXTENTS_PER_INODE; i++) {
            signal_extent_t *e = &node.extents[i];
            u32 b;
            if (!e->start_block && !e->block_count) break;
            for (b = 0; b < e->block_count; b++)
                signal_free_block(fs, e->start_block + b);
            e->start_block = 0;
            e->block_count = 0;
        }
        if (node.overflow_block) {
            signal_extent_t ovf[SIGNAL_BLOCK_SIZE / sizeof(signal_extent_t)];
            u32 n_ext = SIGNAL_BLOCK_SIZE / sizeof(signal_extent_t);
            if (blk_read(fs, node.overflow_block, ovf) == SIGNAL_OK) {
                u32 j;
                for (j = 0; j < n_ext; j++) {
                    u32 b;
                    if (!ovf[j].start_block && !ovf[j].block_count) break;
                    for (b = 0; b < ovf[j].block_count; b++)
                        signal_free_block(fs, ovf[j].start_block + b);
                }
            }
            signal_free_block(fs, node.overflow_block);
            node.overflow_block = 0;
        }
    }
    node.size = 0;
    while (bytes_written < len) {
        u32 new_blk = signal_alloc_block(fs);
        if (!new_blk) break;
        u32 chunk = len - bytes_written;
        if (chunk > SIGNAL_BLOCK_SIZE) chunk = SIGNAL_BLOCK_SIZE;
        sig_memset(fs->scratch, 0, SIGNAL_BLOCK_SIZE);
        sig_memcpy(fs->scratch, src + bytes_written, chunk);
        r = signal_journal_write(fs, new_blk, fs->scratch);
        if (r != SIGNAL_OK) break;
        r = signal_journal_commit(fs);
        if (r != SIGNAL_OK) break;
        r = blk_write(fs, new_blk, fs->scratch);
        if (r != SIGNAL_OK) break;
        r = append_extent(fs, &node, new_blk);
        if (r != SIGNAL_OK) break;
        bytes_written += chunk;
    }
    node.size        = bytes_written;
    node.modified_at = now;
    signal_write_inode(fs, ino, &node);
    superblock_write(fs);
    return (int)bytes_written;
}

int signal_read(signal_fs_t *fs, const char *path,
                void *buf, u32 len)
{
    signal_inode_t node;
    u32  ino;
    u32  bytes_read = 0;
    u8  *dst = (u8 *)buf;
    int  r;
    r = path_resolve(fs, path, &ino, 0, 0, 0, 0);
    if (r != SIGNAL_OK) return r;
    r = signal_read_inode(fs, ino, &node);
    if (r != SIGNAL_OK) return r;
    if (node.type == SIGNAL_TYPE_DIR) return SIGNAL_ERR_ISDIR;
    if (len > node.size) len = node.size;
    while (bytes_read < len) {
        u32 phys_blk = get_block_for_offset(fs, &node, bytes_read);
        if (!phys_blk) break;
        r = blk_read(fs, phys_blk, fs->scratch);
        if (r != SIGNAL_OK) break;
        u32 chunk = len - bytes_read;
        if (chunk > SIGNAL_BLOCK_SIZE) chunk = SIGNAL_BLOCK_SIZE;
        u32 block_off = bytes_read % SIGNAL_BLOCK_SIZE;
        u32 available = SIGNAL_BLOCK_SIZE - block_off;
        if (chunk > available) chunk = available;
        sig_memcpy(dst + bytes_read, fs->scratch + block_off, chunk);
        bytes_read += chunk;
    }
    return (int)bytes_read;
}

int signal_delete(signal_fs_t *fs, const char *path)
{
    char last_name[SIGNAL_NAME_MAX + 1];
    u32  parent_ino, target_ino;
    signal_inode_t node;
    int  r;
    r = path_resolve(fs, path, &target_ino, 1,
                     &parent_ino, last_name, sizeof(last_name));
    if (r != SIGNAL_OK) return r;
    {
        u8 tmp_type;
        r = lookup_in_dir(fs, parent_ino, last_name,
                          &target_ino, &tmp_type);
        if (r != SIGNAL_OK) return r;
    }
    r = signal_read_inode(fs, target_ino, &node);
    if (r != SIGNAL_OK) return r;
    if (node.type == SIGNAL_TYPE_DIR && node.size > 0)
        return SIGNAL_ERR_NOTEMPTY;
    {
        u32 i;
        for (i = 0; i < SIGNAL_EXTENTS_PER_INODE; i++) {
            signal_extent_t *e = &node.extents[i];
            u32 b;
            if (!e->start_block && !e->block_count) break;
            for (b = 0; b < e->block_count; b++)
                signal_free_block(fs, e->start_block + b);
        }
        if (node.overflow_block) {
            signal_extent_t ovf[SIGNAL_BLOCK_SIZE / sizeof(signal_extent_t)];
            u32 n_ext = SIGNAL_BLOCK_SIZE / sizeof(signal_extent_t);
            if (blk_read(fs, node.overflow_block, ovf) == SIGNAL_OK) {
                u32 j;
                for (j = 0; j < n_ext; j++) {
                    u32 b;
                    if (!ovf[j].start_block && !ovf[j].block_count) break;
                    for (b = 0; b < ovf[j].block_count; b++)
                        signal_free_block(fs, ovf[j].start_block + b);
                }
            }
            signal_free_block(fs, node.overflow_block);
        }
    }
    r = signal_free_inode(fs, target_ino);
    if (r != SIGNAL_OK) return r;
    r = dir_remove_entry(fs, parent_ino, last_name);
    if (r != SIGNAL_OK) return r;
    superblock_write(fs);
    return SIGNAL_OK;
}

int signal_list(signal_fs_t *fs, const char *path,
                signal_list_cb cb, void *userdata)
{
    signal_inode_t dir;
    u32  dir_ino;
    u32  count = 0;
    u32  i;
    int  r;
    r = path_resolve(fs, path, &dir_ino, 0, 0, 0, 0);
    if (r != SIGNAL_OK) return r;
    r = signal_read_inode(fs, dir_ino, &dir);
    if (r != SIGNAL_OK) return r;
    if (dir.type != SIGNAL_TYPE_DIR) return SIGNAL_ERR_NOTDIR;
    for (i = 0; i < 32; i++) {
        u32 blk_offset = i * SIGNAL_BLOCK_SIZE;
        u32 phys_blk;
        signal_dirent_t *dirents;
        u32 j;
        phys_blk = get_block_for_offset(fs, &dir, blk_offset);
        if (!phys_blk) break;
        if (blk_read(fs, phys_blk, fs->scratch) != SIGNAL_OK) break;
        dirents = (signal_dirent_t *)fs->scratch;
        /* i should have named everything, i am already confussed as hell lol */
        for (j = 0; j < SIGNAL_DIRENTS_PER_BLOCK; j++) {
            signal_dirent_t *de = &dirents[j];
            char name_buf[SIGNAL_NAME_MAX + 1];
            if (de->inode_num == SIGNAL_INO_INVALID) continue;
            u32 nlen = de->name_len;
            if (nlen > SIGNAL_NAME_MAX) nlen = SIGNAL_NAME_MAX;
            sig_memcpy(name_buf, de->name, nlen);
            name_buf[nlen] = '\0';
            if (cb) cb(name_buf, de->type, de->inode_num, userdata);
            count++;
        }
    }
    return (int)count;
}

int signal_stat(signal_fs_t *fs, const char *path,
                signal_inode_t *out)
{
    u32 ino;
    int r;
    r = path_resolve(fs, path, &ino, 0, 0, 0, 0);
    if (r != SIGNAL_OK) return r;
    return signal_read_inode(fs, ino, out);
}
