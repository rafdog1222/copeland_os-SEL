/*
 * same thing here, if your reading from signal_format.c 
 * i just con't be bothered to hash eveything here. 
 * rip,
 *
 *
 *
 *
   signal_format.c fresh signal filesystem to a block device

   Layout written:
     Block 0         : superblock
     Blocks 1-64     : journal (zeroed)
     Block 65        : block bitmap
     Blocks 66-69    : inode table (SIGNAL_INODE_TABLE_BLOCKS = 4)
     Block 70+       : data blocks
*/
#include "signal_format.h"
#include "../../signal.h"

static void fmt_memset(void *dst, uint8_t val, uint32_t n) {
    uint8_t *d = (uint8_t *)dst;
    while (n--) *d++ = val;
}

static void fmt_memcpy(void *dst, const void *src, uint32_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
}

static uint32_t fmt_strlen(const char *s) {
    uint32_t n = 0; while (*s++) n++; return n;
}

int signal_format(signal_fs_t *fs,
                  uint32_t     total_blocks,
                  const char  *volume_label)
{
    uint8_t *block = fs->scratch;   /* no malloc needed, already mine */

    /* 1. zero the journal */
    fmt_memset(block, 0, SIGNAL_BLOCK_SIZE);
    for (uint32_t i = 0; i < SIGNAL_JOURNAL_BLOCKS; i++) {
        if (fs->write_block(SIGNAL_JOURNAL_START + i, block) != 0)
            return SIGNAL_ERR_IO;
    }

    /* 2. block bitmap is all used, then free data blocks */
    fmt_memset(block, 0xFF, SIGNAL_BLOCK_SIZE);
    uint32_t data_start = SIGNAL_DATA_START;
    for (uint32_t b = data_start; b < total_blocks; b++) {
        uint32_t byte = b / 8;
        uint32_t bit  = b % 8;
        if (byte < SIGNAL_BLOCK_SIZE)
            block[byte] &= ~(1u << bit);
    }
    if (fs->write_block(SIGNAL_BITMAP_START, block) != 0)
        return SIGNAL_ERR_IO;

    /* 3. inode table is zeroed, then root inode at slot 1 */
    fmt_memset(block, 0, SIGNAL_BLOCK_SIZE);
    for (uint32_t i = 0; i < SIGNAL_INODE_TABLE_BLOCKS; i++) {
        if (fs->write_block(SIGNAL_INODE_TABLE_START + i, block) != 0)
            return SIGNAL_ERR_IO;
    }
    {
        signal_inode_t root;
        fmt_memset(&root, 0, sizeof(root));
        root.magic       = SIGNAL_MAGIC;
        root.inode_num   = SIGNAL_INO_ROOT;
        root.type        = SIGNAL_TYPE_DIR;
        root.link_count  = 1;

        uint32_t tblk = SIGNAL_INODE_TABLE_START +
                        (SIGNAL_INO_ROOT / SIGNAL_INODES_PER_BLOCK);
        uint32_t toff = (SIGNAL_INO_ROOT % SIGNAL_INODES_PER_BLOCK)
                        * SIGNAL_INODE_SIZE;

        if (fs->read_block(tblk, block) != 0) return SIGNAL_ERR_IO;
        fmt_memcpy(block + toff, &root, sizeof(root));
        if (fs->write_block(tblk, block) != 0) return SIGNAL_ERR_IO;
    }

    /* 4. superblock */
    fmt_memset(block, 0, SIGNAL_BLOCK_SIZE);
    {
        signal_superblock_t *sb = (signal_superblock_t *)block;
        sb->magic              = SIGNAL_MAGIC;
        sb->version            = SIGNAL_VERSION;
        sb->total_blocks       = total_blocks;
        sb->data_start         = data_start;
        sb->free_block_count   = total_blocks - data_start;
        sb->total_inodes       = SIGNAL_MAX_INODES;
        sb->free_inode_count   = SIGNAL_MAX_INODES - 2;
        sb->root_inode         = SIGNAL_INO_ROOT;
        sb->journal_start      = SIGNAL_JOURNAL_START;
        sb->journal_blocks     = SIGNAL_JOURNAL_BLOCKS;
        sb->journal_head       = 0;
        sb->bitmap_block       = SIGNAL_BITMAP_START;
        sb->inode_table_start  = SIGNAL_INODE_TABLE_START;
        sb->inode_table_blocks = SIGNAL_INODE_TABLE_BLOCKS;

        if (volume_label) {
            uint32_t llen = fmt_strlen(volume_label);
            if (llen > 31) llen = 31;
            fmt_memcpy(sb->volume_label, volume_label, llen);
            sb->volume_label[llen] = '\0';
        }
    }
    if (fs->write_block(SIGNAL_SUPERBLOCK_LBA, block) != 0)
        return SIGNAL_ERR_IO;

    return SIGNAL_OK;
}
