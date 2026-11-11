/* linker.ld - coloca el kernel en 0x10000, la misma direccion fisica donde el bootloader lo carga */

ENTRY(_start)

SECTIONS
{
    . = 0x10000;

    .text : {
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        _bss_start = .;
        *(.bss)
        _bss_end = .;
    }

    _kernel_end = .; /* huella real del kernel en RAM (incluye .bss: pila, y pronto PMM/paginacion),
                         no confundir con KERNEL_BUDGET (Makefile/boot.asm), que es cuanto se lee de DISCO */
}
