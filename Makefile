CC := i686-elf-gcc
LD := i686-elf-ld
OBJCOPY := i686-elf-objcopy
NASM := nasm

CFLAGS := -m32 -ffreestanding -fno-pic -fno-stack-protector -Wall -Wextra -c

# presupuestos de sectores de disco: deben coincidir con boot/boot.asm (KERNEL_SECTORS/USER_SECTORS/USER2_SECTORS).
# truncate corta en silencio si el binario ya excede el presupuesto (nos paso una vez con el kernel
# al añadir gdt.c), asi que cada regla de *.bin comprueba el tamaño antes de truncar.
KERNEL_SECTORS := 96
KERNEL_BUDGET  := 49152  # $(KERNEL_SECTORS) * 512
USER_SECTORS   := 4
USER_BUDGET    := 2048   # $(USER_SECTORS) * 512
USER2_SECTORS  := 2
USER2_BUDGET   := 1024   # $(USER2_SECTORS) * 512

# fs.dmg empieza justo detras de todo lo anterior en el disco:
#   cat boot.bin kernel.bin hello1.bin hello2.bin fs.dmg
# Se calcula aqui y se le pasa al kernel y al bootsector para que no haya que
# acordarse de tocar tres ficheros cada vez que cambia un presupuesto: eso ya
# nos paso, y el sintoma es que el FAT16 deja de montarse sin decir por que.
FS_LBA_START := $(shell expr 1 + $(KERNEL_SECTORS) + $(USER_SECTORS) + $(USER2_SECTORS))
CFLAGS += -DFS_LBA_START=$(FS_LBA_START)

# hello3 todavia no esta en el esquema de sectores crudos de boot.asm (ese
# mecanismo esta muerto: scheduler_add_task() esta comentado en kernel.c, las
# tareas se cargan por FAT16 desde fs.dmg via scheduler_new_task()). Este
# budget solo limita el tamano del .bin generado, sin USER3_SECTORS asociado.
USER3_BUDGET   := 2048

# Programas de userland que se cargan por FAT16 desde fs.dmg (mismo caso que
# hello3): no tienen sectores reservados en boot.asm, comparten el linker
# generico userland/linker.ld y este presupuesto, que solo limita el tamano del
# .bin generado. Para anadir uno nuevo basta con ponerlo en USER_PROGS.
USER_PROGS       := ls cat bash grep fib
USER_PROG_ELFS   := $(USER_PROGS:%=userland/%.elf)
USER_PROG_BINS   := $(USER_PROGS:%=userland/%.bin)
USER_PROG_BUDGET := 2048

# kernel_entry.o va primero: su _start debe ser el primer byte del kernel (linker.ld lo carga en 0x10000)
ASM_OBJECTS := kernel/arch/x86/kernel_entry.o kernel/arch/x86/isr.o kernel/arch/x86/usermode.o
C_OBJECTS := kernel/kernel.o kernel/syscall.o kernel/arch/x86/gdt.o kernel/arch/x86/idt.o kernel/arch/x86/fault.o kernel/mm/pmm.o kernel/mm/paging.o kernel/sched/scheduler.o kernel/sched/elf.o kernel/sched/page.o kernel/drivers/screen.o kernel/drivers/keyboard.o kernel/drivers/pit.o kernel/drivers/disk.o kernel/drivers/ata/ata.o kernel/fs/fs.o kernel/fs/fat16/fat16.o kernel/fd/fd.o kernel/fd/tty.o kernel/fd/pipe.o kernel/lib/queue.o kernel/lib/lib.o

# crt0.o va primero: su _start debe ser el primer byte del binario (userland/linker1.ld / linker2.ld lo cargan en 0x20000 / 0x30000)
USER_ASM_OBJECTS := userland/crt0.o
USER_C_OBJECTS := userland/hello1.o
USER2_C_OBJECTS := userland/hello2.o
USER3_C_OBJECTS := userland/hello3.o

all: os-image.bin userland/hello3.bin $(USER_PROG_BINS)

boot/boot.bin: boot/boot.asm Makefile
	$(NASM) -f bin -DKERNEL_SECTORS=$(KERNEL_SECTORS) -DUSER_SECTORS=$(USER_SECTORS) -DUSER2_SECTORS=$(USER2_SECTORS) $< -o $@

kernel/%.o: kernel/%.asm
	$(NASM) -f elf32 $< -o $@

