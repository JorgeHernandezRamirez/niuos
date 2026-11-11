#ifndef ERRNO_H
#define ERRNO_H

/* Convenio de Linux para int 0x80: el valor de retorno ES el error, en
   negativo. Cero o positivo es exito; de -1 a -4095 es un fallo y la
   biblioteca de C lo copia a errno cambiado de signo. Devolver -1 a secas
   significa EPERM, "operacion no permitida", que es lo que salia antes por
   pantalla para todo. */

#define EPERM   1   /* operacion no permitida */
#define ENOENT  2   /* no existe el fichero */
#define EIO     5   /* error de entrada/salida */
#define EBADF   9   /* descriptor invalido */
#define EFAULT  14  /* puntero que no se puede tocar */
#define EINVAL  22  /* argumento invalido */
#define ENOSYS  38  /* llamada no implementada */

#endif
