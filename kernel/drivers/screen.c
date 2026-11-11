/* screen.c - driver de pantalla VGA modo texto: escritura, backspace, scroll y cursor hardware */

#include <stddef.h>
#include "../arch/x86/io.h"
#include "screen.h"

#define VGA_MEMORY ((volatile unsigned char *) 0xB8000)
#define VGA_COLS 80
#define VGA_ROWS 25
#define VGA_COLOR 0x0F

static size_t cursor_row = 0;
static size_t cursor_col = 0;

static void screen_update_cursor(void) {
    unsigned short pos = (unsigned short)(cursor_row * VGA_COLS + cursor_col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char)((pos >> 8) & 0xFF));
}

static void screen_scroll(void) {
    for (size_t row = 1; row < VGA_ROWS; row++) {
        for (size_t col = 0; col < VGA_COLS; col++) {
            size_t dst = ((row - 1) * VGA_COLS + col) * 2;
            size_t src = (row * VGA_COLS + col) * 2;
            VGA_MEMORY[dst] = VGA_MEMORY[src];
            VGA_MEMORY[dst + 1] = VGA_MEMORY[src + 1];
        }
    }
    for (size_t col = 0; col < VGA_COLS; col++) {
        size_t off = ((VGA_ROWS - 1) * VGA_COLS + col) * 2;
        VGA_MEMORY[off] = ' ';
        VGA_MEMORY[off + 1] = VGA_COLOR;
    }
    cursor_row = VGA_ROWS - 1;
}

void screen_init(void) {
    for (size_t i = 0; i < VGA_COLS * VGA_ROWS; i++) {
        VGA_MEMORY[i * 2] = ' ';
        VGA_MEMORY[i * 2 + 1] = VGA_COLOR;
    }
    cursor_row = 0;
    cursor_col = 0;
    screen_update_cursor();
}

void screen_putchar(char c) {
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
        } else if (cursor_row > 0) {
            cursor_row--;
            cursor_col = VGA_COLS - 1;
        }
        size_t off = (cursor_row * VGA_COLS + cursor_col) * 2;
        VGA_MEMORY[off] = ' ';
        VGA_MEMORY[off + 1] = VGA_COLOR;
    } else {
        size_t off = (cursor_row * VGA_COLS + cursor_col) * 2;
        VGA_MEMORY[off] = c;
        VGA_MEMORY[off + 1] = VGA_COLOR;
        cursor_col++;
    }

    if (cursor_col >= VGA_COLS) {
        cursor_col = 0;
        cursor_row++;
    }
    if (cursor_row >= VGA_ROWS) {
        screen_scroll();
    }
    screen_update_cursor();
}

int screen_print(const char *s) {
    int i = 0;
    while(s[i] != '\0'){
        screen_putchar(s[i++]);
    }
    return i;
}

/* para buffers que no terminan en '\0', como oem_name/volume_label en FAT16 */
void screen_print_buf(const uint8_t *buf, int len) {
    for (int i = 0; i < len; i++) {
        screen_putchar((char)buf[i]);
    }
}

void screen_print_hex32(uint32_t v) {
    static const char digits[] = "0123456789ABCDEF";
    screen_putchar('0');
    screen_putchar('x');
    for (int shift = 28; shift >= 0; shift -= 4) {
        screen_putchar(digits[(v >> shift) & 0xF]);
    }
}

void screen_print_int(uint32_t v) {
    static const char digits[] = "0123456789";
    char buf[10]; /* 4294967295 -> 10 digitos */
    int i = 0;
    if (v == 0) {
        screen_putchar('0');
        return;
    }
    while (v > 0) {
        buf[i++] = digits[v % 10];
        v /= 10;
    }
    while (i > 0) {
        screen_putchar(buf[--i]);
    }
}
