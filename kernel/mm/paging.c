/* paging.c - paginacion con espacio de direcciones por proceso.
   paging_init()/paging_enable() arman y activan un identity map
   supervisor-only de TODA la RAM asumida (32MB, debe coincidir con
   pmm.c/Makefile) -- lo que usa el propio kernel para verse igual que antes
   de activar CR0.PG (sigue linkeado en 0x10000, por eso no hace falta
   salto lejano: la instruccion siguiente a activar CR0.PG ya esta
   identity-mapeada por el mismo PD recien activado).
   paging_new_user_address_space() arma, por tarea, un page directory
   PRIVADO: comparte esas mismas PDEs del kernel (misma tabla fisica, no
   una copia) mas una ventana de usuario propia en USER_VIRT_BASE, respaldada
   por fisicos distintos por tarea. Eso es lo que da aislamiento real entre
   procesos: el identity map de abajo queda supervisor-only (User=0), asi
   que ring3 solo puede tocar su propia ventana privada. */

#include "paging.h"
#include "pmm.h"
#include "../drivers/screen.h"

#define PTE_COUNT        1024u

#define PDE_SPAN         (4u * 1024 * 1024)          /* cuanta memoria cubre una PDE sin PAE */
#define KERNEL_PDE_COUNT (TOTAL_RAM_BYTES / PDE_SPAN) /* 8: PDEs 0..7, el identity map de paging_init() */

/* +1 PDE mas alla del identity map: la del heap del kernel (KERNEL_HEAP_VIRT_BASE,
   ver pmm.h), que paging_map_page() va poblando bajo demanda (vmalloc) DESPUES
   de que exista kernel_pd. Si paging_new_user_address_space() solo copiara
   0..KERNEL_PDE_COUNT-1, esta PDE quedaria "no presente" en cada page directory
   de tarea -- cualquier codigo de kernel que corra con ese CR3 cargado (ver
   schedule() en scheduler.c) y toque memoria del heap (p.ej. los globales de
   fat16.c) haria #PF. Se comparte por puntero, igual que el resto: crecimiento
   futuro del heap (mientras siga en esta misma PDE) se ve automaticamente en
   todas las tareas ya creadas. */
#define KERNEL_PDE_COUNT_WITH_HEAP ((KERNEL_HEAP_VIRT_BASE >> 22) + 1u)

static uint32_t kernel_pd_phys;

void paging_init(void) {
    uint32_t pd_phys = pmm_alloc_frame(); /* ya viene en cero: el resto de las 1024 PDEs quedan "no presente" */
    uint32_t *pd = (uint32_t *) pd_phys;  /* sin paginacion todavia: fisico == puntero valido */

    /* supervisor-only (sin PAGE_USER): ring3 ya no necesita tocar este mapeo
       directo -- cada tarea tiene su propia ventana de usuario (ver
       paging_new_user_address_space), asi que dejar esto en User=0 es lo
       que realmente aisla a un proceso de la memoria del otro/del kernel. */
    for (uint32_t i = 0; i < KERNEL_PDE_COUNT; i++) {
        uint32_t pt_phys = pmm_alloc_frame();
        uint32_t *pt = (uint32_t *) pt_phys;

        for (uint32_t j = 0; j < PTE_COUNT; j++) {
            uint32_t phys = i * PDE_SPAN + j * PAGE_SIZE;
            pt[j] = phys | PAGE_PRESENT | PAGE_RW;
        }

        pd[i] = pt_phys | PAGE_PRESENT | PAGE_RW;
    }

    kernel_pd_phys = pd_phys;
}

uint32_t paging_new_user_address_space(uint32_t code_phys, uint32_t code_size, uint32_t stack_pages) {
    uint32_t pd_phys = pmm_alloc_frame(); //pd -> page_data, pt -> page_table
    uint32_t *pd = (uint32_t *) pd_phys;
    uint32_t *kernel_pd = (uint32_t *) kernel_pd_phys;

    /* mismo puntero fisico de page table que kernel_pd, no una copia de
       contenido: un mapeo nuevo del kernel se veria automaticamente en
       todas las tareas ya creadas, sin tener que iterarlas. Incluye la PDE
       del heap (ver KERNEL_PDE_COUNT_WITH_HEAP) por la misma razon. */
    for (uint32_t i = 0; i < KERNEL_PDE_COUNT_WITH_HEAP; i++) {
        pd[i] = kernel_pd[i];
    }

    uint32_t user_pt_phys = pmm_alloc_frame();
    uint32_t *user_pt = (uint32_t *) user_pt_phys;

    uint32_t code_pages = (code_size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint32_t i = 0; i < code_pages && i < PTE_COUNT; i++) {
        uint32_t frame = code_phys + i * PAGE_SIZE; /* reusa el fisico donde boot.asm ya cargo el binario */
        user_pt[i] = frame | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    }

    /* stack: frames NUEVOS del PMM (no el fisico de convencion vieja),
       anclados al techo de la ventana y creciendo hacia abajo. Deja un
       hueco sin mapear entre el codigo y el stack como guarda: un overflow
       cae en una pagina "no presente" (#PF) en vez de corromper memoria. */
    for (uint32_t i = 0; i < stack_pages && i < PTE_COUNT; i++) {
        uint32_t frame = pmm_alloc_frame();
        user_pt[PTE_COUNT - 1 - i] = frame | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    }

    //USER_VIRT_BASE / PDE_SPAN -> 256. 4u * 1024 * 1024=2^22 -> USER_VIRT_BASE / PDE_SPAN es lo meimso que USER_VIRT_BASE << 22 que es como quedarte con los 22 primeros bits
    //Me quedo con los primeros 10 bits. Siempre empieza por 0x40000000. 
    pd[USER_VIRT_BASE >> 22] = user_pt_phys | PAGE_PRESENT | PAGE_RW | PAGE_USER;

    return pd_phys;
}

uint32_t paging_get_kernel_pd_phys(void) {
    return kernel_pd_phys;
}

void paging_map_page(uint32_t virt_addr, uint32_t phys_addr) {
    uint32_t *pd = (uint32_t *) kernel_pd_phys;
    uint32_t pde_index = virt_addr >> 22;
    uint32_t pte_index = (virt_addr >> 12) & 0x3FFu;

    uint32_t *pt;
    if (pd[pde_index] & PAGE_PRESENT) {
        pt = (uint32_t *) (pd[pde_index] & ~0xFFFu);
    } else {
        uint32_t pt_phys = pmm_alloc_frame(); /* ya viene en cero */
        pt = (uint32_t *) pt_phys;
        pd[pde_index] = pt_phys | PAGE_PRESENT | PAGE_RW;
    }

    pt[pte_index] = (phys_addr & ~0xFFFu) | PAGE_PRESENT | PAGE_RW;

    __asm__ volatile ("invlpg (%0)" :: "r" (virt_addr) : "memory");
}

void paging_enable(void) {
    __asm__ volatile (
        "mov %0, %%cr3\n"
        "mov %%cr0, %%eax\n"
        "or  $0x80000000, %%eax\n"
        "mov %%eax, %%cr0\n"
        :: "r" (kernel_pd_phys)
        : "eax", "memory"
    );
}
