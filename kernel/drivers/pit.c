/* pit.c - programa el PIT (8253/8254) para generar IRQ0 periodicamente:
   el "reloj" que dispara los cambios de contexto del scheduler. */

#include "pit.h"
#include "../arch/x86/io.h"

#define PIT_CHANNEL0_DATA 0x40
#define PIT_COMMAND       0x43
#define PIT_INPUT_HZ      1193182u /* frecuencia fija del cristal del PIT */

/* hz: frecuencia deseada de IRQ0 (p.ej. 100 = un tick cada 10ms) */
void pit_init(uint32_t hz) {
    uint32_t divisor = PIT_INPUT_HZ / hz;

    /* canal 0, acceso lobyte/hibyte, modo 3 (onda cuadrada), binario */
    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0_DATA, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0_DATA, (uint8_t)((divisor >> 8) & 0xFF));
}
