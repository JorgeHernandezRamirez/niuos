#include "disk.h"
#include "ata/ata.h"

struct disk_driver disk_driver_instance;

void _init_ata_driver(void){
    disk_driver_instance.open = ata_open;
    disk_driver_instance.read_sectors = ata_read_sectors;
    disk_driver_instance.write_sectors = ata_write_sectors;
}

void init_disk_driver(void) {
    #ifdef DISK_DRIVER_ATA
    _init_ata_driver();
    #endif
}