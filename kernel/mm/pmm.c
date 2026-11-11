/* pmm.c - physical memory manager: bitmap de 1 bit por pagina fisica de 4KB.
   RAM asumida fija (32MB, ver TOTAL_RAM_BYTES) en vez de detectada via E820:
   es una pieza ortogonal a paginacion/aislamiento, se puede agregar despues
   sin tocar este diseño. TOTAL_RAM_BYTES debe coincidir con "-m 32M" en el
   target "run" del Makefile -- si no coinciden, no hay ningun sintoma hasta
   que el PMM entregue como "libre" un frame que en realidad no existe. */

#include "pmm.h"
#include "../drivers/screen.h"
#include "paging.h"

#define TOTAL_FRAMES    (TOTAL_RAM_BYTES / PAGE_SIZE) /* 8192 */
#define BITMAP_WORDS    (TOTAL_FRAMES / 32)             /* 256 */

/* fin real del kernel en RAM (codigo + .bss: pila, este mismo bitmap, etc),
   definido en linker.ld. Distinto de KERNEL_BUDGET (Makefile/boot.asm), que
   es cuanto se lee de DISCO -- la huella en RAM es mayor por el .bss. */
extern uint32_t _kernel_end;

/* USER_BUDGET/USER2_BUDGET en el Makefile: 2 sectores x 512 = 1024 bytes.
   Si esos presupuestos suben, hay que actualizar esto tambien. */
#define USER1_PHYS_START 0x20000u
#define USER1_PHYS_SIZE  1024u
#define USER2_PHYS_START 0x30000u
#define USER2_PHYS_SIZE  1024u

#define VGA_PHYS_START   0xB8000u
#define VGA_PHYS_SIZE    0x1000u /* 80*25*2 = 4000 bytes, redondeado a una pagina */

/* UMA (Upper Memory Area): de 640KB a 1MB no hay RAM, sino el framebuffer de
   video, las ROMs de expansion y la BIOS. Si el PMM reparte estos frames como
   si fueran memoria, lo que escribas ahi se pierde y lo que leas son unos:
   con suficientes tareas el asignador llegaba hasta aqui y entregaba 0xA8000
   como page directory -- cr3 apuntando a 0xFFFFFFFF y triple fault. */
#define UMA_PHYS_START   0xA0000u
#define UMA_PHYS_SIZE    0x60000u /* hasta 0x100000 */

static uint32_t bitmap[BITMAP_WORDS];
static uint32_t free_frames;

static struct kernel_heap_info kernel_heap_info = { .current = NULL, .heap = NULL, .pages = NULL};

static void mark_used(uint32_t frame_index) {
    uint32_t word = frame_index / 32;
    uint32_t bit = 1u << (frame_index % 32);
    if (bitmap[word] & bit) return; /* ya estaba reservado */
    bitmap[word] |= bit;
    free_frames--;
}

static void reserve_range(uint32_t phys_start, uint32_t size) {
    uint32_t first = phys_start / PAGE_SIZE;
    uint32_t last = (phys_start + size + PAGE_SIZE - 1) / PAGE_SIZE; /* exclusivo */
    for (uint32_t f = first; f < last && f < TOTAL_FRAMES; f++) {
        mark_used(f);
    }
}

void pmm_init(void) {
    for (uint32_t i = 0; i < BITMAP_WORDS; i++) {
        bitmap[i] = 0;
    }
    free_frames = TOTAL_FRAMES;

    reserve_range(0x0, 0x1000);       /* IVT + BIOS Data Area */
    reserve_range(0x7C00, 0x200);     /* boot sector (MBR) */
    reserve_range(0x10000, (uint32_t)&_kernel_end - 0x10000); /* kernel: codigo + .bss real */
    reserve_range(USER1_PHYS_START, USER1_PHYS_SIZE);
    reserve_range(USER2_PHYS_START, USER2_PHYS_SIZE);
    reserve_range(VGA_PHYS_START, VGA_PHYS_SIZE);
    reserve_range(UMA_PHYS_START, UMA_PHYS_SIZE);
}

