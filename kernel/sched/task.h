#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include "../lib/queue.h"
#include "../fd/fd.h"

#define MAX_TASKS 5 

#define MAX_FDS 5

#define TASK_KSTACK_WORDS 1024 // 4KB stack per task

#define STATUS_TASK_FINISHED 0

#define STATUS_TASK_WAITING 1

#define STATUS_TASK_RUNNING 2

#define STATUS_TASK_WAITING_STDIN 3

#define STATUS_TASK_WAITING_PIPE 4

#define STATUS_TASK_FINISHED_ERROR 5

#define WAIT_OPTION_WHANG 0 //Hang father

#define WAIT_OPTION_WNOHANG 1 //Just return child state

#define PROT_NONE  0x0
#define PROT_READ  0x1
#define PROT_WRITE 0x2
#define PROT_EXEC  0x4

#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_ANONYMOUS 0x20

typedef struct {
    uint32_t phys;
    uint32_t virt;
    uint32_t flags; //bits de permiso con los que _build_cr3() arma la PTE (PAGE_FLAGS_USER_RW y companyia, ver mm/paging.h)
} page_t;

typedef struct {
    uint32_t virt_init;
    uint32_t virt_last;
    int prot;
    int flags;
    int fd; //fd asociado al fd que se debe leer. No es un campo requerido
    int pgoffset;
} mmap_t;

typedef queue_t pages;

typedef struct {
    int16_t pid; //if status==STATUS_TASK_WAITING you will receive the child pid to wait for. pid = -1 any child. > 0 a valid child
    int    *status;
} wait_t;

typedef struct son_t {
    int16_t pid;
    int     status;
} son_t;

typedef struct task_t {
    uint16_t       pid; //Max 65536 procceses
    uint8_t        status;
    uint32_t       heap; //heap virtual
    uint32_t       mmap; //heap virtual
    uint32_t       virtual_address_entry; //punto de entrada virtual: e_entry del ELF, o USER_VIRT_BASE en un binario plano
    uint32_t       virtual_address_phdr; //donde quedan mapeadas las cabeceras de programa del ELF, 0 si no lo estan
    uint32_t       phdr_count;  //cuantas son (e_phnum)
    uint32_t       phdr_size;   //cuanto ocupa cada una (e_phentsize)
    uint32_t       tls_base;   //base del segmento de almacenamiento local de hilo (%gs), la que fija set_thread_area
    uint32_t       esp;  //sp virtual
    uint32_t       cr3;  //cr3 physical. Contiene los pd del kernel tb
    uint32_t       kstack[TASK_KSTACK_WORDS]; //physical. Contiene los pd del kernel tb
    queue_t        *heap_pages; //physical. Contiene los pd del kernel tb
    queue_t        *stack_pages; //physical. Contiene los pd del kernel tb
    queue_t        *code_pages; //physical. Contiene los pd del kernel tb
    queue_t        *mmap_pages; //physical. Contiene los pd del kernel tb
    queue_t        *mmaps; //Mmap segments of mmap type
    struct task_t  *parent;
    queue_t        *exited_childs;
    wait_t         *wait;
    fd_struct      *fds[MAX_FDS];
} task_t;

#endif
