/* syscall.c - despachador de llamadas al sistema (int 0x80) */

#include "syscall.h"
#include "drivers/screen.h"
#include "sched/scheduler.h"
#include "arch/x86/gdt.h"
#include "errno.h"
#include "fd/fd.h"
#include "sched/task.h"
#include "fs/fs.h"
#include "lib/lib.h"

int _syscall_pipe(int *fds) {
    return scheduler_new_pipe(fds);
}

int _syscall_write(int fd_num, char *buf, int bytes) {
    fd_struct *fd = scheduler_current_task_fd(fd_num);
    if(fd==NULL){
        return -EBADF;
    }
    return fd->write(fd, buf, bytes);
}

static void _zero(uint8_t *p, uint32_t bytes){
    for(uint32_t i = 0; i < bytes; i++){
        p[i] = 0;
    }
}

/* lo que sabemos de un fd: si detras hay un fichero se lo preguntamos al
   sistema de ficheros, y si es la consola o una tuberia nos lo inventamos,
   que no hay a quien preguntarle. */
static int _fd_stat(int fd_num, stat_t *st) {
    fd_struct *fd = scheduler_current_task_fd(fd_num);
    if(fd == NULL){
        return -EBADF;
    }
    _zero((uint8_t *)st, sizeof(stat_t));
    st->st_nlink = 1;

    uint32_t *file = fd_file(fd);
    if(file == NULL){ //consola o tuberia
        st->st_mode = (fd_is_tty(fd) ? S_IFCHR : S_IFIFO) | 0620;
        st->st_blksize = 1024;
        //dispositivo aparte del disco (que es el 0) para que este inodo
        //inventado no pueda coincidir con el de ningun fichero: cat compara
        //dev+ino de la entrada y la salida para no leerse a si mismo
        st->st_dev = 1;
        st->st_ino = fd_is_tty(fd) ? 1 : (uint32_t)fd;
        return 0;
    }
    if(fs_driver_instance.stat(file, st) < 0){
        return -EIO;
    }
    return 0;
}

/* fstat64(fd, struct stat64 *): la biblioteca de C la usa para saber a que
   esta hablando cada descriptor. cat, por ejemplo, la llama sobre la salida
   estandar nada mas arrancar para elegir el tamano de buffer, y se rinde si
   falla. Primero intenta statx, la 383, y al recibir -ENOSYS reintenta aqui. */
int _syscall_fstat64(int fd_num, stat64_t *st) {
    if(st == NULL){
        return -EFAULT;
    }
    stat_t s;
    int err = _fd_stat(fd_num, &s);
    if(err < 0){
        return err;
    }
    _zero((uint8_t *)st, sizeof(stat64_t));
    st->st_dev     = s.st_dev;
    st->st_ino     = s.st_ino;
    st->__st_ino   = s.st_ino;
    st->st_mode    = s.st_mode;
    st->st_nlink   = s.st_nlink;
    st->st_size    = s.st_size;
    st->st_blksize = s.st_blksize;
    st->st_blocks  = (s.st_size + 511) / 512; //en bloques de 512, no en los del sistema de ficheros
    return 0;
}

/* statx(dirfd, camino, flags, mask, struct statx *): el stat moderno de
   Linux. Es el primero que intenta la biblioteca de C en 32 bits, porque el
   fstat64 de siempre trae los tiempos en 32 bits y no le valen para su
   time_t de 64; solo cae en fstat64 cuando statx contesta -ENOSYS.

   Lo que contestamos es lo mismo que contesta fstat64, ni mas ni menos: el
   sistema de ficheros no guarda fechas, asi que los cuatro tiempos van a
   cero y sus bits quedan fuera de stx_mask, que es justo la manera que tiene
   statx de decir "este campo no lo se". Quien llama mira stx_mask antes de
   creerse un campo, asi que esto es honesto y no un cero disfrazado de fecha.

   El mask que nos piden se ignora: siempre rellenamos lo mismo. Linux
   tambien puede devolver de mas, lo que no puede es devolver de menos sin
   avisarlo en stx_mask. */
int _syscall_statx(int dirfd, char *path, int flags, uint32_t mask, statx_t *stx) {
    (void)mask;
    if(stx == NULL){
        return -EFAULT;
    }

    stat_t s;
    int err;
    if(path == NULL || path[0] == '\0'){ //camino vacio: la pregunta es sobre dirfd, como un fstat
        if(path != NULL && !(flags & AT_EMPTY_PATH)){
            return -ENOENT; //sin AT_EMPTY_PATH, "" no nombra a ningun fichero
        }
        err = _fd_stat(dirfd, &s);
    } else {
        //dirfd se ignora: no hay directorio de trabajo todavia, asi que un
        //camino relativo se resuelve desde la raiz, igual que en open
        err = scheduler_stat(path, &s) < 0 ? -ENOENT : 0;
    }
    if(err < 0){
        return err;
    }

    _zero((uint8_t *)stx, sizeof(statx_t));
    stx->stx_mask    = STATX_TYPE | STATX_MODE | STATX_NLINK | STATX_UID |
                       STATX_GID | STATX_INO | STATX_SIZE | STATX_BLOCKS;
    stx->stx_blksize = s.st_blksize;
    stx->stx_nlink   = s.st_nlink;
    stx->stx_mode    = s.st_mode;
    stx->stx_ino     = s.st_ino;
    stx->stx_size    = s.st_size;
    stx->stx_blocks  = (s.st_size + 511) / 512; //en bloques de 512, no en los del sistema de ficheros
    stx->stx_dev_minor = s.st_dev; //un solo disco: el mayor siempre es cero
    return 0;
}

