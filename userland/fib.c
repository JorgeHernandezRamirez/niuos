#include "stdlib.h"

/* fibonacci recursivo con memoizacion: sin la tabla, fib(n) hace ~2*fib(n+1)
   llamadas (fib(48) ~ 15.000 millones). Con ella cada fib(k) se calcula una
   sola vez. La profundidad maxima sigue siendo n. Uso: fib.bin [n], por defecto n=20 */

#define FIB_MAX 47 /* fib(48) ya no cabe en 32 bits */

static unsigned int memo[FIB_MAX + 1]; /* .bss: 0 = no calculado todavia */

unsigned int fib(unsigned int n) {
    if (n < 2)
        return n;
    if (memo[n] == 0)
        memo[n] = fib(n - 1) + fib(n - 2);
    return memo[n];
}

static void print_dec(unsigned int val);

static int streq(char *a, char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

/* recursion sin caso base: cada nivel baja el stack ~96 bytes (buf + frame).
   Al pasar STACK_VIRTUAL_LAST el acceso cae en la guard page y el kernel
   deberia matar la tarea con "Segmentation fault". buf es volatile para que
   gcc no lo elimine y tocarlo en cada nivel garantiza que el fallo se produce
   pagina a pagina, sin saltarse la guard page. */
static unsigned int overflow(unsigned int depth) {
    volatile char buf[64];
    buf[0] = (char) depth;
    if (depth % 16 == 0) {
        printf("depth ");
        print_dec(depth);
        printf("\n");
    }
    return overflow(depth + 1) + buf[0];
}

static unsigned int atoi(char *s) {
    unsigned int n = 0;
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        s++;
    }
    return n;
}

static void print_dec(unsigned int val) {
    char buf[11]; /* 10 digitos maximo en 32 bits + '\0' */
    int i = 10;
    buf[10] = '\0';
    do {
        buf[--i] = '0' + (val % 10);
        val /= 10;
    } while (val);
    printf(&buf[i]);
}

int main(int argc, char *argv[]) {
    unsigned int n = 20;
    if (argc >= 2 && streq(argv[1], "overflow")) {
        overflow(0);
        printf("no deberia llegar aqui\n");
        exit(0);
    }
    if (argc >= 2)
        n = atoi(argv[1]);
    if (n > FIB_MAX) {
        printf("n demasiado grande (max 47)\n");
        exit(0);
    }
    printf("fib(");
    print_dec(n);
    printf(") = ");
    print_dec(fib(n));
    printf("\n");
    exit(0);
    return 0;
}
