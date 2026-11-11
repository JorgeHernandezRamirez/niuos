#include "stdlib.h"
#include "dirent.h"

int main(int argc, char *argv[]) {
    if(argc < 2){
        printf("You need to pass the path argument");
        exit(0); //TODO: Debemos implmenetar exit(1) error
    }
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0)
        return 1;
    char buffer[1024];
    int n;
    while ((n = read(fd, buffer, 1024)) > 0) {
        write(STDOUT_FILENO, buffer, n);
    }
    close(fd);
    exit(0);
    return 0;
}