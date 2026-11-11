#ifndef DIRENT_H
#define DIRENT_H

#include <stdint.h>
#include "stdlib.h"

#define DT_DIR 4
#define DT_REG 8

typedef struct __attribute__((packed)) dirent {
    uint64_t       d_ino;       // número de inodo
    uint64_t       d_off;       // offset al siguiente dirent (uso interno)
    unsigned short d_reclen;    // longitud de este registro
    unsigned char  d_type;      // tipo de fichero (no siempre soportado por el FS)
    char           d_name[256]; // nombre del fichero, terminado en '\0'
} dirent;

#define DIRENT_BUFFERED 4

typedef struct DIR{
    int    fd;
    int    bytes;   
    int    offset;  
    dirent buf[DIRENT_BUFFERED];
} DIR;

static inline int getdents64(int fd, dirent *buffer, int size) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)               /* eax de salida -> ret (bytes escritos, 0 al final, -1 error) */
        : "a" (220), "b" (fd), "c" ((int) buffer), "d" (size)  /* eax=220 (SYS_GETDENTS64) */
    );
    return ret;
}

static inline DIR *opendir(char *file_path){
    int fd = open(file_path, O_RDONLY);
    if(fd < 0){
        return NULL;
    }
    DIR *dir = (DIR *)malloc(sizeof(DIR));
    dir->fd = fd;
    dir->bytes = 0;
    dir->offset = 0;
    return dir;
}

static inline dirent *readdir(DIR *dir){
    if(dir->offset >= dir->bytes){
        int bytes = getdents64(dir->fd, dir->buf, sizeof(dir->buf));
        if(bytes <= 0){
            return NULL;
        }
        dir->bytes = bytes;
        dir->offset = 0;
    }
    dirent *entry = (dirent *)((char *)dir->buf + dir->offset);
    dir->offset += entry->d_reclen;
    return entry;
}

static inline int closedir(DIR *dir){
    return close(dir->fd);
}

#endif
