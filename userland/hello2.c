#include "stdlib.h"

int main(void) {
    const char *msg = "Hello 2 new V1.04\n";
    int i;
    for(i=0; i<2; i++) {
        printf(msg);
        delay();
    }
    int pid = fork();
    if(pid){
        printf("Padre HELLO 2\n");
        int status;
        int pid = wait(&status);
        printf("Padre HELLO 2 DESPUES\n");
        for(;;){} //Mantenemos un proceso siempre arriba
    }
    else{
        /*printf("HIJO!!\n");
        char *argv[] = { "argumento1", "argumento2", NULL};
        execve("/bin/hello3.bin", argv);*/
        char *argv[] = { "/tmp/bar.txt", NULL};
        execve("/bin/cat.bin", argv);
    }
    exit(0);
    return 0;
}

