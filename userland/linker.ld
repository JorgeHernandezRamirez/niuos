/* linker_ls.ld - ls: mismo VMA que linker1.ld/linker2.ld/linker3.ld
   (0x40000000, la ventana de usuario comun a todas las tareas), LMA en
   0x50000 (siguiente hueco libre tras 0x20000/0x30000/0x40000 de
   hello1/hello2/hello3). Ver linker1.ld para el detalle -- esta LMA no la
   usa nadie en tiempo de ejecucion (scheduler_new_task carga los binarios
   desde fs.dmg via FAT16), pero SI determina el orden y los huecos con que
   objcopy -O binary vuelca el .bin.

   Ojo con el AT(): NO encadenar LMAs con LOADADDR(anterior)+SIZEOF(anterior).
   Eso deja las LMAs pegadas mientras el VMA si respeta el ALIGN de cada
   seccion, y en cuanto una seccion pide mas de 1 byte de alineacion (p.ej.
   el .rodata de ls.o es 2**2) el binario plano queda desfasado respecto a
   las direcciones que compilo el codigo: los literales de cadena se leen
   corridos y salen truncados o vacios. Derivamos cada LMA del propio VMA
   para que offset-en-el-.bin == VMA - USER_VIRT_BASE siempre. */

ENTRY(_start)

USER_VIRT_BASE = 0x40000000;
LOAD_BASE      = 0x50000;

SECTIONS
{
    . = USER_VIRT_BASE;

    .text : AT(LOAD_BASE + ADDR(.text) - USER_VIRT_BASE) {
        *(.text)
    }

    .rodata : AT(LOAD_BASE + ADDR(.rodata) - USER_VIRT_BASE) {
        *(.rodata)
    }

    .data : AT(LOAD_BASE + ADDR(.data) - USER_VIRT_BASE) {
        *(.data)
    }

    .bss : AT(LOAD_BASE + ADDR(.bss) - USER_VIRT_BASE) {
        *(.bss)
    }
}
