; boot.asm - MBR: imprime un mensaje, lee el kernel del disco, cambia a modo protegido y salta al kernel

[org 0x7C00]
[bits 16]

%ifndef KERNEL_SECTORS
KERNEL_SECTORS equ 96          ; numero de sectores a leer para el kernel. Normalmente lo fija el Makefile con -DKERNEL_SECTORS
%endif
KERNEL_LOAD_SEGMENT equ 0x1000 ; 0x1000:0x0000 = fisico 0x10000

%ifndef USER_SECTORS
USER_SECTORS equ 2          ; sectores del programa de usuario 1. Normalmente lo fija el Makefile con -DUSER_SECTORS
%endif
USER_LOAD_SEGMENT equ 0x2000   ; 0x2000:0x0000 = fisico 0x20000 (debe coincidir con userland/linker1.ld y kernel.c)

%ifndef USER2_SECTORS
USER2_SECTORS equ 2          ; sectores del programa de usuario 2. Normalmente lo fija el Makefile con -DUSER2_SECTORS
%endif
USER2_LOAD_SEGMENT equ 0x3000  ; 0x3000:0x0000 = fisico 0x30000 (debe coincidir con userland/linker2.ld y kernel.c)

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00      ; pila justo debajo del boot sector
    sti

    mov [boot_drive], dl   ; BIOS pasa el numero de unidad de arranque en DL

    mov si, msg_loading
    call print_string_rm

    call enable_a20
    call load_kernel
    call load_user_program
    call load_user_program2

    cli
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp CODE_SEG:protected_mode_start

; ---------- modo real: imprimir cadena via BIOS teletype ----------
print_string_rm:
    pusha
.next:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp .next
.done:
    popa
    ret

; ---------- habilitar linea A20 (fast A20 gate, puerto 0x92) esto es el puerto de I/0 x92. Hay que poner el bit 1 a 1 (por eso or al, 2 para que se puede acceder a mas de 1 MiB de memoria) ----------
enable_a20:
    in al, 0x92
    or al, 2
    out 0x92, al
    ret

; ---------- leer el kernel del disco con BIOS int 13h extendido (LBA) la interrupción 0x13 con 0x42 es leer de disco----------
load_kernel:
    mov si, dap ;le dice la dirección de memoria donde encontrar el kernel. secto1 (cada sector son 512 bytes, despues del MBR)
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc disk_error
    ret

; ---------- leer el programa de usuario 1, justo despues del kernel en disco ----------
load_user_program:
    mov si, dap2
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc disk_error
    ret

; ---------- leer el programa de usuario 2, justo despues del programa de usuario 1 ----------
load_user_program2:
    mov si, dap3
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc disk_error
    ret

disk_error:
    mov si, msg_disk_error
    call print_string_rm
    cli
    hlt

; Disk Address Packet para int 13h / AH=42h (16 bytes)
dap:
    db 0x10                     ; tamano del paquete
    db 0                        ; reservado
    dw KERNEL_SECTORS           ; sectores a leer
    dw 0x0000                   ; offset destino
    dw KERNEL_LOAD_SEGMENT      ; segmento destino
    dq 1                        ; LBA de inicio (sector 1, justo tras el MBR)

dap2:
    db 0x10                     ; tamano del paquete
    db 0                        ; reservado
    dw USER_SECTORS             ; sectores a leer
    dw 0x0000                   ; offset destino
    dw USER_LOAD_SEGMENT        ; segmento destino
    dq 1 + KERNEL_SECTORS       ; LBA de inicio (justo tras los sectores del kernel)

dap3:
    db 0x10                     ; tamano del paquete
    db 0                        ; reservado
    dw USER2_SECTORS            ; sectores a leer
    dw 0x0000                   ; offset destino
    dw USER2_LOAD_SEGMENT       ; segmento destino
    dq 1 + KERNEL_SECTORS + USER_SECTORS  ; LBA de inicio (justo tras el programa de usuario 1)

boot_drive:     db 0
msg_loading:    db "Cargando kernel...", 13, 10, 0
msg_disk_error: db "Error de disco!", 0

[bits 32]
protected_mode_start:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    jmp 0x10000        ; saltar al kernel ya cargado en memoria fisica

; ---------- GDT minima: nulo, codigo 32-bit, datos 32-bit (modelo plano) ----------
gdt_start:
gdt_null:
    dq 0

gdt_code:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b   ; presente, ring0, codigo, ejecutable, legible
    db 11001111b   ; granularidad 4K, 32-bit, limite 16-19
    db 0x00

gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b   ; presente, ring0, datos, escribible
    db 11001111b
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

times 510 - ($ - $$) db 0
dw 0xAA55
