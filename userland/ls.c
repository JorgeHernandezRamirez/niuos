#include "stdlib.h"
#include "dirent.h"
#include "stat.h"

int main(int argc, char *argv[]) {
    if(argc < 2){
        printf("You need to pass the path argument");
        exit(0); //TODO: Debemos implmenetar exit(1) error
    }
    stat_t st;
    if(stat(argv[1], &st) < 0){
        printf("ls: cannot access ");
        printf(argv[1]);
        printf("\n");
        exit(0); //TODO: Debemos implmenetar exit(1) error
    }
    if(!S_ISDIR(st.st_mode)){ //un fichero suelto: ls solo imprime su path
        printf(argv[1]);
        printf("\n");
        exit(0);
    }
    DIR *dir = opendir(argv[1]);
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        printf(entry->d_name);
        printf("\n");
    }
    exit(0);
    return 0;
}