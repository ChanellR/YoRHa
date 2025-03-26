#include <ata.h>

// https://wiki.osdev.org/ATA_PIO_Mode

static void ata_wait_ready() {
    while (inb(ATA_REG_STATUS) & ATA_SR_BSY);
}

static void ata_cache_flush() {
    ata_wait_ready();
    outb(ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    ata_wait_ready();
}

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

    for (uint8_t sector = 0; sector < sector_count; sector++) {
        while (!(inb(ATA_REG_STATUS) & ATA_SR_DRQ));
        // NOTE: If bytes are entered in small endian, does it call it back the right way
        rep_insw(ATA_REG_DATA, (void*)(&buffer[sector * SECTOR_BYTES]), SECTOR_WORDS); // 256 words (512 bytes)
    }
}

void ata_write_sectors(uint32_t lba, uint32_t sector_count, const uint8_t* buffer) {

    ata_wait_ready();

    // Select drive (Master)
    outb(ATA_REG_DRIVE_SELECT, 0xE0 | ((lba >> 24) & 0x0F));

    // Waste time
    outb(ATA_REG_ERROR, 0x00);

    outb(ATA_REG_SECCOUNT, (uint8_t)sector_count); // Read one sector
    outb(ATA_REG_LBA0, (uint8_t)(lba & 0xFF));
    outb(ATA_REG_LBA1, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_REG_LBA2, (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_REG_COMMAND, ATA_CMD_WRITE_SECTORS);

    for (uint8_t sector = 0; sector < sector_count; sector++) {
        while (!(inb(ATA_REG_STATUS) & ATA_SR_DRQ));
        for (uint32_t sw = 0; sw < SECTOR_WORDS; sw++) {
            // looks like big endian
            outsw(ATA_REG_DATA, ((uint16_t)buffer[sector * SECTOR_BYTES + sw * 2 + 1]) << 8
            | (uint16_t)(buffer[sector * SECTOR_BYTES + sw * 2]));
        }
    }

    ata_cache_flush();
}

void ata_select_drive(int is_master) {
    outb(ATA_REG_DRIVE_SELECT, is_master ? 0xA0 : 0xB0);
}

void ata_identify() {
    ata_select_drive(1); // Select master drive
    
    outb(ATA_REG_SECCOUNT, 0);
    outb(ATA_REG_LBA0, 0);
    outb(ATA_REG_LBA1, 0);
    outb(ATA_REG_LBA2, 0);
    outb(ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
}

int ata_wait() {
    while (1) {
        uint8_t status = inb(ATA_REG_STATUS);
        if (!(status & 0x80)) return 1; // Check BSY (Busy) bit
    }
}

void ata_read_buffer(uint16_t* buffer) {
    // reads an entire sector, 256 words, 512 bytes
    for (int i = 0; i < 256; i++) {
        buffer[i] = inw(ATA_REG_DATA);
    }
}

uint64_t ata_get_disk_size() {
    // 1024^2 = 1048576 (1 MiB)
    uint16_t ata_buffer[256];
    ata_identify();
    ata_wait();
    ata_read_buffer(ata_buffer);
    uint32_t total_sectors = ((uint32_t)ata_buffer[61] << 16) | ata_buffer[60];
    return (uint64_t)total_sectors * 512; // Convert to bytes
}

void ata_read_blocks(uint32_t block_num, const uint8_t* buffer, uint32_t count) {
    ata_read_sectors(block_num * SECTORS_PER_BLOCK, SECTORS_PER_BLOCK * count, buffer);
}

// these are wasteful, just writes past buffer, regardless of length
void ata_write_blocks(uint32_t block_num, const uint8_t* buffer, uint32_t count) {
    ata_write_sectors(block_num * SECTORS_PER_BLOCK, SECTORS_PER_BLOCK * count, buffer);
}
