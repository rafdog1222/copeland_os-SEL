#include "ata.h"
#include "../kernel.h"

uint64_t g_ata_partition_lba = 0;

static inline uint8_t inb(uint16_t port) {
    uint8_t r;
    __asm__ volatile ("inb %1, %0" : "=a"(r) : "Nd"(port));
    return r;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t r;
    __asm__ volatile ("inw %1, %0" : "=a"(r) : "Nd"(port));
    return r;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

/* wait until bsy clears, returns 0 on success, -1 on error/timeout */
static int ata_wait_ready(void) {
    uint32_t timeout = 0x100000;
    while (timeout--) {
        uint8_t status = inb(ATA_STATUS);
        if (status & ATA_SR_ERR)  return -1;
        if (!(status & ATA_SR_BSY)) return 0;
    }
    return -1; /* timeout */
}

/* wait for DRQ ,data ready */
static int ata_wait_drq(void) {
    uint32_t timeout = 0x100000;
    while (timeout--) {
        uint8_t status = inb(ATA_STATUS);
        if (status & ATA_SR_ERR) return -1;
        if (status & ATA_SR_DRQ) return 0;
    }
    return -1;
}

void ata_init(void) {
    outb(0x376, 0x04);   /* secondary bus reset */
    outb(0x376, 0x00);
    ata_wait_ready();
    outb(ATA_DRIVE_HEAD, 0x50);  /* select slave */
    ata_wait_ready();
    uint8_t status = inb(ATA_STATUS);
    /* we are moving to clean splash, so i am going to hash this out
    vga_print("ata: status=0x", 0x0A00);
    vga_print_hex(status);
    vga_print("\n", 0x0A00);
    vga_print("ata: secondary bus ready\n", 0x0A00);
    */
}

void ata_set_partition_offset(uint64_t lba_sector_offset) {
    g_ata_partition_lba = lba_sector_offset;
}

/* read one 512-byte sector into buf (must be 512 bytes) */
static int ata_read_sector(uint64_t lba, void *buf) {
    if (ata_wait_ready() != 0) return -1;

    outb(ATA_DRIVE_HEAD, 0x50);   /* LBA48, second slave */
    outb(ATA_SECTOR_COUNT, 0);    /* high byte of count */
    outb(ATA_LBA_LO,  (uint8_t)((lba >> 24) & 0xFF));
    outb(ATA_LBA_MID, 0);
    outb(ATA_LBA_HI,  0);
    outb(ATA_SECTOR_COUNT, 1);    /* low byte of count */
    outb(ATA_LBA_LO,  (uint8_t)(lba & 0xFF));
    outb(ATA_LBA_MID, (uint8_t)((lba >> 8)  & 0xFF));
    outb(ATA_LBA_HI,  (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_COMMAND, 0x24);      

    if (ata_wait_drq() != 0) return -1;

    uint16_t *dst = (uint16_t *)buf;
    for (int i = 0; i < 256; i++)
        dst[i] = inw(ATA_DATA);

    return 0;
}

static int ata_write_sector(uint64_t lba, const void *buf) {
    if (ata_wait_ready() != 0) return -1;

    outb(ATA_DRIVE_HEAD, 0x50);   /* LBA48, second slave */
    outb(ATA_SECTOR_COUNT, 0);
    outb(ATA_LBA_LO,  (uint8_t)((lba >> 24) & 0xFF));
    outb(ATA_LBA_MID, 0);
    outb(ATA_LBA_HI,  0);
    outb(ATA_SECTOR_COUNT, 1);
    outb(ATA_LBA_LO,  (uint8_t)(lba & 0xFF));
    outb(ATA_LBA_MID, (uint8_t)((lba >> 8)  & 0xFF));
    outb(ATA_LBA_HI,  (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_COMMAND, 0x34);      

    if (ata_wait_drq() != 0) return -1;

    const uint16_t *src = (const uint16_t *)buf;
    for (int i = 0; i < 256; i++)
        outw(ATA_DATA, src[i]);

    outb(ATA_COMMAND, 0xE7);
    ata_wait_ready();

    return 0;
}

/* signal block read callback, block_lba is signal's logical block number */
int ata_read_block(uint32_t block_lba, void *buf) {
    uint64_t sector = g_ata_partition_lba + ((uint64_t)block_lba * ATA_SECTORS_PER_BLOCK);
    uint8_t *dst = (uint8_t *)buf;
    for (int i = 0; i < ATA_SECTORS_PER_BLOCK; i++) {
        if (ata_read_sector(sector + i, dst) != 0)
            return -1;
        dst += ATA_SECTOR_SIZE;
    }
    return 0;
}

int ata_write_block(uint32_t block_lba, const void *buf) {
    uint64_t sector = g_ata_partition_lba + ((uint64_t)block_lba * ATA_SECTORS_PER_BLOCK);
    const uint8_t *src = (const uint8_t *)buf;
    for (int i = 0; i < ATA_SECTORS_PER_BLOCK; i++) {
        if (ata_write_sector(sector + i, src) != 0)
            return -1;
        src += ATA_SECTOR_SIZE;
    }
    return 0;
}
