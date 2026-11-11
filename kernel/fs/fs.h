#ifndef FS_H
#define FS_H

#include <stdint.h>

#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR   0x0002
#define O_CREAT  0x0040
#define O_TRUNC  0x0200

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define DT_REG 8

#define DT_DIR 4

typedef struct __attribute__((packed)) dirent {
    uint64_t       d_ino;       // número de inodo
    uint64_t       d_off;       // offset al siguiente dirent (uso interno)
    unsigned short d_reclen;    // longitud de este registro
    unsigned char  d_type;      // tipo de fichero (no siempre soportado por el FS)
    char           d_name[256]; // nombre del fichero, terminado en '\0'
} dirent;

/* st_mode: tipo de fichero (mismos valores que POSIX) */
#define S_IFMT  0xF000
#define S_IFREG 0x8000
#define S_IFDIR 0x4000
#define S_IFCHR 0x2000
#define S_IFIFO 0x1000

#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)

typedef struct __attribute__((packed)) stat {
    uint32_t st_dev;     // dispositivo (siempre 0: solo hay un disco)
    uint32_t st_ino;     // numero de inodo (en FAT16: cluster inicial)
    uint16_t st_mode;    // tipo (S_IFREG/S_IFDIR) | permisos
    uint16_t st_nlink;   // enlaces duros (FAT16 no los tiene: siempre 1)
    uint32_t st_size;    // tamaño en bytes
    uint32_t st_blksize; // tamaño de bloque del FS (en FAT16: el cluster)
    uint32_t st_blocks;  // bloques ocupados (clusters)
} stat_t;

#define FS_FAT16

#include "fat16/fat16.h"

typedef struct{
    void (*fs_init)(uint32_t lba_start);
    uint32_t* (*open)(char *file_path, int flags);
    uint32_t* (*opendir)(char *file_path);
    uint16_t (*read)(uint32_t *ff, uint32_t bytes, uint32_t *buf);
    uint16_t (*write)(uint32_t *ff, uint32_t bytes, uint32_t *buf);
    void (*seek)(uint32_t *ff, long offset, int whence);
    int (*remove)(char *file_path);
    int (*truncate)(char *file_path, int length);
    dirent * (*readdir)(uint32_t *ff);
    uint8_t (*isdir)(uint32_t *ff);
    int (*stat)(uint32_t *ff, stat_t *st);
} fs_driver;

extern fs_driver fs_driver_instance;

/**
 * Init fs driver
 */
void init_fs_driver(void);
/**
 * Format file path. If ends on / it is removed
 */
char *fs_format_file_path(char *file_path);

#endif
