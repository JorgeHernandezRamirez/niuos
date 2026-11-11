#ifndef DISK_H
#define DISK_H

#include <stdint.h>

#define DISK_DRIVER_ATA

#define BYTES_PER_SECTOR 512

/* sin libc (i686-elf-gcc esta compilado --without-headers): no hay
   <sys/types.h>, definimos el tipo que necesitamos nosotros mismos. */
typedef int32_t ssize_t;

struct disk_driver{
    int (*open)(void);
    ssize_t (*read_sectors)(uint32_t start_sector, uint32_t count, uint16_t *buffer);
    ssize_t (*write_sectors)(uint32_t start_sector, uint32_t count, const uint16_t *buffer);
};

extern struct disk_driver disk_driver_instance;

void init_disk_driver(void);

#endif
