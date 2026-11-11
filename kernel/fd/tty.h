#ifndef TTY_H
#define TTY_H

#include <stdint.h>
#include "fd.h"

#define KEYBOARD_BUFFER_MAX_CHAR 128

typedef struct tty_struct{
    int (*read)(fd_struct *fd, char *buf, int bytes);
    int (*write)(fd_struct *fd, char *buf, int bytes);
    int16_t pid; //process which is waiting for input
    char buffer[KEYBOARD_BUFFER_MAX_CHAR + 1];
    uint8_t pos;
} tty_struct;

extern tty_struct tty;

int tty_read(fd_struct *fd, char *buf, int bytes);
int tty_write(fd_struct *fd, char *buf, int bytes);
uint8_t tty_task_is_waiting_stdin();
void tty_put_char(char c);
void tty_awake_process();

#endif