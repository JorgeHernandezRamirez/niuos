#ifndef LIB_H
#define LIB_H

#include <stdint.h>

#define NULL ((void*)0)

/* Firma estandar (dest, src, n) a proposito: GCC genera llamadas a memcpy por
   su cuenta al copiar structs grandes (p.ej. el `*buffer = *entry` de
   scheduler_getdents64) y siempre usa este orden. Cuando aqui los argumentos
   estaban al reves, esas copias implicitas iban en sentido contrario sin que
   nada lo avisara: getdents64 devolvia dirents en blanco y ls no imprimia
   nada. No cambiar el orden. */
void *memcpy(void *dest, const void *src, uint32_t n);

int strcmp(char *s1, char *s2);

uint32_t strlen(char *buf);

char *char_last_pos(char *buf);

uint8_t is_null(void *a);

uint32_t min(uint32_t a, uint32_t b);

uint8_t is_root(char *filename);

#endif