#ifndef ATA_H
#define ATA_H

#include <stdint.h>
#include "../disk.h"

#ifdef DISK_DRIVER_ATA

int ata_open(void);
ssize_t ata_read_sectors(uint32_t start_sector, uint32_t count, uint16_t *buffer);
ssize_t ata_write_sectors(uint32_t start_sector, uint32_t count, const uint16_t *buffer);

#endif

#endif
