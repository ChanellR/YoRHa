#ifndef ATA_H
#define ATA_H

#include <asm/io.h>

// ATA I/O Ports
#define ATA_PRIMARY_IO_BASE    0x1F0
#define ATA_PRIMARY_CTRL_BASE  0x3F6
#define ATA_REG_DATA           (ATA_PRIMARY_IO_BASE + 0)
#define ATA_REG_ERROR          (ATA_PRIMARY_IO_BASE + 1)
#define ATA_REG_SECCOUNT       (ATA_PRIMARY_IO_BASE + 2)
#define ATA_REG_LBA0           (ATA_PRIMARY_IO_BASE + 3)
#define ATA_REG_LBA1           (ATA_PRIMARY_IO_BASE + 4)
#define ATA_REG_LBA2           (ATA_PRIMARY_IO_BASE + 5)
#define ATA_REG_DRIVE_SELECT   (ATA_PRIMARY_IO_BASE + 6)
#define ATA_REG_COMMAND        (ATA_PRIMARY_IO_BASE + 7)
#define ATA_REG_STATUS         (ATA_PRIMARY_IO_BASE + 7)
#define ATA_REG_ALTSTATUS      (ATA_PRIMARY_CTRL_BASE + 0)
#define ATA_REG_CONTROL        (ATA_PRIMARY_CTRL_BASE + 0)

// ATA Command
#define ATA_CMD_READ_SECTORS   0x20
#define ATA_CMD_WRITE_SECTORS  0x30
#define ATA_CMD_CACHE_FLUSH    0xE7

// Status Flags
#define ATA_SR_BSY             0x80  // Busy
#define ATA_SR_DRDY            0x40  // Drive ready
#define ATA_SR_DRQ             0x08  // Data request ready

// Function to wait until the drive is ready
static void ata_wait_ready() {
    while (inb(ATA_REG_STATUS) & ATA_SR_BSY);
}

static void ata_cache_flush() {
    ata_wait_ready();
    outb(ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    ata_wait_ready();
}

// Function to read a sector
void ata_read_sectors(uint32_t lba, uint32_t sector_count, const uint8_t* buffer) { 
    ata_wait_ready();

    // Select drive (Master)
    outb(ATA_REG_DRIVE_SELECT, 0xE0 | ((lba >> 24) & 0x0F));

    // Waste time
    outb(ATA_REG_ERROR, 0x00);

    outb(ATA_REG_SECCOUNT, sector_count); // Read one sector
    outb(ATA_REG_LBA0, (uint8_t)(lba & 0xFF));
    outb(ATA_REG_LBA1, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_REG_LBA2, (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_REG_COMMAND, ATA_CMD_READ_SECTORS);

    ata_wait_ready();

    // Wait for DRQ (data request) to be set, indicating the drive is ready to transfer data
    while (!(inb(ATA_REG_STATUS) & ATA_SR_DRQ));

    // Read the sector (512 bytes)
    rep_insw(ATA_REG_DATA, (void*)buffer, 256 * sector_count); // 256 words (512 bytes)
}

void ata_write_sectors(uint32_t lba, uint32_t sector_count, const uint8_t* buffer) {

    ata_wait_ready();

    // Select drive (Master)
    outb(ATA_REG_DRIVE_SELECT, 0xE0 | ((lba >> 24) & 0x0F));

    // Waste time
    outb(ATA_REG_ERROR, 0x00);

    outb(ATA_REG_SECCOUNT, sector_count); // Read one sector
    outb(ATA_REG_LBA0, (uint8_t)(lba & 0xFF));
    outb(ATA_REG_LBA1, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_REG_LBA2, (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_REG_COMMAND, ATA_CMD_WRITE_SECTORS);

    while (!(inb(ATA_REG_STATUS) & ATA_SR_DRQ));

    for (uint32_t sw = 0; sw < sector_count * 256; sw++) {
        outsw(ATA_REG_DATA, (uint16_t)(buffer[sw * 2 + 1] << 8) | (uint16_t)(buffer[sw * 2]));
    }

    ata_cache_flush();
}

#endif // ATA_H