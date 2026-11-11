#include "stdlib.h"
#include "dirent.h"

#define MAX_ARGS 16

static int is_space(char c){
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static int parse_argv(char *line, char *out[], int max_args){
    int argc = 0;
    char *p = line;

    while(*p != '\0' && argc < max_args - 1){
        while(is_space(*p)){
            *p = '\0';   /* cierra el token anterior */
            p++;
        }
        if(*p == '\0'){
            break;
        }
        out[argc++] = p;
        while(*p != '\0' && !is_space(*p)){
            p++;
        }
    }

    out[argc] = NULL;
    return argc;
}

static int find_pipe(char *argv[], int argc){
    for(int i = 0; i < argc; i++){
        if(argv[i][0] == '|' && argv[i][1] == '\0'){
            return i;
        }
    }
    return -1;
}

static void run(char *argv[]){
    int pid = fork();
    if(pid){
        int status;
        wait(&status);
    }
    else{
        execve(argv[0], &argv[1]);
        exit(0);
    }
}

static void run_pipeline(char *left[], char *right[]){
    int fds[2];
    pipe(fds);

    int pid_left = fork();
    if(pid_left == 0){
        dup2(fds[1], STDOUT_FILENO);
        close(fds[0]);
        //close(fds[1]);
        execve(left[0], &left[1]);
        exit(0);
    }

    int pid_right = fork();
    if(pid_right == 0){
        dup2(fds[0], STDIN_FILENO);
        //close(fds[0]);
        close(fds[1]);
        execve(right[0], &right[1]);
        exit(0);
    }

    int status;
    close(fds[1]);
    close(fds[0]);
    wait(&status);
    wait(&status);
}

int main(int argc, char *argv[]) {
    for(;;){
        printf("$:> ");
        char buf[128];
        int bytes = read(STDIN_FILENO, buf, sizeof(buf) - 1);
        if(bytes <= 0){
            continue;
        }
        buf[bytes] = '\0';

        char *cmd_argv[MAX_ARGS];
        int cmd_argc = parse_argv(buf, cmd_argv, MAX_ARGS);
        if(cmd_argc == 0){ /* solo pulso enter */
            continue;
        }

        int at = find_pipe(cmd_argv, cmd_argc);
        if(at < 0){
            run(cmd_argv);
        }
        else if(at == 0 || cmd_argv[at + 1] == NULL){
            printf("bash: sintaxis: cmd1 | cmd2\n");
        }
        else{
            cmd_argv[at] = NULL;
            run_pipeline(cmd_argv, &cmd_argv[at + 1]);
        }
    }
    exit(0);
    return 0;
}