kernel/%.o: kernel/%.c Makefile kernel/errno.h kernel/lib/lib.h kernel/lib/queue.h kernel/syscall.h kernel/arch/arch.h kernel/arch/x86/io.h kernel/arch/x86/gdt.h kernel/arch/x86/idt.h kernel/arch/x86/usermode.h kernel/arch/x86/fault.h kernel/mm/pmm.h kernel/mm/paging.h kernel/sched/scheduler.h kernel/sched/elf.h kernel/sched/task.h kernel/sched/page.h kernel/drivers/screen.h kernel/drivers/pit.h kernel/drivers/disk.h kernel/drivers/ata/ata.h kernel/fs/fs.h kernel/fs/fat16/fat16.h kernel/fd/fd.h kernel/fd/tty.h kernel/fd/pipe.h
	$(CC) $(CFLAGS) $< -o $@

kernel/kernel.elf: $(ASM_OBJECTS) $(C_OBJECTS) kernel/linker.ld
	$(LD) -m elf_i386 -T kernel/linker.ld -o $@ $(ASM_OBJECTS) $(C_OBJECTS)

kernel/kernel.bin: kernel/kernel.elf
	$(OBJCOPY) -O binary $< $@
	@size=$$(wc -c < $@); \
	if [ $$size -gt $(KERNEL_BUDGET) ]; then \
		echo "ERROR: $@ ($$size bytes) excede el presupuesto de KERNEL_SECTORS=$(KERNEL_SECTORS) ($(KERNEL_BUDGET) bytes)."; \
		echo "Sube KERNEL_SECTORS y KERNEL_BUDGET en este Makefile: el bootsector y FS_LBA_START salen de ahi."; \
		echo "Tope: el kernel se carga en 0x10000 y el programa 1 en 0x20000, o sea 128 sectores."; \
		exit 1; \
	fi
	truncate -s $(KERNEL_BUDGET) $@

userland/%.o: userland/%.asm
	$(NASM) -f elf32 $< -o $@

userland/%.o: userland/%.c userland/stdlib.h userland/dirent.h userland/stat.h
	$(CC) $(CFLAGS) $< -o $@

userland/hello1.elf: $(USER_ASM_OBJECTS) $(USER_C_OBJECTS) userland/linker1.ld
	$(LD) -m elf_i386 -T userland/linker1.ld -o $@ $(USER_ASM_OBJECTS) $(USER_C_OBJECTS)

userland/hello1.bin: userland/hello1.elf
	$(OBJCOPY) -O binary $< $@
	@size=$$(wc -c < $@); \
	if [ $$size -gt $(USER_BUDGET) ]; then \
		echo "ERROR: $@ ($$size bytes) excede el presupuesto de USER_SECTORS=$(USER_SECTORS) ($(USER_BUDGET) bytes)."; \
		echo "Sube USER_SECTORS en boot/boot.asm y USER_SECTORS/USER_BUDGET en este Makefile."; \
		exit 1; \
	fi
	truncate -s $(USER_BUDGET) $@

userland/hello2.elf: $(USER_ASM_OBJECTS) $(USER2_C_OBJECTS) userland/linker2.ld
	$(LD) -m elf_i386 -T userland/linker2.ld -o $@ $(USER_ASM_OBJECTS) $(USER2_C_OBJECTS)

userland/hello2.bin: userland/hello2.elf
	$(OBJCOPY) -O binary $< $@
	@size=$$(wc -c < $@); \
	if [ $$size -gt $(USER2_BUDGET) ]; then \
		echo "ERROR: $@ ($$size bytes) excede el presupuesto de USER2_SECTORS=$(USER2_SECTORS) ($(USER2_BUDGET) bytes)."; \
		echo "Sube USER2_SECTORS en boot/boot.asm y USER2_SECTORS/USER2_BUDGET en este Makefile."; \
		exit 1; \
	fi
	truncate -s $(USER2_BUDGET) $@

userland/hello3.elf: $(USER_ASM_OBJECTS) $(USER3_C_OBJECTS) userland/linker3.ld
	$(LD) -m elf_i386 -T userland/linker3.ld -o $@ $(USER_ASM_OBJECTS) $(USER3_C_OBJECTS)

