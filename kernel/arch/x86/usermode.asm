; usermode.asm - arranque de la primera tarea del scheduler.

[bits 32]

global context_start

; void context_start(uint32_t esp) __attribute__((noreturn))
;
; Carga el esp de una tarea (apunta a un trap_frame_t: 8 registros de pusha +
; frame de iret, ver scheduler.c) y hace popa+iret -- exactamente lo mismo
; que el tail de irq0 (isr.asm) al conmutar de tarea. La unica diferencia es
; el recarga de DS/ES/FS/GS al selector de usuario ANTES del iret: en todo
; otro momento (arranque del kernel, IRQ0 interrumpiendo/retomando una tarea)
; esos registros o bien ya son de usuario (nunca los toca nadie mas, y todas
; las tareas comparten el mismo selector plano) o el propio iret los
; anularia si tuvieran DPL insuficiente para el CPL3 de destino -- pero aca,
; la primera vez, todavia valen lo que dejo gdt_flush (selector de kernel,
; DPL0 < CPL3 de destino), asi que hace falta ponerlos a mano una unica vez.
context_start:
    mov eax, [esp+4]       ; esp de la tarea entrante (leer antes de pisar esp)

    mov cx, 0x23             ; GDT_USER_DATA (DPL=3, RPL=3)
    mov ds, cx
    mov es, cx
    mov fs, cx

    ; %gs va al descriptor de almacenamiento local de hilo (GDT_USER_TLS,
    ; entrada 6). Su base la pone el scheduler antes de cada iret con la de
    ; la tarea entrante; para una tarea que nunca llamo a set_thread_area la
    ; base es 0 y el segmento es identico a GDT_USER_DATA. Recargarlo aqui
    ; es lo que hace que la CPU relea la base nueva del GDT.
    mov cx, 0x33             ; GDT_USER_TLS
    mov gs, cx

    mov esp, eax            ; saltar a la kstack de la tarea entrante
    popa
    iret                     ; entra a ring3 en (eip=entry, cs=user code, esp=user stack, ss=user data)
