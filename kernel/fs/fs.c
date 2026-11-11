#include "fs.h"
#include "fat16/fat16.h"
#include "../lib/lib.h"
#include "../mm/pmm.h"

fs_driver fs_driver_instance;

void _init_fat16_driver(void){
    fs_driver_instance.fs_init = fat16_init;
    fs_driver_instance.open = (uint32_t* (*)(char *filename, int flags))fat16_open;
    fs_driver_instance.opendir =  (uint32_t* (*)(char *filename))fat16_opendir;
    fs_driver_instance.read = (uint16_t (*)(uint32_t *ff, uint32_t bytes, uint32_t *buf))fat16_read;
    fs_driver_instance.write = (uint16_t (*)(uint32_t *ff, uint32_t bytes, uint32_t *buf))fat16_write;
    fs_driver_instance.seek = (void (*)(uint32_t *ff, long offset, int whence))fat16_seek;
    fs_driver_instance.remove = fat16_remove;
    fs_driver_instance.truncate = fat16_truncate;
    fs_driver_instance.readdir = (dirent* (*)(uint32_t *ff))fat16_readdir; 
    fs_driver_instance.isdir = (uint8_t (*)(uint32_t *ff))fat16_isdir;
    fs_driver_instance.stat = (int (*)(uint32_t *ff, stat_t *st))fat16_stat;
}

void init_fs_driver(void){
    #ifdef FS_FAT16
    _init_fat16_driver();
    #endif
}

char *fs_format_file_path(char *file_path){
    if(is_root(file_path)){
        return file_path;
    }
    char *aux = char_last_pos(file_path);
    if(aux[0] != FAT16_DELIMITER){
        return file_path;
    }
    int size = strlen(file_path);
    aux = (char *)vmalloc(size);
    memcpy(aux, file_path, size - 1);
    aux[size - 1] = '\0';
    return aux;
}


