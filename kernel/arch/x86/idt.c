/* idt.c - tabla de interrupciones (IDT) + remapeo del PIC 8259 */

#include <stdint.h>
#include "io.h"
#include "idt.h"

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attr;
    uint16_t offset_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr idtp;

extern void irq0(void);   /* definida en isr.asm */
extern void irq1(void);   /* definida en isr.asm */
extern void isr128(void); /* definida en isr.asm */
extern void isr13(void);  /* #GP, definida en isr.asm */
extern void isr14(void);  /* #PF, definida en isr.asm */

static void idt_set_gate(int n, uint32_t handler, uint16_t selector, uint8_t flags) {
    idt[n].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[n].selector    = selector;
    idt[n].zero        = 0;
    idt[n].type_attr   = flags;
    idt[n].offset_high = (uint16_t)((handler >> 16) & 0xFFFF);
}

/* Remapea el PIC: por defecto IRQ0-7 caen en los vectores 0x08-0x0F, que
   chocan con las excepciones de la CPU. Los movemos a 0x20-0x2F. */
static void pic_remap(void) {
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20); /* IRQ0-7  -> vectores 0x20-0x27 */
    outb(0xA1, 0x28); /* IRQ8-15 -> vectores 0x28-0x2F */
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0xFC); /* mascara todo excepto IRQ0 (timer) e IRQ1 (teclado) */
    outb(0xA1, 0xFF); /* mascara todo el PIC esclavo */
}

void idt_init(void) {
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0); /* gate no presente */
    }

    /* excepciones de CPU con error code: DPL0, la CPU las dispara directo,
       ring3 nunca las invoca a mano como si fueran isr128 */
    idt_set_gate(13, (uint32_t)isr13, 0x08, 0x8E); /* #GP */
    idt_set_gate(14, (uint32_t)isr14, 0x08, 0x8E); /* #PF */

    /* Tras el remapeo, IRQ0 (timer/PIT) cae en el vector 0x20 y IRQ1 (teclado) en 0x21 */
    idt_set_gate(0x20, (uint32_t)irq0, 0x08, 0x8E);
    idt_set_gate(0x21, (uint32_t)irq1, 0x08, 0x8E);

    /* int 0x80 = syscall. DPL=3 (0xEE en vez de 0x8E) para que el futuro
       codigo de ring 3 pueda invocarla con "int $0x80" sin un #GP. */
    idt_set_gate(0x80, (uint32_t)isr128, 0x08, 0xEE); 
    //0xEE -> Esta permitido que ring3 invoque a esta interrupción
    /*
    - bit 7 (P) = 1 → gate presente
    - bits 6-5 (DPL) = 11 = 3 (Quien me puede invocar)
    - bit 4 = 0
    - bits 3-0 = 1110 → interrupt gate de 32 bits
    */
   //0x08 -> Decide en que ring corre el handler, en este caso 0x08 es el selector de segmento de codigo del kernel (ring 0)

    /*0x08 = 0000 0000 0000 1000 en binario:

    - RPL (bits 1-0) = 00 → 0 -> Cuando hay interrupción la CPU cambia a ring 0 ya que se lee este campo solo
    - TI (bit 2) = 0 → apunta a la GDT (no LDT)
    - Index (bits 15-3) = 0000000000001 = 1 -> Apunta a la entrada 1 de GDT que es el code. Le dice a que memoria podemos apuntar*/

    /*
    Lo que hace es describir los segmentos de memoria que la CPU permite utilizar.
    En tu caso concreto, como has definido un modelo plano (flat model), la GDT dice prácticamente:

    Hay un segmento de código:

    Base = 0
    Límite = 4 GB

    Hay un segmento de datos:

    Base = 0
    Límite = 4 GB
        */

    idtp.limit = sizeof(idt) - 1;
    idtp.base = (uint32_t)&idt;
    __asm__ volatile ("lidt %0" : : "m"(idtp));

    pic_remap();
    __asm__ volatile ("sti");
}
