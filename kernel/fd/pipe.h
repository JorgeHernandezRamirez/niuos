#ifndef PIPE_H
#define PIPE_H

#include "../lib/queue.h"
#include "fd.h"

#define PIPE_BUFFER_SIZE 1024

typedef struct fd_pipe_struct{
    int (*read)(fd_struct *fd, char *buf, int bytes);
    int (*write)(fd_struct *fd, char *buf, int bytes);
    int (*close)(fd_struct *fd);
    void (*copy)(fd_struct **fd_dest, fd_struct **fd_src);

    char buffer[PIPE_BUFFER_SIZE];
    int pos_reader;
    int pos_writer;
    queue_t *writers; //Sleep task pid writers.
    queue_t *readers; //Sleep task pid readers.
    int num_readers;
    int num_writers;
} fd_pipe_struct;

typedef struct fd_pipe_struct_reader{
    int (*read)(fd_struct *fd, char *buf, int bytes);
    int (*write)(fd_struct *fd, char *buf, int bytes);
    int (*close)(fd_struct *fd);
    void (*copy)(fd_struct **fd_dest, fd_struct **fd_src);
    fd_pipe_struct *pipe;  
} fd_pipe_struct_reader;

typedef struct fd_pipe_struct_writer{
    int (*read)(fd_struct *fd, char *buf, int bytes);
    int (*write)(fd_struct *fd, char *buf, int bytes);
    int (*close)(fd_struct *fd);
    void (*copy)(fd_struct **fd_dest, fd_struct **fd_src);
    fd_pipe_struct *pipe;  
} fd_pipe_struct_writer;

fd_pipe_struct_writer *pipe_new_writer(fd_pipe_struct *pipe);
fd_pipe_struct_reader *pipe_new_reader(fd_pipe_struct *pipe);
fd_pipe_struct *pipe_new();
int pipe_read(fd_struct *fd, char *buf, int bytes);
int pipe_write(fd_struct *fd, char *buf, int bytes);
int pipe_close(fd_struct *fd);
void pipe_copy(fd_struct **fd_dest, fd_struct **fd_src);

#endif