#ifndef GDT_H
#define GDT_H

#include <stdint.h>

void gdt_init(void);
void tss_set_esp0(uint32_t esp0);

/* reprograma la base del descriptor de almacenamiento local de hilo. El
   selector es el mismo para todas las tareas; lo que cambia de una a otra es
   la base, asi que el scheduler la reescribe en cada cambio de contexto. */
void gdt_set_tls_base(uint32_t base);

#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_CODE   0x1B   /* 0x18 | RPL 3 */
#define GDT_USER_DATA   0x23   /* 0x20 | RPL 3 */
#define GDT_USER_TLS    0x33   /* 0x30 | RPL 3: el segmento al que apunta %gs en ring3 */

/* indice de GDT_USER_TLS dentro del GDT. Es lo que set_thread_area() le
   devuelve al usuario en user_desc.entry_number, y de donde glibc saca el
   selector haciendo (entry_number << 3) | 3. */
#define GDT_TLS_ENTRY   6

#endif
