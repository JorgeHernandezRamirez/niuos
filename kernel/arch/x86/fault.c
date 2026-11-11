/* fault.c - handlers de excepciones de CPU con error code (#GP, #PF). Sin
   recuperacion todavia: son la red de seguridad para depurar paginacion --
   sin esto, cualquier mapeo mal armado es un triple fault silencioso que
   QEMU resuelve reseteando la maquina. */

#include "fault.h"
#include "../../drivers/screen.h"
#include "../../sched/scheduler.h"

static void halt_forever(void) __attribute__((noreturn));
static void halt_forever(void) {
    __asm__ volatile ("cli");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void gp_fault_handler(uint32_t error_code) {
    screen_print("\n*** GENERAL PROTECTION FAULT ***\nerror_code=");
    screen_print_hex32(error_code);
    screen_print("\nSistema detenido.\n");
    halt_forever();
}

/**
Bit	Nombre	0	1
0	P	página no presente	violación de permisos
1	W/R	acceso de lectura	acceso de escritura
2	U/S	modo supervisor (kernel)	modo usuario
3	RSVD	—	bit reservado de PTE en 1 (corrupción)
4	I/D	—	fue un fetch de instrucción (con NX)
 */

uint8_t _page_not_found(uint32_t error_code) {
    return (error_code & 1) == 0;
}

uint8_t _user_mode(uint32_t error_code) {
    return (error_code & 4) > 0;
}


void page_fault_handler(uint32_t error_code, uint32_t cr2) {
    /* el fallo puede venir del kernel y no ser un error: un syscall como write()
       lee el buffer del proceso, y esa pagina puede seguir sin materializarse.
       scheduler_page_not_found() dice si la direccion era de la tarea actual. */
    if (_page_not_found(error_code) && scheduler_page_not_found(cr2)) {
        return;
    }
    screen_print("\n*** PAGE FAULT ***\nCR2=");
    screen_print_hex32(cr2);
    screen_print(" error_code=");
    screen_print_hex32(error_code);
    screen_print(" (P=");
    screen_putchar((error_code & 1) ? '1' : '0');
    screen_print(" W=");
    screen_putchar((error_code & 2) ? '1' : '0');
    screen_print(" U=");
    screen_putchar((error_code & 4) ? '1' : '0');
    screen_print(")\nSistema detenido.\n");
    halt_forever();
}
