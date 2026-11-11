#ifndef FD_H
#define FD_H

#include <stdint.h>

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

typedef struct fd_struct fd_struct;
typedef struct fd_file_struct fd_file_struct;

struct fd_struct {
    int (*read)(fd_struct *fd, char *buf, int bytes);
    int (*write)(fd_struct *fd, char *buf, int bytes);
    int (*close)(fd_struct *fd);
    void (*copy)(fd_struct **fd_dest, fd_struct **fd_src); //Duplica el fd en fd_dest cuando otra tarea lo hereda (fork)
};

struct fd_file_struct {
    int (*read)(fd_struct *fd, char *buf, int bytes);
    int (*write)(fd_struct *fd, char *buf, int bytes);
    int (*close)(fd_struct *fd);
    void (*copy)(fd_struct **fd_dest, fd_struct **fd_src);
    uint32_t *file; //Var to point out to file
};

void init_default_fds();

void fd_stdout_init(fd_struct **);
void fd_stdin_init(fd_struct **);
void fd_stderr_init(fd_struct **);

int fd_read(fd_struct *fd, char *buf, int bytes);
int fd_write(fd_struct *fd, char *buf, int bytes);
void fd_copy(fd_struct **fd_dest, fd_struct **fd_src);

int fd_unssuported_oper(fd_struct *fd, char *buf, int bytes);
int fd_unssuported_oper_close(fd_struct *fd);
int fd_nop_close(fd_struct *fd);
void fd_default_copy(fd_struct **fd_dest, fd_struct **fd_src);

/* devuelve el fichero que hay detras de un fd, o NULL si ese fd no es un
   fichero. El tipo no se guarda en fd_struct: lo unico que distingue a un
   fichero de un tty o de una tuberia son sus punteros a funcion. */
uint32_t *fd_file(fd_struct *fd);

/* 1 si el fd es la consola (stdin/stdout/stderr sin redirigir) */
uint8_t fd_is_tty(fd_struct *fd);

void fd_file_init(fd_struct **fd, uint32_t *file);
void fd_pipe_init(fd_struct **fd_reader, fd_struct **fd_writer);

#endif

