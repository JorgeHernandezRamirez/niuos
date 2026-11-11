#ifndef SCREEN_H
#define SCREEN_H

#include <stdint.h>

void screen_init(void);
void screen_putchar(char c);
int screen_print(const char *s);
void screen_print_buf(const uint8_t *buf, int len);
void screen_print_hex32(uint32_t v);
void screen_print_int(uint32_t v);

#endif
