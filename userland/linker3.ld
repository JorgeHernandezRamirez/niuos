/* linker3.ld - hello3: mismo VMA que linker1.ld/linker2.ld (0x40000000, la
   ventana de usuario comun a todas las tareas), LMA en 0x40000 (siguiente
   hueco libre tras 0x20000/0x30000 de hello1/hello2). Ver linker1.ld para
   el detalle -- ojo, esta LMA ya no la usa nadie en tiempo de ejecucion
   (scheduler_new_task carga los binarios desde fs.dmg via FAT16, no desde
   estos offsets fisicos fijos), se mantiene solo por consistencia con los
   otros dos linker scripts. */

ENTRY(_start)

SECTIONS
{
    . = 0x40000000;

    .text : AT(0x40000) {
        *(.text)
    }

    .rodata : AT(LOADADDR(.text) + SIZEOF(.text)) {
        *(.rodata)
    }

    .data : AT(LOADADDR(.rodata) + SIZEOF(.rodata)) {
        *(.data)
    }

    .bss : AT(LOADADDR(.data) + SIZEOF(.data)) {
        *(.bss)
    }
}
