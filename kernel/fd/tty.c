#include "tty.h"
#include "fd.h"
#include "../sched/scheduler.h"
#include "../sched/task.h"
#include "../drivers/screen.h"
#include "../lib/lib.h"

extern void schedule_kernel(void); /* definida en isr.asm */

int tty_read(fd_struct *fd, char *buf, int bytes){
    task_t *current = scheduler_current_task();
    if(tty.pid != NO_PID){ //A process is already on the tty
        return -1;
    }
    current->status = STATUS_TASK_WAITING_STDIN;
    tty.pid = current->pid;
    schedule_kernel(); /* duerme aca; retoma justo aca mismo cuando tty_awake_process() la despierte */
    bytes = min(tty.pos, bytes);
    tty.buffer[bytes] = '\0';
    memcpy(buf, tty.buffer, bytes + 1);
    tty.pos = 0;
    tty.pid = NO_PID;
    return bytes;
}

int tty_write(fd_struct *fd, char *buf, int bytes){
    screen_print_buf((const uint8_t *)buf, bytes);
    return bytes;
}

tty_struct tty = {
    .read = tty_read,
    .write = tty_write,
    .pid = NO_PID
};

uint8_t tty_task_is_waiting_stdin(){
    return tty.pid != NO_PID;
}

void tty_put_char(char c){
    if(c == '\b'){
        tty.pos--;
        return;
    }
    if(tty.pos < KEYBOARD_BUFFER_MAX_CHAR){
        tty.buffer[tty.pos++] = c;
    }
}

void tty_awake_process(){
    tty.buffer[tty.pos] = '\0';
    scheduler_awake(tty.pid);
}
