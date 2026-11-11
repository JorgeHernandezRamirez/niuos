; isr.asm - puntos de entrada de bajo nivel para interrupciones
; La CPU salta aqui automaticamente cuando llega la interrupcion correspondiente.

[bits 32]

extern keyboard_handler
extern syscall_handler
extern scheduler_next_task
extern gp_fault_handler
extern page_fault_handler
global irq0
global schedule_kernel
global irq1
global isr128
global isr13
global isr14
global gdt_flush

; ---------- recarga los registros de segmento tras cargar un nuevo GDT ----------
; CS no se puede recargar con "mov": hace falta un far jump. DS/ES/FS/GS/SS
; se recargan aunque los selectores (0x08/0x10) no cambien de valor, porque la
; CPU cachea base/limite/derechos del segmento al cargarlo, no al leer el GDTR.
gdt_flush:
    jmp 0x08:.flush
.flush:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret

irq1:
    pusha                 ; guarda eax,ecx,edx,ebx,esp,ebp,esi,edi
    call keyboard_handler
    popa                  ; restaura los registros
    iret                  ; vuelve del handler (pop de EIP, CS, EFLAGS)

; ---------- timer (IRQ0, PIT): tick del scheduler preventivo ----------
; A diferencia de irq1, aqui SI cambiamos de pila: pusha deja el estado
; completo de la tarea interrumpida en SU kstack (ver task.h/scheduler.c).
; schedule() decide cual tarea sigue y devuelve el esp desde el que hay que
; retomarla -- puede ser el que dejo su propio pusha anterior, o el frame
; prefabricado por scheduler_new_task() si es la primera vez que corre.
irq0:
    pusha
    push esp                 ; unico argumento de schedule(uint32_t old_esp)
    call scheduler_next_task  ; devuelve en eax el esp de la tarea entrante
    mov esp, eax             ; saltar a la pila de kernel de la tarea entrante
    ; %gs cachea base/limite del descriptor desde que se cargo el selector, y
    ; iret NO lo recarga. Como scheduler_next_task() acaba de reescribir la
    ; base de GDT_USER_TLS con la de la tarea entrante, hay que volver a
    ; cargar el selector aqui o la tarea correria con el TLS de la anterior.
    ; cx se pisa libremente: popa restaura todos los registros justo despues.
    mov cx, 0x33             ; GDT_USER_TLS
    mov gs, cx
    popa
    iret                     ; retoma esa tarea en ring3 donde se haya quedado

; ---------- bloqueo voluntario dentro de modo kernel (no via interrupcion) ----------
; Llamada como funcion C comun (no por hardware) desde mitad de una syscall
; que necesita dormir la tarea actual (tty_read, futuro pipe) y retomarla mas
; adelante exactamente en el punto de la llamada -- ver scheduler_block_current()
; en scheduler.c. Arma a mano un trap_frame_t "de mismo privilegio" (sin
; ESP/SS, porque nunca salimos de ring 0: CS que empujamos == CS actual, asi
; que iret no va a exigirlos) para poder reusar tal cual el mismo mecanismo
; de retomo que ya usan irq0/context_start (iret via tasks[].esp), sin
; necesitar un camino de resumen aparte.
;
; "call irq_nuevo" ya dejo en la pila UNA sola cosa: la direccion de retorno
; (nuestro futuro EIP). La sacamos y la volvemos a empujar en el orden que
; iret espera (EIP mas cerca de esp, se popea primero; CS y EFLAGS debajo).
schedule_kernel:
    pop eax                  ; eax = direccion de retorno que dejo "call irq_nuevo"
    pushfd                    ; EFLAGS actual
    push cs                    ; CS actual (kernel, ring 0)
    push eax                   ; EIP (el que sacamos) -- el primero que iret va a leer
    pusha
    push esp                  ; unico argumento de scheduler_next_task(uint32_t old_esp)
    call scheduler_next_task  ; devuelve en eax el esp de la tarea entrante
    mov esp, eax              ; saltar a la pila de kernel de la tarea entrante
    mov cx, 0x33              ; GDT_USER_TLS: recargar %gs, ver irq0
    mov gs, cx
    popa
    iret                      ; retoma esa tarea (ring0 "de mismo privilegio" o ring3, segun corresponda)

