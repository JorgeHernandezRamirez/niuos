#include "fd.h"
#include "tty.h"
#include "../fs/fat16/fat16.h"
#include "../lib/lib.h"
#include "../drivers/screen.h"
#include "../mm/pmm.h"
#include "pipe.h"

fd_struct stdout;

fd_struct stdin;

void init_default_fds(){
    stdout.read = fd_unssuported_oper;
    stdout.write =  tty.write;
    stdout.close = fd_nop_close;
    stdout.copy = fd_default_copy;
    stdin.read = tty.read;
    stdin.write = fd_unssuported_oper;
    stdin.close = fd_nop_close;
    stdin.copy = fd_default_copy;
}

void fd_stdout_init(fd_struct **fd){
    *fd = &stdout;
}

void fd_stdin_init(fd_struct **fd){
    *fd = &stdin;
}

void fd_stderr_init(fd_struct **fd){
    fd_stdout_init(fd);
}

 int _fd_file_read(fd_struct *fd, char *buf, int bytes){
    return fs_driver_instance.read(((fd_file_struct *)fd)->file, (uint32_t)bytes, (uint32_t *)buf);
 }

 int _fd_file_write(fd_struct *fd, char *buf, int bytes){
    return fs_driver_instance.write(((fd_file_struct *)fd)->file, (uint32_t)bytes, (uint32_t *)buf);
 }

int _fd_file_close(fd_struct *fd){
    fd_file_struct *fd_file = (fd_file_struct *)fd;
    //TODO: free(fd_file->file)
    return 0;
 }

void fd_file_init(fd_struct **fd, uint32_t *file){ //TODO: Esto debería ser File no FileFAT
    fd_file_struct *fd_file = (fd_file_struct *)vmalloc(sizeof(fd_file_struct));
    fd_file->file = file;
    fd_file->read = _fd_file_read;
    fd_file->write = _fd_file_write;
    fd_file->close = _fd_file_close;
    fd_file->copy = fd_default_copy; //un fichero no lleva cuenta de cuantas tareas lo tienen abierto
    *fd = (fd_struct *)fd_file;
}

uint32_t *fd_file(fd_struct *fd){
    if(fd == NULL || (fd->read != _fd_file_read && fd->write != _fd_file_write)){
        return NULL;
    }
    return ((fd_file_struct *)fd)->file;
}

uint8_t fd_is_tty(fd_struct *fd){
    return fd == &stdout || fd == &stdin;
}

void fd_pipe_init(fd_struct **fd_reader, fd_struct **fd_writer){
    fd_pipe_struct *pipe = pipe_new();
     *fd_reader = (fd_struct *)pipe_new_reader(pipe);
     *fd_writer = (fd_struct *)pipe_new_writer(pipe);
}

int fd_read(fd_struct *fd, char *buf, int bytes){
    return fd->read(fd, buf, bytes);
}

int fd_write(fd_struct *fd, char *buf, int bytes){
    return fd->write(fd, buf, bytes);
}

void fd_copy(fd_struct **fd_dest, fd_struct **fd_src){
    if(*fd_src == NULL){ //hueco libre en la tabla de fds
        *fd_dest = NULL;
        return;
    }
    (*fd_src)->copy(fd_dest, fd_src);
}

int fd_unssuported_oper(fd_struct *fd, char *buf, int bytes){
    screen_print("fd unsupported operation");
}

int fd_unssuported_oper_close(fd_struct *fd){
    screen_print("fd unsupported operation");
    return -1;
}

int fd_nop_close(fd_struct *fd){
    return 0;
}

void fd_default_copy(fd_struct **fd_dest, fd_struct **fd_src){ //compartir el fd tal cual, sin contabilidad extra
    *fd_dest = *fd_src;
}