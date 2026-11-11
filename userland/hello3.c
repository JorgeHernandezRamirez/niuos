#include "stdlib.h"
#include "dirent.h"

/*int main(int argc, char *argv[]) {
    const char *msg = "\n Bash v1.06: \n\n";
    printf(msg);
    printf("Arguments: \n");
    print_hex(argc);
    printf("\n");
    printf("\n");
    print_hex((unsigned int)&argc);
    print_hex((unsigned int)&argv);
    for(int i = 0; i < argc; i++){
        printf(argv[i]);
        printf("\n");
    }

    //AQUI
    int fd = open("/tmp/jorge.txt", O_CREAT);
    
    char buf[40];
    int bytes = read(fd, buf, 40);
    buf[bytes] = '\0';
    printf(buf);
    printf("\n");

    char *msg1 = "hola soy jorge. ";
    char *msg2 = "hola soy jose";
    write(fd, msg1, my_strlen(msg1));
    write(fd, msg2, my_strlen(msg2));
    seek(fd, 0, SEEK_SET);
    buf[40];
    bytes = read(fd, buf, 40);
    buf[bytes] = '\0';
    print_hex(bytes);
    printf("\n");
    printf(buf);
    printf("\n");
    close(fd);

    printf("Listar ficheros /tmp \n");
    DIR *dir = opendir("/tmp");
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        printf(entry->d_name);
        printf("\n");
    }

    for(;;){
        printf("$: ");
        char buf[40];
        int bytes = read(STDIN_FILENO, buf, 20);
        printf("Data writen: ");
        printf(buf);
        printf("\n");
    }
    exit(0);
    return 0;
}*/

int main() {
    int fd[2];
    pipe(fd);

    int pid = fork();

    printf("PIPE v1.04\n");

    if (pid == 0) {
        printf("Hijo\n");
        // HIJO: lee del pipe
        close(fd[1]);  // no escribe

        for(int i = 0; i < 100; i++){
            char buffer[100];
            int n = read(fd[0], buffer, sizeof(buffer) - 1);
            buffer[n] = '\0';
            printf(buffer);
        }
        close(fd[0]);
        printf("Fin Hijo pipo 1.04!!! \n");
        exit(0);
    } else {
        printf("Padre\n");
        // PADRE: escribe en el pipe
        close(fd[0]);  // no lee
        const char *mensaje = "Hola hijo soy jorge\n";
        for(int i = 0; i < 100; i++){
            write(fd[1], mensaje, strlen(mensaje));
        }
        close(fd[1]);
        exit(0);
    }
    
    return 0;
}