uint32_t pmm_alloc_frame(void) {
    for (uint32_t w = 0; w < BITMAP_WORDS; w++) {
        if (bitmap[w] == 0xFFFFFFFFu) continue;
        for (uint32_t b = 0; b < 32; b++) {
            uint32_t bitmask = 1u << b;
            if (bitmap[w] & bitmask) continue;

            bitmap[w] |= bitmask;
            free_frames--;

            uint32_t phys = (w * 32 + b) * PAGE_SIZE;
            uint32_t *p = (uint32_t *) phys; /* sin paginacion todavia: fisico == puntero valido */
            for (uint32_t i = 0; i < PAGE_SIZE / 4; i++) {
                p[i] = 0;
            }
            return phys;
        }
    }
    return 0; /* sin memoria libre */
}

void pmm_free_frame(uint32_t phys_addr) {
    uint32_t frame_index = phys_addr / PAGE_SIZE;
    if (frame_index >= TOTAL_FRAMES) return;

    uint32_t word = frame_index / 32;
    uint32_t bit = 1u << (frame_index % 32);
    if (!(bitmap[word] & bit)) return; /* ya estaba libre */
    bitmap[word] &= ~bit;
    free_frames++;
}

uint32_t pmm_free_frame_count(void) {
    return free_frames;
}

/* KERNEL_HEAP_VIRT_BASE esta en pmm.h: esas paginas se mapean bajo demanda
   via paging_map_page() a medida que vmalloc() las necesita. */
static uint32_t next_heap_virt = KERNEL_HEAP_VIRT_BASE;

/* bookkeeping de paginas del heap (para _print_info): pool aparte, NO
   dentro de las paginas que entrega vmalloc -- si viviera ahi, una
   asignacion de mas de una pagina quedaria con estos bytes de control
   incrustados en medio del buffer que ve el caller. */
#define MAX_HEAP_PAGES 1024
static struct page_node heap_page_pool[MAX_HEAP_PAGES];
static uint32_t heap_page_pool_count = 0;

struct page_node *_kernel_heap_new_page() {
    if (heap_page_pool_count >= MAX_HEAP_PAGES) {
        screen_print("\n*** KERNEL HEAP EXHAUSTED (MAX_HEAP_PAGES) ***\nSistema detenido.\n");
        __asm__ volatile ("cli");
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }

    uint32_t phys = pmm_alloc_frame();
    uint32_t virt = next_heap_virt;
    paging_map_page(virt, phys);
    next_heap_virt += PAGE_SIZE;

    struct page_node *previous = kernel_heap_info.current;
    struct page_node *new_page = &heap_page_pool[heap_page_pool_count++];
    new_page->physical_address = (uint32_t *)phys;
    new_page->next = NULL;
    if(previous != NULL){
        previous->next = new_page;
    }
    kernel_heap_info.current = new_page;
    return new_page;
}

void _print_info(){
    struct page_node *iterator = kernel_heap_info.pages;
    int i = 0;
    while(iterator != NULL){
        screen_print("\n-----------------\n");
        screen_print("PAGE ");
        screen_print_int(i++);
        screen_print(":");
        screen_print_hex32((uint32_t)iterator->physical_address);
        iterator = iterator->next;
    }
    screen_print("\n-----------------\n");
    screen_print("HEAP: ");
    screen_print_hex32((uint32_t)kernel_heap_info.heap);
    screen_print("\n");
    screen_print("-----------------\n");
}  

void kernel_heap_init() {
    kernel_heap_info.heap = (uint32_t *)next_heap_virt;
    kernel_heap_info.pages = _kernel_heap_new_page();
}

uint32_t *vmalloc(uint32_t size){
    /* si no alcanza con lo ya mapeado, seguimos pidiendo paginas (cada una
       mapeada a la siguiente direccion virtual consecutiva) hasta cubrir
       "size" completo -- por eso heap no se toca aca, solo next_heap_virt:
       asi el buffer devuelto queda contiguo aunque los frames fisicos que
       entrega el PMM no lo sean. */
    while ((uint8_t *)kernel_heap_info.heap + size > (uint8_t *)next_heap_virt) {
        _kernel_heap_new_page();
    }
    uint32_t *value_to_return = kernel_heap_info.heap;
    kernel_heap_info.heap = (uint32_t *)((uint8_t *)kernel_heap_info.heap + size);
    return value_to_return;
}

struct kernel_heap_info *get_kernel_heap_info(){
    return &kernel_heap_info;
}