/* fcntl64(fd, cmd, arg): solo lo justo para que la biblioteca de C pueda
   preguntar por un descriptor sin llevarse un error. No guardamos flags por
   descriptor, asi que F_GETFL contesta lectura y escritura. */
int _syscall_fcntl64(int fd_num, int cmd, int arg) {
    (void)arg;
    fd_struct *fd = scheduler_current_task_fd(fd_num);
    if(fd == NULL){
        return -EBADF;
    }
    switch(cmd){
        case F_GETFD: //no hay exec que cierre descriptores, asi que nunca hay FD_CLOEXEC
        case F_SETFD:
        case F_SETFL:
            return 0;
        case F_GETFL:
            return O_RDWR;
        default:
            return -EINVAL;
    }
}

/* set_tid_address: Linux apunta ahi el id del hilo para despertar a quien
   espere cuando muera. Con un solo hilo por tarea no hay nada que despertar,
   pero hay que devolver el id igual porque es lo que hace la de verdad. */
int _syscall_set_tid_address(int *tidptr) {
    (void)tidptr;
    return scheduler_current_task()->pid;
}

/* writev(fd, iov, iovcnt): escribe los iovcnt trozos en orden y devuelve el
   total de bytes escritos. Si el primer trozo ya falla se devuelve el error;
   si falla despues de haber escrito algo se devuelve lo escrito, que es lo
   que espera quien llama para poder reintentar el resto. */
int _syscall_writev(int fd_num, iovec *iov, int iovcnt) {
    fd_struct *fd = scheduler_current_task_fd(fd_num);
    if(fd == NULL){
        return -EBADF;
    }
    if(iov == NULL || iovcnt < 0){
        return -EINVAL;
    }
    int total = 0;
    for(int i = 0; i < iovcnt; i++){
        if(iov[i].iov_len == 0){
            continue;
        }
        int written = fd->write(fd, (char *)iov[i].iov_base, iov[i].iov_len);
        if(written < 0){
            return total > 0 ? total : -1;
        }
        total += written;
        if((uint32_t)written < iov[i].iov_len){ //escritura parcial: no seguimos con el resto
            break;
        }
    }
    return total;
}

int _syscall_read(int fd_num, char *buf, int bytes) {
    fd_struct *fd = scheduler_current_task_fd(fd_num);
    if(fd == NULL){
        return -EBADF;
    }
    return fd->read(fd, buf, bytes);
}

int _syscall_open(char *path, int flags) {
    return scheduler_open_file(path, flags);
}

int _syscall_close(int fd_num) {
    return scheduler_close_fd(fd_num);
}

int _syscall_dup2(int oldfd, int newfd) {
    return scheduler_dup2(oldfd, newfd);
}

int _syscall_seek(int fd_num, long offset, int whence) {
    return scheduler_seek_file(fd_num, offset, whence);
}

int _syscall_brk(int size) {
    return scheduler_malloc(size);
}

void _syscall_exit(int status) {
    scheduler_exit_task(status);
}

uint16_t _syscall_fork() {
    return scheduler_fork();
}

int16_t _syscall_waitpid(int pid, int status, int options) {
    return scheduler_wait(pid, (int *)status, options);
}

int16_t _syscall_wait(int status) {
    return scheduler_wait(-1, (int *)status, WAIT_OPTION_WHANG);
}

int16_t _syscall_execve(char *file_path, char *argv[]){
    return scheduler_execve(file_path, argv);
}

int _syscall_stat(char *file_path, stat_t *st){
    return scheduler_stat(file_path, st);
}

long _syscall_getdents64(int fd, dirent *buffer, int size){
    return scheduler_getdents64(fd, buffer, size);
}

/* set_thread_area(struct user_desc *u): apunta el segmento de %gs al bloque
   de control de hilo que pide el usuario. Solo se usa base_addr; el limite y
   los flags se ignoran porque el descriptor que arma el kernel es siempre
   plano de 4GB, igual que el resto de segmentos de usuario. */
int _syscall_set_thread_area(user_desc *u) {
    if(u == NULL){
        return -1;
    }
    /* -1 es "elige tu"; cualquier otro valor solo vale si es el unico
       descriptor de hilo que tenemos */
    if(u->entry_number != 0xFFFFFFFFu && u->entry_number != GDT_TLS_ENTRY){
        return -1;
    }
    u->entry_number = scheduler_set_thread_area(u->base_addr);
    return 0;
}

