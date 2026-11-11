#ifndef STAT_H
#define STAT_H

#include <stdint.h>

/* st_mode: tipo de fichero (mismos valores que POSIX) */
#define S_IFMT  0xF000
#define S_IFREG 0x8000
#define S_IFDIR 0x4000

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

static inline int stat(char *path, stat_t *st) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)               /* eax de salida -> ret (0, o -1 si no existe) */
        : "a" (106), "b" ((int) path), "c" ((int) st)  /* eax=106 (SYS_STAT), ebx=path, ecx=st */
    );
    return ret;
}

#endif
