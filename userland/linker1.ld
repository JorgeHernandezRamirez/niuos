/* linker1.ld - hello1 (tasks[0]): direccion de ENLACE (VMA) = 0x40000000,
   la ventana de usuario comun a TODAS las tareas (ver USER_VIRT_BASE en
   kernel/paging.h) -- ya no es la direccion fisica real. La direccion de
   CARGA (LMA, via AT()) sigue siendo 0x20000, donde boot.asm efectivamente
   deja los bytes en disco; el kernel mapea ese fisico a esta virtual con su
   propio page directory (paging_new_user_address_space(), kernel/paging.c).
   Ver linker2.ld para hello2 (mismo VMA, LMA en 0x30000). */

ENTRY(_start)

SECTIONS
{
    . = 0x40000000;

    .text : AT(0x20000) {
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
