#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include "arch/arch.h"

#define SYS_EXIT 1

#define SYS_FORK 2   /* mismo numero que sys_fork en Linux x86 de 32-bit, por familiaridad */

#define SYS_READ 3   /* mismo numero que sys_read en Linux x86 de 32-bit, por familiaridad */

#define SYS_WRITE 4   /* mismo numero que sys_write en Linux x86 de 32-bit, por familiaridad */

#define SYS_OPEN 5   /* mismo numero que sys_open en Linux x86 de 32-bit, por familiaridad */

#define SYS_CLOSE 6   /* mismo numero que sys_close en Linux x86 de 32-bit, por familiaridad */

#define SYS_SEEK 19   /* mismo numero que sys_lseek en Linux x86 de 32-bit, por familiaridad */

#define SYS_DUP2 63   /* mismo numero que sys_dup2 en Linux x86 de 32-bit, por familiaridad */

#define SYS_BRK 45 //malloc

#define SYS_WAITPID 7

#define SYS_WAIT 114

#define SYS_EXECVE 11

#define SYS_PIPE 42   /* mismo numero que sys_pipe en Linux x86 de 32-bit, por familiaridad */

#ifdef ARCH_x86_32
#define SYS_IOCTL 54   /* mismo numero que sys_pipe en Linux x86 de 32-bit, por familiaridad */
#define SYS_MMAP2 192   /* mismo numero que sys_pipe en Linux x86 de 32-bit, por familiaridad */
#define SYS_MPROTECT 125   /* mismo numero que sys_mprotect en Linux x86 de 32-bit, por familiaridad */
#define SYS_MUNMAP 91   /* mismo numero que sys_munmap en Linux x86 de 32-bit, por familiaridad */
#else
#error Not defined SYS_IOCTL number
#endif

#define SYS_STAT 106   /* mismo numero que sys_stat en Linux x86 de 32-bit, por familiaridad */

#define SYS_WRITEV 146   /* mismo numero que sys_writev en Linux x86 de 32-bit */

#define SYS_GETDENTS64 220

#define SYS_FSTAT64 197   /* mismo numero que sys_fstat64 en Linux x86 de 32-bit */

#define SYS_FCNTL64 221   /* mismo numero que sys_fcntl64 en Linux x86 de 32-bit */

#define SYS_SET_THREAD_AREA 243   /* mismo numero que sys_set_thread_area en Linux x86 de 32-bit */

#define SYS_EXIT_GROUP 252   /* mismo numero que sys_exit_group en Linux x86 de 32-bit */

#define SYS_SET_TID_ADDRESS 258   /* mismo numero que sys_set_tid_address en Linux x86 de 32-bit */

#define SYS_FADVISE64_64 272   /* mismo numero que sys_fadvise64_64 en Linux x86 de 32-bit */

#define SYS_STATX 383   /* mismo numero que sys_statx en Linux x86 de 32-bit */

/* consejos de posix_fadvise: como piensa leer el fichero quien llama */
#define POSIX_FADV_NORMAL     0
#define POSIX_FADV_RANDOM     1
#define POSIX_FADV_SEQUENTIAL 2
#define POSIX_FADV_WILLNEED   3
#define POSIX_FADV_DONTNEED   4
#define POSIX_FADV_NOREUSE    5

/* banderas de las llamadas *at: dirfd y camino en vez de un camino a secas */
#define AT_FDCWD            -100   /* el camino es relativo al directorio de trabajo */
#define AT_EMPTY_PATH       0x1000 /* camino vacio: la pregunta es sobre dirfd, como un fstat */

/* que campos de struct statx pide quien llama, y cuales devolvemos rellenos.
   STATX_BASIC_STATS es lo que pide la biblioteca de C: todo lo que traia el
   viejo struct stat. */
#define STATX_TYPE          0x001
#define STATX_MODE          0x002
#define STATX_NLINK         0x004
#define STATX_UID           0x008
#define STATX_GID           0x010
#define STATX_ATIME         0x020
#define STATX_MTIME         0x040
#define STATX_CTIME         0x080
#define STATX_INO           0x100
#define STATX_SIZE          0x200
#define STATX_BLOCKS        0x400
#define STATX_BASIC_STATS   0x7FF
#define STATX_BTIME         0x800

