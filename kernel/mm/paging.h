#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

/* ventana virtual de usuario: identica para TODAS las tareas (cada una la
   respalda con fisicos propios en su propio page directory, ver
   paging_new_user_address_space). PDE 256 = 0x40000000 / 4MB. */
#define USER_VIRT_BASE      0x40000000u

/* bits de permiso de una PDE/PTE x86 sin PAE. Ojo: no hay bit de "no
   ejecutable" sin PAE+EFER.NXE, asi que PF_X del ELF no se puede hacer
   cumplir; lo unico que se distingue de verdad es solo-lectura vs RW. */
#define PAGE_PRESENT 0x1u
#define PAGE_RW      0x2u
#define PAGE_USER    0x4u

/* permisos por defecto de una pagina de usuario (stack, heap, .data) */
#define PAGE_FLAGS_USER_RW (PAGE_PRESENT | PAGE_RW | PAGE_USER)
/* solo lectura para ring3 (.text, .rodata) */
#define PAGE_FLAGS_USER_RO (PAGE_PRESENT | PAGE_USER)
//#define USER_STACK_TOP_VIRT 0x40400000u /* techo de esa ventana de 4MB */

/* construye, via el PMM, un page directory que mapea TODA la RAM asumida
   (32MB) identity (fisico == virtual), supervisor-only. No activa
   paginacion todavia. */
void paging_init(void);

/* activa CR3 + CR0.PG con el page directory armado por paging_init(). No
   retorna a una direccion distinta: como el kernel sigue linkeado en
   0x10000 (identity map), la ejecucion continua sin discontinuidad. */
void paging_enable(void);

/* arma un espacio de direcciones nuevo para una tarea de ring3:
   - comparte las PDEs del kernel (mismo puntero fisico de page table que
     paging_init() armo -- no una copia de contenido, asi un mapeo nuevo del
     kernel se ve en todas las tareas sin iterarlas).
   - mapea [code_phys, code_phys+code_size) en el fondo de USER_VIRT_BASE
     (present|RW|User).
   - mapea stack_pages frames NUEVOS (via pmm_alloc_frame) al techo de esa
     ventana (USER_STACK_TOP_VIRT hacia abajo), dejando un hueco sin mapear
     entre codigo y stack como guarda contra overflow.
   devuelve el CR3 (fisico) de ese page directory. */
uint32_t paging_new_user_address_space(uint32_t code_phys, uint32_t code_size, uint32_t stack_pages);

/* fisico del page directory del kernel armado por paging_init(), para que
   quien arme un page directory de tarea a mano (ver cr3() en scheduler.c)
   pueda compartir sus PDEs igual que hace paging_new_user_address_space. */
uint32_t paging_get_kernel_pd_phys(void);

/* mapea una pagina de 4KB (virt_addr, alineada) al fisico phys_addr en el
   page directory del kernel activo (present|RW, supervisor). Crea la page
   table intermedia si todavia no existe. Para direcciones fuera del
   identity map de paging_init() (>= 32MB): asi es como el heap del kernel
   (ver pmm.c/vmalloc) crece pagina a pagina sin depender de que los frames
   fisicos del PMM salgan contiguos. */
void paging_map_page(uint32_t virt_addr, uint32_t phys_addr);

#endif