userland/hello3.bin: userland/hello3.elf
	$(OBJCOPY) -O binary $< $@
	@size=$$(wc -c < $@); \
	if [ $$size -gt $(USER3_BUDGET) ]; then \
		echo "ERROR: $@ ($$size bytes) excede el presupuesto de USER3_BUDGET ($(USER3_BUDGET) bytes)."; \
		echo "Sube USER3_BUDGET en este Makefile."; \
		exit 1; \
	fi
	truncate -s $(USER3_BUDGET) $@

# static pattern rules: solo aplican a los targets de $(USER_PROGS), asi que no
# pisan las reglas explicitas de hello1/hello2/hello3 (que usan linker1/2/3.ld).
$(USER_PROG_ELFS): userland/%.elf: $(USER_ASM_OBJECTS) userland/%.o userland/linker.ld
	$(LD) -m elf_i386 -T userland/linker.ld -o $@ $(USER_ASM_OBJECTS) userland/$*.o

$(USER_PROG_BINS): userland/%.bin: userland/%.elf
	$(OBJCOPY) -O binary $< $@
	@size=$$(wc -c < $@); \
	if [ $$size -gt $(USER_PROG_BUDGET) ]; then \
		echo "ERROR: $@ ($$size bytes) excede el presupuesto de USER_PROG_BUDGET ($(USER_PROG_BUDGET) bytes)."; \
		echo "Sube USER_PROG_BUDGET en este Makefile."; \
		exit 1; \
	fi
	truncate -s $(USER_PROG_BUDGET) $@

os-image.bin: boot/boot.bin kernel/kernel.bin userland/hello1.bin userland/hello2.bin fs.dmg
	cat boot/boot.bin kernel/kernel.bin userland/hello1.bin userland/hello2.bin fs.dmg > $@
	@size=$$(wc -c < $@); \
	budget=$$(( 512 + $(KERNEL_BUDGET) + $(USER_BUDGET) + $(USER2_BUDGET) + $$(wc -c < fs.dmg) )); \
	if [ $$size -ne $$budget ]; then \
		echo "ERROR: os-image.bin ($$size bytes) no coincide con el tamaño esperado ($$budget bytes)."; \
		exit 1; \
	fi

# En macOS con pantalla Retina, QEMU dibuja el framebuffer VGA (720x400 en modo
# texto) pixel a pixel, asi que la ventana sale diminuta. zoom-to-fit=on deja
# escalar el contenido al redimensionar la ventana.
#
# El backend cocoa NO tiene opcion de tamano inicial de ventana (ver
# 'qemu-system-i386 -help': solo full-grab/swap-opt-cmd/show-cursor/
# left-command-key/full-screen/zoom-to-fit), y -g WxH no existe en x86. Para
# arrancar ya grande hay dos targets:
#   make run-full  -> cocoa a pantalla completa
#   make run-term  -> dentro del terminal (curses), el tamano lo manda la
#                     fuente/ventana del terminal y no hay escalado borroso
QEMU     := qemu-system-i386
QEMU_MEM := 32M
QEMU_DISPLAY := cocoa,zoom-to-fit=on

run: os-image.bin
	$(QEMU) -m $(QEMU_MEM) -display $(QEMU_DISPLAY) -drive format=raw,file=os-image.bin

run-full: os-image.bin
	$(QEMU) -m $(QEMU_MEM) -display cocoa,zoom-to-fit=on,full-screen=on -drive format=raw,file=os-image.bin

# curses lee el framebuffer de texto VGA y lo pinta con la fuente del terminal.
# Salir: Ctrl-A X (o Ctrl-A C para el monitor). Necesita un terminal de 80x25
# como minimo, o recorta la pantalla.
run-term: os-image.bin
	$(QEMU) -m $(QEMU_MEM) -display curses -drive format=raw,file=os-image.bin

clean:
	rm -f boot/boot.bin kernel/*.o kernel/arch/x86/*.o kernel/mm/*.o kernel/sched/*.o kernel/drivers/*.o kernel/drivers/ata/*.o kernel/fs/*.o kernel/fs/fat16/*.o kernel/fd/*.o kernel/lib/*.o kernel/kernel.elf kernel/kernel.bin userland/*.o userland/hello1.elf userland/hello1.bin userland/hello2.elf userland/hello2.bin userland/hello3.elf userland/hello3.bin $(USER_PROG_ELFS) $(USER_PROG_BINS) os-image.bin

.PHONY: all run run-full run-term clean