/* fadvise64_64(fd, offset, len, advice): quien llama avisa de como piensa
   leer el fichero (de principio a fin, a saltos, esto ya no lo voy a releer)
   para que el kernel ajuste cuanto adelanta en su cache de paginas. Es un
   consejo, no una orden: Linux mismo lo ignora para los ficheros que no
   puede adelantar, y contestar que si sin hacer nada es una respuesta
   valida. Aqui no hay cache de paginas que ajustar, asi que comprobamos lo
   unico que se puede comprobar, que el descriptor y el consejo existan.

   En i386 los dos long long ocupan dos argumentos cada uno, asi que el
   consejo viaja en el sexto (ebp) y no en el cuarto. */
int _syscall_fadvise64_64(int fd_num, int advice) {
    if(scheduler_current_task_fd(fd_num) == NULL){
        return -EBADF;
    }
    if(advice < POSIX_FADV_NORMAL || advice > POSIX_FADV_NOREUSE){
        return -EINVAL;
    }
    return 0;
}

int _syscall_ioctl(int i, unsigned long request, ...){
    //TODO: Implementar
    return 1;
}

int _syscall_mmap2(void *addr, int length, int prot, int flags, int fd, int pgoffset){
    return scheduler_mmap2(addr, length, prot, flags, fd, pgoffset);
}

int _syscall_munmap(void *addr, int length){
    //TODO: Implementar. De momento no se libera nada: las paginas siguen mapeadas hasta que muere la tarea
    (void)addr;
    (void)length;
    return 0;
}

int _syscall_mprotect(void *addr, int length, int prot){
    //TODO: Implementar. Los permisos de las paginas ya mapeadas se quedan como estan
    (void)addr;
    (void)length;
    (void)prot;
    return 0;
}

int syscall_handler(int num, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6) {
    //screen_print("SYSCALL: ");
    //screen_print_int(num);
    //screen_putchar('\n');
    switch (num) {
        case SYS_WRITE: {
            return _syscall_write(arg1, (char *)arg2, arg3);
        }
        case SYS_READ: {
            return _syscall_read(arg1, (char *)arg2, arg3);
        }
        case SYS_OPEN: {
            return _syscall_open((char *)arg1, arg2);
        }
        case SYS_CLOSE: {
            return _syscall_close(arg1);
        }
        case SYS_DUP2: {
            return _syscall_dup2(arg1, arg2);
        }
        case SYS_SEEK: {
            return _syscall_seek(arg1, arg2, arg3);
        }
        case SYS_BRK: {
            return _syscall_brk(arg1);
        }
        case SYS_EXIT: {
            _syscall_exit(arg1);
            return 0;
        }
        case SYS_FORK: {
            return _syscall_fork();
        }
        case SYS_WAIT: {
            return _syscall_wait(arg1);
        }
        case SYS_WAITPID: {
            return _syscall_waitpid(arg1, arg2, arg3);
        }
        case SYS_EXECVE: {
            return _syscall_execve((char *)arg1, (char **)arg2);
        }
        case SYS_PIPE: {
            return _syscall_pipe((int *)arg1);
        }
        case SYS_STAT: {
            return _syscall_stat((char *)arg1, (stat_t *)arg2);
        }
        case SYS_GETDENTS64: {
            return _syscall_getdents64(arg1, (dirent *)arg2, arg3);
        }
        case SYS_FSTAT64: {
            return _syscall_fstat64(arg1, (stat64_t *)arg2);
        }
        case SYS_FADVISE64_64: { //arg2/arg3 son el offset y arg4/arg5 el tamano, partidos en dos mitades cada uno
            return _syscall_fadvise64_64(arg1, arg6);
        }
        case SYS_STATX: {
            return _syscall_statx(arg1, (char *)arg2, arg3, (uint32_t)arg4, (statx_t *)arg5);
        }
        case SYS_FCNTL64: {
            return _syscall_fcntl64(arg1, arg2, arg3);
        }
        case SYS_EXIT_GROUP: { //un solo hilo por tarea: terminar el grupo es terminar la tarea
            _syscall_exit(arg1);
            return 0;
        }
        case SYS_SET_TID_ADDRESS: {
            return _syscall_set_tid_address((int *)arg1);
        }
        case SYS_WRITEV: {
            return _syscall_writev(arg1, (iovec *)arg2, arg3);
        }
        case SYS_SET_THREAD_AREA: {
            return _syscall_set_thread_area((user_desc *)arg1);
        }
        case SYS_IOCTL: {
            return _syscall_ioctl(arg1, (unsigned long)arg2, (char *)arg3);
        }
        case SYS_MMAP2: {
            return _syscall_mmap2((void *)arg1, arg2, arg3, arg4, arg5, arg6);
        }
        case SYS_MPROTECT: {
            return _syscall_mprotect((void *)arg1, arg2, arg3);
        }
        case SYS_MUNMAP: {
            return _syscall_munmap((void *)arg1, arg2);
        }   
        default:
            screen_print("SYSCALL number NOT implemented: ");
            screen_print_int(num);
            screen_putchar('\n');
            return -ENOSYS;
    }
}
