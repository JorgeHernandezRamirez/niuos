; crt0.asm - primer codigo del binario de usuario. Igual que kernel_entry.asm:
; garantiza que _start sea el primer byte del binario porque ld coloca los
; objetos en el orden en que aparecen en su linea de comandos (no depende del
; orden de funciones dentro de un .c).

[bits 32]

extern main

global _start
_start:
    ; la ABI de System V deja en esp el argc, y justo encima el array argv.
    ; Se los pasamos a main en el orden que espera cdecl.
    mov eax, [esp]      ; argc
    lea edx, [esp + 4]  ; argv
    push edx
    push eax
    call main
    ; cli/hlt son privilegiadas (CPL<=IOPL, y ac corremos en ring3 con
    ; IOPL=0): ejecutarlas aca causaba un #GP en cuanto user_main() volvia.
    ; Sin una syscall de salida de tarea todavia, la unica forma valida de
    ; "terminar" en ring3 es quedarse girando -- el scheduler preventivo le
    ; sigue dando su rebanada al resto de las tareas igual.
.hang:
    jmp .hang