; ---------- syscall (int 0x80): eax=numero, ebx/ecx/edx=argumentos ----------
; A diferencia de irq1, aqui SI necesitamos pasarle los registros a la funcion
; de C como argumentos normales (cdecl), porque syscall_handler necesita leer
; el numero de syscall y sus argumentos, no solo "hacer algo y volver".
; Ademas hace falta el pusha/popa (igual que irq0/isr14): sin el, el bloque
; de 8 registros que deja pusha en la pila de kernel (el mismo layout que lee
; trap_frame_t en scheduler.c) queda con basura -- los push sueltos de aqui
; abajo mas la direccion de retorno de "call syscall_handler" pisan justo esa
; zona. Eso no se nota en una tarea que sigue corriendo normal (los registros
; en la CPU nunca se tocan, solo se leen para armar los argumentos), pero
; scheduler_fork() SI lee esa zona de memoria (via kstack_top(current)) para
; clonarla al hijo, y ahi termina copiando esa basura como si fueran los
; registros del padre.
isr128:
    pusha                  ; snapshot completo de la tarea (layout trap_frame_t)
    push ebp              ; 7o argumento (arg6)
    push edi              ; 6o argumento (arg5)
    push esi              ; 5o argumento (arg4)
    push edx              ; 4o argumento (arg3)
    push ecx              ; 3er argumento (arg2)
    push ebx              ; 2o argumento (arg1)
    push eax              ; 1er argumento (num) -- el ultimo empujado es el 1er argumento en cdecl
    call syscall_handler  ; el compilador acceder a los elementos haciendo mov eax, [ebp+8] -> Accede a direccion de memoria
    add esp, 28           ; limpia los 7 argumentos empujados (7 x 4 bytes)
    mov [esp+28], eax     ; parchea el EAX del snapshot de pusha con el valor de retorno (offset del campo eax dentro del bloque de pusha)
    popa                   ; restaura los registros de la tarea (EAX ya con el valor de retorno)
    iret

; ---------- #GP (13): excepcion de CPU con error code ----------
; La CPU empuja el error code ANTES de EIP/CS/EFLAGS/... (queda en [esp+0] al
; entrar aca). gp_fault_handler es noreturn (siempre halt_forever()), asi que
; no hace falta limpiar el error code de la pila ni un iret.
isr13:
    mov eax, [esp]         ; error_code
    push eax
    call gp_fault_handler   ; void gp_fault_handler(uint32_t error_code) -- no retorna

; ---------- #PF (14): fallo de pagina, con recuperacion ----------
; A diferencia de isr13, page_fault_handler SI puede volver: si el fallo es
; heap/stack de una tarea creciendo, mapea la pagina que falta y retorna para
; reintentar la misma instruccion. Por eso hace falta pusha/popa como en
; irq0 -- la call de abajo pisa eax/ecx/edx (caller-saved) y esos son
; registros de la tarea interrumpida, no del kernel -- y hay que descartar a
; mano el error code antes del iret (iret no lo hace solo).
isr14:
    pusha                    ; guarda el estado completo de la tarea que fallo
    mov eax, cr2             ; direccion que fallo, la CPU la deja en CR2
    push eax                 ; arg2 = cr2
    mov eax, [esp+36]        ; error_code: 32 de pusha + 4 del push de arriba
    push eax                 ; arg1 = error_code
    call page_fault_handler  ; void page_fault_handler(uint32_t error_code, uint32_t cr2)
    add esp, 8                ; limpiar los 2 argumentos empujados
    popa                      ; restaurar el estado de la tarea (si no hubo halt)
    add esp, 4                ; descartar el error code que empujo la CPU
    iret                      ; reintenta la instruccion que causo el fault
