#include "stdlib.h"

#define BUF_SIZE  512
#define LINE_SIZE 512

static int contains(char *line, int len, char *pattern){
    int plen = strlen(pattern);
    if(plen == 0){
        return 1;
    }
    for(int i = 0; i + plen <= len; i++){
        int j = 0;
        while(j < plen && line[i + j] == pattern[j]){
            j++;
        }
        if(j == plen){
            return 1;
        }
    }
    return 0;
}

static void print_if_match(char *line, int len, char *pattern){
    if(len > 0 && contains(line, len, pattern)){
        write(STDOUT_FILENO, line, len);
        write(STDOUT_FILENO, "\n", 1);
    }
}

int main(int argc, char *argv[]) {
    if(argc < 2){
        printf("usage: grep <pattern> [file]\n");
        exit(0);
    }

    int fd = STDIN_FILENO;
    if(argc > 2){
        fd = open(argv[2], O_RDONLY);
        if(fd < 0){
            printf("grep: no can't open it ");
            printf(argv[2]);
            printf("\n");
            exit(0);
        }
    }

    char buf[BUF_SIZE];
    char line[LINE_SIZE];
    int len = 0;
    int n;

    while((n = read(fd, buf, BUF_SIZE)) > 0){
        for(int i = 0; i < n; i++){
            if(buf[i] == '\n'){
                print_if_match(line, len, argv[1]);
                len = 0;
            }
            else if(len < LINE_SIZE){
                line[len++] = buf[i];
            }
        }
    }
    print_if_match(line, len, argv[1]);

    if(fd != STDIN_FILENO){
        close(fd);
    }
    exit(0);
    return 0;
}
