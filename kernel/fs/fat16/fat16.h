#ifndef FAT16_H
#define FAT16_H

#include <stdint.h>
#include "../fs.h"

#define FAT16_DELIMITER '/'
#define FAT16_MAX_FILENAME_SIZE 11
#define FAT16_DEBUG 0
#define LAST_CLUSTER 0xFFFF

typedef struct __attribute__((packed)) {
    uint8_t  jump_boot[3];
    uint8_t  oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t  media_descriptor;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    uint8_t  volume_label[11];
    uint8_t  fs_type[8];
    uint8_t  boot_code[448];
    uint16_t boot_sector_sig;
} fat16_boot_sector_t;

typedef struct {
    //Is root
    uint8_t root;
    //FAT16 cluster number
    uint16_t cluster;
    //File size
    uint32_t file_size;
    //File Type
    uint8_t type;
    //File flags
    uint32_t flags;
    //Current pointer reader
    struct {
        uint16_t cluster;
        uint16_t cluster_offset; //Offset inside the cluster
        uint32_t offset; //Total offset
    } reader;
    //Parent
    struct {
        uint16_t cluster;
        uint16_t cluster_offset;
        uint8_t root; //1 si el padre es el directorio raiz (en vez de inferirlo de cluster==0)
    } dirent_parent;

} FileFAT;


/**
 * Init FAT16 filesystem. It reads the boot sector, the FAT16 table and the root directory
 */
void fat16_init(uint32_t lba_start);

/**
 * Open FileFAT. If its not found it returns a NULL
 */
FileFAT* fat16_open(char *file_path, int flags);

/**
 * Open FileFAT. If its not found it returns a NULL. If is not a directory it returns NULL
 */
FileFAT* fat16_opendir(char *file_path);

/**
 * Return bytes readed. It updates FileFAT reader pointer
 */
uint16_t fat16_read(FileFAT *ff, uint32_t bytes, uint32_t *buf);

/**
 * Write bytes at the current cursor position, growing the cluster chain
 * as needed. If the cursor is beyond the current file size (e.g. after a
 * seek past EOF), the gap is zero-filled first. Returns bytes written.
 */
uint16_t fat16_write(FileFAT *ff, uint32_t bytes, uint32_t *buf);

/**
 * Read next directory
 */
dirent *fat16_readdir(FileFAT *ff);

/**
 * Seek function
 */
int fat16_seek(FileFAT *ff, long offset, int whence);

/**
 * Remove file. If is directory return -1
 */
int fat16_remove(char *file_path);

/**
 * Truncate file
 */
int fat16_truncate(char *file_path, int length);

/**
 * Is directory
 */
uint8_t fat16_isdir(FileFAT *ff);

/**
 * Fill st with the metadata of an already open FileFAT. Returns 0, or -1 on
 * a NULL argument.
 */
int fat16_stat(FileFAT *ff, stat_t *st);


#endif
