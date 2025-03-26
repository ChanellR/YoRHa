#ifndef FLAGS_H
#define FLAGS_H

#define SECTOR_BYTES 512
#define SECTOR_WORDS 256

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
#define ATA_CMD_IDENTIFY       0xEC

// Status Flags
#define ATA_SR_BSY             0x80  // Busy
#define ATA_SR_DRDY            0x40  // Drive ready
#define ATA_SR_DRQ             0x08  // Data request ready

// Drive types
#define ATA_MASTER             0xA0
#define ATA_SLAVE              0xB0

#define SECTORS_PER_BLOCK      0x8  // for filesystem, 4KB blocks 
#define BLOCK_BYTES            SECTORS_PER_BLOCK*SECTOR_BYTES

#endif // FLAGS_H