/* ordenes de fcntl */
#define F_DUPFD 0
#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 4

/* struct stat64 tal y como la define Linux para i386: es la que rellena
   fstat64 y la que la biblioteca de C espera byte a byte. Los huecos y el
   orden raro de los campos son parte del formato, no se pueden reordenar.
   En i386 un long long se alinea a 4, asi que no hay relleno implicito y la
   estructura mide exactamente 96 bytes. */
typedef struct {
    uint64_t st_dev;
    uint8_t  __pad0[4];
    uint32_t __st_ino;
    uint32_t st_mode;
    uint32_t st_nlink;
    uint32_t st_uid;
    uint32_t st_gid;
    uint64_t st_rdev;
    uint8_t  __pad3[4];
    int64_t  st_size;
    uint32_t st_blksize;
    uint64_t st_blocks;
    uint32_t st_atime_;
    uint32_t st_atime_nsec;
    uint32_t st_mtime_;
    uint32_t st_mtime_nsec;
    uint32_t st_ctime_;
    uint32_t st_ctime_nsec;
    uint64_t st_ino;
} stat64_t;

_Static_assert(sizeof(stat64_t) == 96, "struct stat64 de i386 mide 96 bytes");

/* struct statx_timestamp / struct statx tal y como las define Linux. A
   diferencia de struct stat64 esta no depende de la arquitectura: mismos
   offsets y mismo tamano en 32 y en 64 bits, y con segundos de 64 bits. Por
   eso la biblioteca de C la prefiere y solo cae en fstat64 si statx no
   existe: en 32 bits el fstat64 de siempre trae los tiempos en 32 bits y no
   le sirven para su time_t. */
typedef struct {
    int64_t  tv_sec;
    uint32_t tv_nsec;
    int32_t  __reserved;
} statx_timestamp;

typedef struct {
    uint32_t stx_mask;       /* que campos de los de abajo van rellenos de verdad */
    uint32_t stx_blksize;
    uint64_t stx_attributes;
    uint32_t stx_nlink;
    uint32_t stx_uid;
    uint32_t stx_gid;
    uint16_t stx_mode;
    uint16_t __spare0[1];
    uint64_t stx_ino;
    uint64_t stx_size;
    uint64_t stx_blocks;
    uint64_t stx_attributes_mask;
    statx_timestamp stx_atime;
    statx_timestamp stx_btime;
    statx_timestamp stx_ctime;
    statx_timestamp stx_mtime;
    uint32_t stx_rdev_major; /* el dispositivo lo parte en mayor y menor, no lo trae empaquetado */
    uint32_t stx_rdev_minor;
    uint32_t stx_dev_major;
    uint32_t stx_dev_minor;
    uint64_t stx_mnt_id;
    uint32_t stx_dio_mem_align;
    uint32_t stx_dio_offset_align;
    uint64_t __spare3[12];   /* sitio reservado para lo que Linux agregue mas adelante */
} statx_t;

_Static_assert(sizeof(statx_t) == 256, "struct statx mide 256 bytes");

/* un trozo de los que escribe writev. La biblioteca estandar de C vacia sus
   flujos con una lista de estos en vez de con un write por trozo: cuando ya
   tiene datos en su buffer interno y encima le llega un bloque nuevo, los
   manda juntos en una sola llamada. */
typedef struct {
    void     *iov_base;
    uint32_t  iov_len;
} iovec;

/* i386 no tiene registro de hilo, asi que el almacenamiento local vive en un
   segmento aparte al que apunta %gs. Esta es la estructura que Linux recibe
   en set_thread_area y la que arma glibc en TLS_INIT_TP: entry_number a -1
   significa "elige tu el descriptor y dime cual", y la respuesta se escribe
   en ese mismo campo. */
typedef struct {
    uint32_t entry_number;
    uint32_t base_addr;
    uint32_t limit;
    uint32_t flags; /* seg_32bit:1, contents:2, read_exec_only:1, limit_in_pages:1, seg_not_present:1, useable:1 */
} user_desc;

/* dispatcher: eax=numero de syscall, ebx=arg1, ecx=arg2, edx=arg3. Devuelve el valor para eax. */
int syscall_handler(int num, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6);

#endif
