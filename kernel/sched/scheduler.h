#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include "../fd/fd.h"
#include "task.h"
#include "../fs/fs.h"

#define NO_PID -1

uint8_t scheduler_new_task(char *file_path);

/* registra una tarea nueva de ring3: le arma un espacio de direcciones
   privado (ver paging_new_user_address_space(), kernel/paging.c) mapeando
   [code_phys, code_phys+code_size) y stack_pages paginas nuevas de stack en
   la ventana virtual comun USER_VIRT_BASE/USER_STACK_TOP_VIRT (misma para
   TODAS las tareas -- el aislamiento lo da el CR3 privado, no la direccion
   virtual), y le arma un frame de arranque (el mismo layout trap_frame_t
   que deja pusha+iret, ver scheduler.c) en su propia pila de kernel, listo
   para ser retomado la primera vez que le toque correr -- sea esa primera
   vez un arranque via scheduler_start() o un cambio de contexto normal via
   IRQ0. Todas las tareas se tratan igual, no hay ningun caso especial para
   "la primera". */
//void scheduler_add_task(uint32_t code_phys, uint32_t code_size, uint32_t stack_pages);

/* arranca el scheduler: prepara el TSS, carga el CR3 de la primera tarea y
   pasa a ring3 ahi. No vuelve nunca -- de ahi en mas la CPU se reparte
   entre las tareas via IRQ0 (ver scheduler_next_task() e isr.asm). */
void scheduler_start(void) __attribute__((noreturn));

/* llamada desde el trampolin de irq0 (isr.asm): recibe el esp con el que
   IRQ0 interrumpio a la tarea actual, decide cual sigue (round-robin) y
   devuelve el esp con el que hay que retomarla */
uint32_t scheduler_next_task(uint32_t old_esp);

uint32_t scheduler_malloc(uint32_t size);

uint8_t scheduler_page_not_found(uint32_t cr2);

void scheduler_exit_task(int status);

uint16_t scheduler_fork();

int16_t scheduler_wait(int16_t pid, int *status, int options);

uint8_t scheduler_execve(char *file_path, char *argv[]);

fd_struct *scheduler_current_task_fd(int fd_num);

/* fija la base del segmento de almacenamiento local de hilo de la tarea
   actual y devuelve el indice de GDT donde ha quedado */
uint32_t scheduler_set_thread_area(uint32_t tls_base);

task_t *scheduler_current_task();

uint8_t scheduler_awake(int16_t pid);

int scheduler_open_file(char *file_path, int flags);

uint8_t scheduler_seek_file(int fd_num, long offset, int whence);

int scheduler_getdents64(int fd_num, dirent *buffer, int size);

int scheduler_stat(char *file_path, stat_t *st);

int scheduler_close_fd(int fd);

int scheduler_dup2(int oldfd, int newfd);

int scheduler_new_pipe(int *fds);

int scheduler_mmap2(void *addr, int length, int prot, int flags, int fd, int pgoffset);

#endif
