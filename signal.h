#ifndef SIGNAL_H
#define SIGNAL_H

#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t  s32;

#define SIGNAL_MAGIC            0x6E476953U
#define SIGNAL_VERSION_MAJOR    0
#define SIGNAL_VERSION_MINOR    1
#define SIGNAL_VERSION          ((SIGNAL_VERSION_MAJOR << 8) | SIGNAL_VERSION_MINOR)
#define SIGNAL_BLOCK_SIZE       4096        
#define SIGNAL_BLOCK_SIZE_BITS  12          
#define SIGNAL_SUPERBLOCK_LBA   0           
#define SIGNAL_JOURNAL_START    1           
#define SIGNAL_JOURNAL_BLOCKS   64
#define SIGNAL_BITMAP_START     65          
#define SIGNAL_INODE_TABLE_START 66         
#define SIGNAL_INODE_TABLE_BLOCKS   4
#define SIGNAL_INODE_SIZE           128     
#define SIGNAL_INODES_PER_BLOCK     (SIGNAL_BLOCK_SIZE / SIGNAL_INODE_SIZE)
#define SIGNAL_MAX_INODES           (SIGNAL_INODE_TABLE_BLOCKS * SIGNAL_INODES_PER_BLOCK)
#define SIGNAL_DATA_START       (SIGNAL_INODE_TABLE_START + SIGNAL_INODE_TABLE_BLOCKS)
#define SIGNAL_EXTENTS_PER_INODE    4
#define SIGNAL_DIRENT_SIZE      64          
#define SIGNAL_DIRENTS_PER_BLOCK (SIGNAL_BLOCK_SIZE / SIGNAL_DIRENT_SIZE)
#define SIGNAL_TYPE_FREE        0x00        
#define SIGNAL_TYPE_FILE        0x01        
#define SIGNAL_TYPE_DIR         0x02        
#define SIGNAL_INO_INVALID      0           
#define SIGNAL_INO_ROOT         1           
#define SIGNAL_JOURNAL_MAGIC    0x4A4C5347U 
#define SIGNAL_JOURNAL_COMMIT   0xC0FFEE01U 
#define SIGNAL_OK               0
#define SIGNAL_ERR_NOENT       -1           
#define SIGNAL_ERR_EXIST       -2           
#define SIGNAL_ERR_NOSPACE     -3           
#define SIGNAL_ERR_NOTDIR      -4           
#define SIGNAL_ERR_ISDIR       -5           
#define SIGNAL_ERR_NAMETOOLONG -6           
#define SIGNAL_ERR_CORRUPT     -7           
#define SIGNAL_ERR_IO          -8           
#define SIGNAL_ERR_INVAL       -9           
#define SIGNAL_ERR_NOTEMPTY   -10           

typedef struct __attribute__((packed)) {
    u32 start_block;    
    u32 block_count;    
} signal_extent_t;      

typedef struct __attribute__((packed)) {
    u32 magic;          
    u32 inode_num;      
    u8  type;           
    u8  _pad0[3];       
    u32 size;           
    u32 created_at;     
    u32 modified_at;    
    u32 link_count;     
    u32 overflow_block; 
    signal_extent_t extents[SIGNAL_EXTENTS_PER_INODE]; 
    u8  _reserved[64];  
} signal_inode_t;

#define SIGNAL_NAME_MAX     56
#define SIGNAL_DIRENT_SIZE  64

typedef struct __attribute__((packed)) {
    u32 inode_num;
    u8  type;
    u8  name_len;
    u8  _pad[2];
    char name[SIGNAL_NAME_MAX];
} signal_dirent_t;

typedef struct __attribute__((packed)) {
    u32 magic;              
    u16 version;            
    u8  _pad0[2];           
    u32 total_blocks;       
    u32 data_start;         
    u32 free_block_count;   
    u32 total_inodes;       
    u32 free_inode_count;   
    u32 root_inode;         
    u32 journal_start;      
    u32 journal_blocks;     
    u32 journal_head;       
    u32 bitmap_block;       
    u32 inode_table_start;  
    u32 inode_table_blocks; 
    u32 created_at;         
    u32 last_mount_at;      
    u32 mount_count;        
    char volume_label[32];  
    u8  _reserved[3964];    
} signal_superblock_t;

typedef struct __attribute__((packed)) {
    u32 magic;          
    u32 seq;            
    u32 target_block;   
    u32 block_count;    
    u32 checksum;       
    u8  _pad[12];       
} signal_journal_header_t;  

typedef struct __attribute__((packed)) {
    u32 magic;          
    u32 seq;            
    u8  _pad[24];       
} signal_journal_commit_t; 

typedef struct {
    signal_superblock_t sb;
    u8   bitmap[SIGNAL_BLOCK_SIZE];  /* was: u8 *bitmap (pointer) */
    u32  journal_seq;
    int (*read_block)(u32 lba, void *buf);
    int (*write_block)(u32 lba, const void *buf);
    u8   scratch[SIGNAL_BLOCK_SIZE];
    u8   journal_buf[SIGNAL_BLOCK_SIZE];
} signal_fs_t;

typedef int (*signal_read_fn)(u32 lba, void *buf);
typedef int (*signal_write_fn)(u32 lba, const void *buf);

int signal_init(signal_fs_t *fs,
                signal_read_fn  read_fn,
                signal_write_fn write_fn);
int signal_mount(signal_fs_t *fs);
int signal_unmount(signal_fs_t *fs);
int signal_create(signal_fs_t *fs, const char *path, u8 type);
int signal_write(signal_fs_t *fs, const char *path,
                 const void *buf, u32 len);
int signal_read(signal_fs_t *fs, const char *path,
                void *buf, u32 len);
int signal_delete(signal_fs_t *fs, const char *path);
typedef void (*signal_list_cb)(const char *name, u8 type,
                               u32 inode_num, void *userdata);
int signal_list(signal_fs_t *fs, const char *path,
                signal_list_cb cb, void *userdata);
int signal_stat(signal_fs_t *fs, const char *path,
                signal_inode_t *out);
int signal_read_inode(signal_fs_t *fs, u32 ino, signal_inode_t *out);
int signal_write_inode(signal_fs_t *fs, u32 ino,
                       const signal_inode_t *in);
u32 signal_alloc_block(signal_fs_t *fs);          
int signal_free_block(signal_fs_t *fs, u32 blk);
u32 signal_alloc_inode(signal_fs_t *fs);           
int signal_free_inode(signal_fs_t *fs, u32 ino);
int signal_journal_write(signal_fs_t *fs, u32 target_block,
                         const void *block_data);
/* lost in the fs right now... */
int signal_journal_commit(signal_fs_t *fs);
int signal_journal_replay(signal_fs_t *fs);
u32 signal_timestamp(void);

#endif 
