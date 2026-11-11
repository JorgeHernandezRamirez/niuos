#include "stdlib.h"

/* prueba del cargador ELF: una global inicializada (va a .data, segmento RW)
   y otra sin inicializar (va a .bss, hace que p_memsz > p_filesz). */
int global_init = 0x1234;
int global_bss;

int main(void) {
    char *map = mmap2(NULL, 16000, PROT_WRITE, MAP_ANONYMOUS, -1, 0);
    print_hex((unsigned int) map);
    for(int j=0; j<16000; j++) {
        map[j] = (char) j;
    }
    printf("mmap escrito\n");
    int size = 16430;
    int fd = open("/tmp/bar.txt", O_RDONLY);
    print_hex(fd);
    char *file = mmap2(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    print_hex((unsigned int) file);
    for(int off = 0; off < size; off += 64) {
        int bytes = size - off < 64 ? size - off : 64;
        write(STDOUT_FILENO, file + off, bytes);
    }
    printf("\n");
    printf("hello1.bin v0.07\n");
    print_hex(global_init);
    print_hex(global_bss);
    char *msg1 = malloc(32);
    strcpy(msg1, "Hello 1 malloc\n");
    print_hex((unsigned int) msg1);
    char *msg2 = malloc(4096);
    print_hex((unsigned int) msg2);
    strcpy(msg2, "Hello 2 malloc\n");
    char *msg = "Hello 1\n";
    int i;
    for(i=0; i<5; i++) {
        printf(msg);
        delay();
        printf(msg1);
    }
    int pid = fork();
    if(pid){
        printf("Padre\n");
        int status;
        int pid = wait(&status);
        printf("Padre despues\n");
        print_hex(pid);
        print_hex(status);
        printf("Padre despues II\n");
    }
    else{
        printf("Hijo\n");
        print_hex(pid);
        printf("XXXX\n");
        for(i=0; i<5; i++) {
            printf("Hijo msg\n");
            delay();
        }
    }
    exit(0);
    for(;;){}
    /*msg = "Hello 1 Despues\n";
    for(i=0; i<20; i++) {
        printf(msg);
        delay();
    }
    for(;;){}
    return 0;*/
}
