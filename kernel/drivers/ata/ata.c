#include "ata.h"
#include "../disk.h"

#ifdef DISK_DRIVER_ATA

#define ATA_PRIMARY_IO    0x1F0

#define ATA_REG_DATA      (ATA_PRIMARY_IO + 0)
#define ATA_REG_SECCOUNT0 (ATA_PRIMARY_IO + 2)
#define ATA_REG_LBA0      (ATA_PRIMARY_IO + 3)
#define ATA_REG_LBA1      (ATA_PRIMARY_IO + 4)
#define ATA_REG_LBA2      (ATA_PRIMARY_IO + 5)
#define ATA_REG_HDDEVSEL  (ATA_PRIMARY_IO + 6)
#define ATA_REG_COMMAND   (ATA_PRIMARY_IO + 7)
#define ATA_REG_STATUS    (ATA_PRIMARY_IO + 7)

#define ATA_SR_BSY  0x80
#define ATA_SR_DRQ  0x08
#define ATA_SR_ERR  0x01

#define ATA_CMD_READ_SECTORS  0x20
#define ATA_CMD_WRITE_SECTORS 0x30
#define ATA_CMD_CACHE_FLUSH   0xE7

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void ata_wait_bsy(void) {
    while (inb(ATA_REG_STATUS) & ATA_SR_BSY) { }
}

// Espera a DRQ (datos listos). Devuelve 1 si el disco senaliza ERR.
static int ata_wait_drq(void) {
    uint8_t status;
    do {
        status = inb(ATA_REG_STATUS);
        if (status & ATA_SR_ERR) return 1;
    } while (!(status & ATA_SR_DRQ));
    return 0;
}

int ata_open(void) {
    ata_wait_bsy();
    return 0;
}

// Lee 'count' sectores (siempre 512 bytes en PIO 28-bit LBA) desde 'start_sector'
// hacia 'buffer', copiando palabra a palabra desde el puerto de datos (MMIO por outb/inb).
ssize_t ata_read_sectors(uint32_t start_sector, uint32_t count, uint16_t *buffer) {
    // ATA PIO 28-bit siempre usa sectores de 512 bytes

    ata_wait_bsy();

    outb(ATA_REG_HDDEVSEL, (uint8_t)(0xE0 | ((start_sector >> 24) & 0x0F)));
    outb(ATA_REG_SECCOUNT0, (uint8_t)count);
    outb(ATA_REG_LBA0, (uint8_t)(start_sector));
    outb(ATA_REG_LBA1, (uint8_t)(start_sector >> 8));
    outb(ATA_REG_LBA2, (uint8_t)(start_sector >> 16));
    outb(ATA_REG_COMMAND, ATA_CMD_READ_SECTORS);

    uint16_t *buf16 = (uint16_t *)buffer;

    for (uint32_t sector = 0; sector < count; sector++) {
        if (ata_wait_drq() != 0) {
            return -1;
        }
        for (int i = 0; i < 256; i++) {
            buf16[i] = inw(ATA_REG_DATA);
        }
        buf16 += 256;
    }

    return (ssize_t)(count * 512);
}

// Escribe 'count' sectores desde 'buffer' hacia 'start_sector', palabra a
// palabra por el puerto de datos, igual que ata_read_sectors pero en el
// sentido contrario. CACHE_FLUSH al final para que quede persistido de
// verdad y no solo en la cache de escritura del disco (importante en QEMU:
// sin flush, un reset de la VM puede perder la escritura).
ssize_t ata_write_sectors(uint32_t start_sector, uint32_t count, const uint16_t *buffer) {
    ata_wait_bsy();

    outb(ATA_REG_HDDEVSEL, (uint8_t)(0xE0 | ((start_sector >> 24) & 0x0F)));
    outb(ATA_REG_SECCOUNT0, (uint8_t)count);
    outb(ATA_REG_LBA0, (uint8_t)(start_sector));
    outb(ATA_REG_LBA1, (uint8_t)(start_sector >> 8));
    outb(ATA_REG_LBA2, (uint8_t)(start_sector >> 16));
    outb(ATA_REG_COMMAND, ATA_CMD_WRITE_SECTORS);

    const uint16_t *buf16 = buffer;

    for (uint32_t sector = 0; sector < count; sector++) {
        if (ata_wait_drq() != 0) {
            return -1;
        }
        for (int i = 0; i < 256; i++) {
            outw(ATA_REG_DATA, buf16[i]);
        }
        buf16 += 256;
    }

    outb(ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    ata_wait_bsy();

    return (ssize_t)(count * 512);
}

#endif
