/* gdt.c - GDT propia del kernel (reemplaza a la de boot.asm, que solo servia
   para llegar a modo protegido) + TSS para permitir el cambio de privilegio
   ring3 -> ring0 en interrupciones/syscalls. */

#include <stdint.h>
#include "gdt.h"

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

/* TSS de 32 bits (formato de hardware). Solo usamos esp0/ss0 (stack de ring0
   al que salta la CPU en un cambio de privilegio) e iomap_base (para
   deshabilitar el acceso a puertos de E/S desde ring3). El resto de campos
   solo importan para task-switching por hardware, que aqui no se usa. */
struct tss {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1, ss1;
    uint32_t esp2, ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed));

extern void gdt_flush(void); /* definida en isr.asm: recarga CS/DS/ES/FS/GS/SS */
extern uint32_t stack_top;   /* definida en kernel_entry.asm: tope de la pila del kernel */

static struct gdt_entry gdt[7];
static struct gdt_ptr   gdtp;
static struct tss       kernel_tss;

static void gdt_set_gate(int n, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[n].base_low    = (uint16_t)(base & 0xFFFF);
    gdt[n].base_middle = (uint8_t)((base >> 16) & 0xFF);
    gdt[n].base_high   = (uint8_t)((base >> 24) & 0xFF);

    gdt[n].limit_low   = (uint16_t)(limit & 0xFFFF);
    gdt[n].granularity = (uint8_t)((limit >> 16) & 0x0F);

    gdt[n].granularity |= gran & 0xF0;
    gdt[n].access = access;
}

void gdt_init(void) {
    gdt_set_gate(0, 0, 0, 0, 0); /* nulo */

    /* mismos selectores 0x08/0x10 que el GDT de boot.asm: idt.c no necesita cambios */
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); /* kernel code, DPL=0 */
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); /* kernel data, DPL=0 */

    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); /* user code, DPL=3 */
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); /* user data, DPL=3 */

    kernel_tss.ss0 = GDT_KERNEL_DATA;
    kernel_tss.esp0 = (uint32_t)&stack_top;
    kernel_tss.iomap_base = sizeof(struct tss); /* > limite del TSS: sin bitmap de E/S para ring3 */

    /* TSS: DPL=0 (solo el kernel puede ltr/jmp a este descriptor), granularidad
       en bytes (0x00, NO 0xCF: el limite del TSS no lleva multiplicador de 4K) */
    gdt_set_gate(5, (uint32_t)&kernel_tss, sizeof(struct tss) - 1, 0x89, 0x00);

    /* almacenamiento local de hilo: arranca con base 0, o sea exactamente
       equivalente a GDT_USER_DATA. Asi una tarea que nunca llame a
       set_thread_area() puede llevar %gs=GDT_USER_TLS sin notar diferencia,
       y no hace falta guardar %gs por tarea. */
    gdt_set_gate(GDT_TLS_ENTRY, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    gdtp.limit = sizeof(gdt) - 1;
    gdtp.base = (uint32_t)&gdt;
    __asm__ volatile ("lgdt %0" : : "m"(gdtp));

    gdt_flush();

    __asm__ volatile ("ltr %%ax" : : "a"((uint16_t)0x28));
}

/* usado por el scheduler: en cada cambio de tarea hay que apuntar esp0 a la
   pila de kernel de la tarea que va a correr ahora, porque es la que la CPU
   usara para apilar el frame de interrupcion la proxima vez que se interrumpa
   a esa tarea desde ring3 */
void tss_set_esp0(uint32_t esp0) {
    kernel_tss.esp0 = esp0;
}

/* la CPU relee el descriptor del GDT cada vez que se carga un selector, pero
   NO cuando ya esta cargado: %gs mantiene en cache base/limite/derechos. Por
   eso esto solo vale escrito ANTES del iret que devuelve a ring3, que es
   cuando context_start recarga %gs. */
void gdt_set_tls_base(uint32_t base) {
    gdt_set_gate(GDT_TLS_ENTRY, base, 0xFFFFFFFF, 0xF2, 0xCF);
}
