#ifndef STDLIB_H
#define STDLIB_H

#define NULL ((void*)0)

//Esto debería ir en stdio.h
#define SEEK_SET 0

#define SEEK_CUR 1

#define SEEK_END 2

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR   0x0002
#define O_CREAT  0x0040
#define O_TRUNC  0x0200

static inline char *strcpy(char *dest, char *src){
    char *ret = dest;
    while ((*dest++ = *src++) != '\0');
    return ret;
}

static int strlen(char *s) {
    int n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

static int my_strlen(char *s) {
    return strlen(s);
}

static inline int write(int fd, char *buf, int bytes) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)               /* eax de salida -> ret */
        : "a" (4), "b" (fd), "c" ((int) buf), "d" (bytes)  /* eax=4 (SYS_WRITE), ebx=fd, ecx=buf, edx=bytes */
    );
    return ret;
}

static inline int printf(char *buf) {
    return write(STDOUT_FILENO, buf, my_strlen(buf));
}

static inline int read(int fd, char *buf, int bytes) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)               /* eax de salida -> ret */
        : "a" (3), "b" (fd), "c" ((int) buf), "d" (bytes)  /* eax=3 (SYS_READ), ebx=fd, ecx=buf, edx=bytes */
    );
    return ret;
}

static inline int open(char *path, int flags) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)               /* eax de salida -> ret (fd, o -1 si fallo) */
        : "a" (5), "b" ((int) path), "c" (flags)  /* eax=5 (SYS_OPEN), ebx=path, ecx=flags */
    );
    return ret;
}

static inline int seek(int fd, long offset, int whence) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)               /* eax de salida -> ret */
        : "a" (19), "b" (fd), "c" (offset), "d" (whence)  /* eax=19 (SYS_SEEK), ebx=fd, ecx=offset, edx=whence */
    );
    return ret;
}

static inline int close(int fd) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)               /* eax de salida -> ret */
        : "a" (6), "b" (fd)        /* eax=6 (SYS_CLOSE), ebx=fd */
    );
    return ret;
}

static inline int dup2(int oldfd, int newfd) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)               /* eax de salida -> ret (newfd, o -1 si fallo) */
        : "a" (63), "b" (oldfd), "c" (newfd)  /* eax=63 (SYS_DUP2), ebx=oldfd, ecx=newfd */
    );
    return ret;
}

static inline void exit(int status) {
    __asm__ volatile (
        "int $0x80"
        :
        : "a" (1), "b" (status)    /* eax=1 (SYS_EXIT), ebx=status */
    );
    for(;;){} /* nunca deberia volver: el kernel para la tarea antes del iret */
}

static inline int fork(void) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)               /* eax de salida -> ret */
        : "a" (2)                  /* eax=2 (SYS_FORK) */
    );
    return ret;
}

static inline int wait(int *status) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)               /* eax de salida -> ret (pid del hijo que termino) */
        : "a" (114), "b" ((int) status)  /* eax=114 (SYS_WAIT), ebx=status */
    );
    return ret;
}

static inline int waitpid(int pid, int *status) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)               /* eax de salida -> ret (pid del hijo que termino) */
        : "a" (7), "b" (pid), "c" ((int) status)  /* eax=114 (SYS_WAIT), ebx=status */
    );
    return ret;
}

static inline int execve(char *path, char **argv) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)               /* eax de salida -> ret */
        : "a" (11), "b" ((int) path), "c" ((int) argv)  /* eax=11 (SYS_EXECVE), ebx=path, ecx=argv */
    );
    return ret;
}

static inline int pipe(int fds[2]) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)               /* eax de salida -> ret */
        : "a" (42), "b" ((int) fds)  /* eax=42 (SYS_PIPE), ebx=fds */
    );
    return ret;
}

static inline void *malloc(int size) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)               /* eax de salida -> ret */
        : "a" (45), "b" (size)     /* eax=45 (SYS_BRK), ebx=size */
    );
    return (void *) ret;
}

#define PROT_READ     0x1
#define PROT_WRITE    0x2
#define MAP_PRIVATE   0x02
#define MAP_ANONYMOUS 0x20

static inline void *mmap2(void *addr, int length, int prot, int flags, int fd, int offset) {
    int ret;
    int pgoffset = offset / 4096; /* mmap2 recibe el offset en paginas */
    /* el 6o argumento va en ebp, que gcc no deja usar como restriccion:
       se apila pgoffset antes de tocar ebp y se carga desde la pila */
    __asm__ volatile (
        "push %[pgoff]\n\t"
        "push %%ebp\n\t"
        "mov 4(%%esp), %%ebp\n\t"
        "int $0x80\n\t"
        "pop %%ebp\n\t"
        "add $4, %%esp"
        : "=a" (ret)               /* eax de salida -> ret (direccion virtual) */
        : "a" (192), "b" ((int) addr), "c" (length), "d" (prot),
          "S" (flags), "D" (fd), [pgoff] "g" (pgoffset)  /* eax=192 (SYS_MMAP2) */
        : "memory"
    );
    return (void *) ret;
}

static void print_hex(unsigned int val) {
    const char *hex = "0123456789abcdef";
    char buf[12]; /* "0x" + 8 digitos + '\n' + '\0' */
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 8; i++) {
        buf[2 + i] = hex[(val >> ((7 - i) * 4)) & 0xF];
    }
    buf[10] = '\n';
    buf[11] = '\0';
    printf(buf);
}

static void delay(void) {
    volatile unsigned long i;
    for (i = 0; i < 200000000UL; i++) {
        /* no hacer nada: solo espaciar los prints. Como el scheduler ahora
           es preventivo, este bucle ya NO bloquea al otro proceso mientras
           corre -- IRQ0 le sigue dando su rebanada de CPU igual. */
    }
}

static void less_delay(void) {
    volatile unsigned long i;
    for (i = 0; i < 20000000UL; i++) {
        /* no hacer nada: solo espaciar los prints. Como el scheduler ahora
           es preventivo, este bucle ya NO bloquea al otro proceso mientras
           corre -- IRQ0 le sigue dando su rebanada de CPU igual. */
    }
}

#endif
