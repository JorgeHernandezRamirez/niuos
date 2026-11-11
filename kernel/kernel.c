/* kernel.c - kernel minimo con pantalla, teclado y syscalls por interrupciones */

#include "drivers/screen.h"
#include "arch/x86/gdt.h"
#include "arch/x86/idt.h"
#include "syscall.h"
#include "sched/scheduler.h"
#include "sched/elf.h"
#include "drivers/pit.h"
#include "mm/pmm.h"
#include "mm/paging.h"
#include "mm/pmm.h"
#include "drivers/disk.h"
#include "fs/fat16/fat16.h"
#include "fs/fs.h"
#include "lib/lib.h"

/* fs.dmg arranca justo despues del programa de usuario 2 en disco (ver
   Makefile: cat boot.bin kernel.bin hello1.bin hello2.bin fs.dmg). LBA =
   1 (MBR) + KERNEL_SECTORS + USER_SECTORS + USER2_SECTORS. Lo calcula el
   Makefile y lo pasa con -DFS_LBA_START, que es el mismo sitio del que sale
   el KERNEL_SECTORS del bootsector: asi no pueden quedar descuadrados. */
#ifndef FS_LBA_START
#define FS_LBA_START 101u
#endif

#define TIMER_HZ 100 /* frecuencia del tick del scheduler: 100 Hz = 10ms */

void print_files(char *path){
    int32_t* ff_dir = fs_driver_instance.opendir(path);
    dirent *value = fat16_readdir((FileFAT *)ff_dir);
    while(value != NULL){
        char b[11];
        memcpy(b, value->d_name, 11);
        screen_print(b);
        screen_print("\n");
        value = fat16_readdir((FileFAT *)ff_dir);
    }
}

void print_content(char *file_path, int flags){
    uint32_t* ff = fs_driver_instance.open(file_path, flags);
    if(ff == NULL){
        screen_print("Fichero no existe o no es un fichero\n");
    }
    else{
        screen_print("Fichero existe!\n");
        uint32_t *line = vmalloc(4097);
        int bytes_readed = 0;
        do{
            bytes_readed = fs_driver_instance.read(ff, 4096, line);
            ((char *)line)[bytes_readed] = '\0';
            screen_print((char *)line);
        }while(bytes_readed == 4096);
    }
}

static void delay(void) {
    volatile unsigned long i;
    for (i = 0; i < 2000000UL; i++) {
        /* no hacer nada: solo espaciar los prints. Como el scheduler ahora
           es preventivo, este bucle ya NO bloquea al otro proceso mientras
           corre -- IRQ0 le sigue dando su rebanada de CPU igual. */
    }
}

void kmain(void) {
    screen_init();
    screen_print(
" _   _ ___ _   _  ___  ____ \n"
"| \\ | |_ _| | | |/ _ \\/ ___|\n"
"|  \\| || || | | | | | \\___ \\\n"
"| |\\  || || |_| | |_| |___) |\n"
"|_| \\_|___|\\___/ \\___/|____/ \n"
"--------------------------------\n"
"niuOS v0.01\n"
"--------------------------------\n\n"
);

    gdt_init();
    screen_print("GDT+TSS............................OK\n"); /* smoke test: si lgdt/gdt_flush/ltr fallan, QEMU resetea antes de llegar aqui */

    idt_init();
    screen_print("IDT................................OK\n");

    __asm__ volatile ("cli");

    pmm_init();
    screen_print("PMM................................OK frames libres=");
    screen_print_hex32(pmm_free_frame_count());
    screen_print("\n");

    paging_init();
    paging_enable();
    screen_print("PAGING (identity)..................OK\n");

    screen_print("KERNEL HEAP........................OK\n");
    kernel_heap_init();

    init_default_fds();
    init_disk_driver();
    init_fs_driver();
    disk_driver_instance.open();
    fs_driver_instance.fs_init(FS_LBA_START);    
    print_files("/tmp");
    print_content("/tmp/foo.txt", O_RDONLY);
    
    //scheduler_new_task("/bin/hello1.bin");
    //scheduler_new_task("/bin/hello2.bin");
    //scheduler_new_task("/bin/hello3.bin");
    scheduler_new_task("/bin/bash.bin");
    screen_print("SCHEDULER..........................OK\n");

    pit_init(TIMER_HZ);
    screen_print("PIT................................OK\n");

    screen_print("\n\n");
    scheduler_start();
}
