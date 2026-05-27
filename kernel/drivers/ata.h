#ifndef ATA_H
#define ATA_H
#include <stdint.h>

#define ATA_SECTOR_SIZE       512
#define ATA_SECTORS_PER_BLOCK (4096 / ATA_SECTOR_SIZE)

/* secondary bus slave, 'why?' our data disk lives here (index=3, slave) */
#define ATA_DATA         0x170
#define ATA_ERROR        0x171
#define ATA_SECTOR_COUNT 0x172
#define ATA_LBA_LO       0x173
#define ATA_LBA_MID      0x174
#define ATA_LBA_HI       0x175
#define ATA_DRIVE_HEAD   0x176
#define ATA_STATUS       0x177
#define ATA_COMMAND      0x177
#define ATA_ALT_STATUS   0x376
#define ATA_SR_BSY  0x80
#define ATA_SR_DRDY 0x40
#define ATA_SR_DRQ  0x08
#define ATA_SR_ERR  0x01

extern uint64_t g_ata_partition_lba;
void ata_init(void);
void ata_set_partition_offset(uint64_t lba_sector_offset);
int  ata_read_block(uint32_t block_lba,  void *buf);
int  ata_write_block(uint32_t block_lba, const void *buf);

#endif
