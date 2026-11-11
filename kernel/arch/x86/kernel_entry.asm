; kernel_entry.asm - primer codigo que se ejecuta del kernel, en modo protegido 32-bit.
; Se ensambla como objeto ELF32 (no binario plano) para poder enlazarlo con kernel.o via ld.

[bits 32]

extern kmain      ; kmain esta definida en kernel.c
extern _bss_start ; limites de .bss, definidos en linker.ld (abarca TODO .bss:
extern _bss_end   ; el de este archivo y el de cada .c, no solo stack_bottom/top de aqui)

global _start
_start:
    ; limpiar .bss a mano: QEMU deja la RAM del guest en cero, pero no hay
    ; que depender de eso -- en cuanto haya page directories/tablas de pagina
    ; ahi, un "present=1" de basura no inicializada es un triple fault
    ; silencioso. No toca esp (usa edi/ecx/eax), asi que es seguro correrlo
    ; con el esp transitorio que dejo boot.asm, antes de fijar stack_top.
    cld
    mov edi, _bss_start
    mov ecx, _bss_end
    sub ecx, edi
    xor eax, eax
    rep stosb

    mov esp, stack_top   ; el compilador de C necesita una pila valida
    call kmain
    cli
.hang:
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 16384        ; 16 KB de pila
global stack_top
stack_top:
