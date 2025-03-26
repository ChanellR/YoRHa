#ifndef ATA_H
#define ATA_H

#include <asm/cpu_io.h>
#include <flags.h>

void ata_read_sectors(uint32_t lba, uint32_t sector_count, const uint8_t* buffer);
void ata_write_sectors(uint32_t lba, uint32_t sector_count, const uint8_t* buffer);
void ata_read_buffer(uint16_t* buffer);
uint64_t ata_get_disk_size();
void ata_read_blocks(uint32_t block_num, const uint8_t* buffer, uint32_t count);
void ata_write_blocks(uint32_t block_num, const uint8_t* buffer, uint32_t count);

#endif // ATA_H