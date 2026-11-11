#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include "../lib/lib.h"

//Page/frame size x86 without PAE -- single value, used by both pmm.c and paging.c
#define PAGE_SIZE       4096u

#define STACK_VIRTUAL_INIT 0xFFFFFFFF
#define STACK_SIZE 8192 //8192 //8KB
#define STACK_VIRTUAL_LAST (STACK_VIRTUAL_INIT - STACK_SIZE + 1)

#define MMAP_VIRTUAL_INIT (STACK_VIRTUAL_LAST - PAGE_SIZE)

//Redondeo de una direccion virtual/fisica al principio o al final de su pagina
#define ALIGN_DOWN_PAGE(addr) ((addr) & ~(PAGE_SIZE - 1))
#define ALIGN_UP_PAGE(addr)   ALIGN_DOWN_PAGE((addr) + PAGE_SIZE - 1)

//RAM in MB. Should be read dinamically
#define TOTAL_RAM_MB    32u

//RAM size on bytes.
#define TOTAL_RAM_BYTES (TOTAL_RAM_MB * 1024 * 1024)

/* region virtual dedicada al heap del kernel (vmalloc), fuera del identity
   map estatico (32MB) que arma paging_init() -- publico aca (no solo en
   pmm.c) porque paging.c necesita conocer su PDE para compartirla, igual
   que comparte por puntero las PDEs del identity map, con cada page
   directory de tarea nuevo (ver paging_new_user_address_space). */
#define KERNEL_HEAP_VIRT_BASE 0x02000000u

struct page_node {
    uint32_t *physical_address;
    struct page_node *next;
};

struct kernel_heap_info {
    struct page_node *pages;
    struct page_node *current;
    uint32_t *heap; //Current heap
};

void pmm_init(void);

/* devuelve la direccion FISICA de un frame de 4KB libre, ya puesto a cero,
   o 0 si no queda memoria. Valido llamarla incluso antes de activar
   paginacion (la fisica es directamente un puntero usable). */
uint32_t pmm_alloc_frame(void);
void pmm_free_frame(uint32_t phys_addr);
uint32_t pmm_free_frame_count(void);

void kernel_heap_init();
uint32_t *vmalloc(uint32_t size);
struct kernel_heap_info *get_kernel_heap_info();

#endif
