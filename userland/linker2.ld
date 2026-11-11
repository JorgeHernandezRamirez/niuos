/* linker2.ld - hello2 (tasks[1]): mismo VMA que linker1.ld (0x40000000, la
   ventana de usuario comun a todas las tareas), LMA en 0x30000 (donde
   boot.asm efectivamente lo carga). Ver linker1.ld para el detalle. */

ENTRY(_start)

SECTIONS
{
    . = 0x40000000;

    .text : AT(0x30000) {
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
