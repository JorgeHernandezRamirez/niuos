/* keyboard.c - traduce scancodes (set 1) del PS/2 a ASCII y los imprime en pantalla */

#include "../arch/x86/io.h"
#include "screen.h"
#include "../fd/tty.h"

/* Layout ES (teclado español ISO). Los scancodes son POSICIONES fisicas, no
   caracteres: QEMU manda la posicion de la tecla, y es esta tabla la que decide
   que letra sale. Por eso antes, con una tabla US, la tecla "/" (que en español
   es Shift+7) llegaba como el scancode del 7 -> imprimia un '7'.

   Las entradas a 0 son teclas sin equivalente ASCII (ñ, acentos, º/ª, ç, ¡/¿, ·)
   o que necesitan AltGr (@, #, [, ], {, }, \, |), que todavia no manejamos. */
static const char scancode_to_ascii[128] = {
    [0x01] = 27,   /* Esc */
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5',
    [0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0A] = '9', [0x0B] = '0',
    [0x0C] = '\'', /* [0x0D] = '¡' no ASCII */
    [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't',
    [0x15] = 'y', [0x16] = 'u', [0x17] = 'i', [0x18] = 'o', [0x19] = 'p',
    [0x1A] = '`', [0x1B] = '+', [0x1C] = '\n',
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f', [0x22] = 'g',
    [0x23] = 'h', [0x24] = 'j', [0x25] = 'k', [0x26] = 'l',
    /* [0x27] = 'ñ', [0x28] = '´', [0x29] = 'º', [0x2B] = 'ç' no ASCII */
    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v',
    [0x30] = 'b', [0x31] = 'n', [0x32] = 'm', [0x33] = ',', [0x34] = '.',
    [0x35] = '-', [0x37] = '*', [0x39] = ' ',
    [0x56] = '<', /* tecla extra del ISO, a la izquierda de la Z */
};

/* Misma tabla con Shift pulsado. Aca esta el '/' que buscabamos: Shift + la
   tecla del 7 (scancode 0x08). */
static const char scancode_to_ascii_shift[128] = {
    [0x01] = 27,   /* Esc */
    [0x02] = '!', [0x03] = '"', /* [0x04] = '·' no ASCII */
    [0x05] = '$', [0x06] = '%',
    [0x07] = '&', [0x08] = '/', [0x09] = '(', [0x0A] = ')', [0x0B] = '=',
    [0x0C] = '?', /* [0x0D] = '¿' no ASCII */
    [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R', [0x14] = 'T',
    [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I', [0x18] = 'O', [0x19] = 'P',
    [0x1A] = '^', [0x1B] = '*', [0x1C] = '\n',
    [0x1E] = 'A', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F', [0x22] = 'G',
    [0x23] = 'H', [0x24] = 'J', [0x25] = 'K', [0x26] = 'L',
    /* [0x27] = 'Ñ', [0x28] = '¨', [0x29] = 'ª', [0x2B] = 'Ç' no ASCII */
    [0x2C] = 'Z', [0x2D] = 'X', [0x2E] = 'C', [0x2F] = 'V',
    [0x30] = 'B', [0x31] = 'N', [0x32] = 'M', [0x33] = ';', [0x34] = ':',
    [0x35] = '_', [0x37] = '*', [0x39] = ' ',
    [0x56] = '>',
};

/* Tercera capa: AltGr (Alt derecho). Es lo que en el layout ES da los simbolos
   de programacion, empezando por el '|' que necesitan los pipes del shell. */
static const char scancode_to_ascii_altgr[128] = {
    [0x02] = '|', [0x03] = '@', [0x04] = '#', [0x05] = '~',
    [0x1A] = '[', [0x1B] = ']',
    [0x28] = '{', [0x29] = '\\', [0x2B] = '}',
};

/* Shift es una tecla con estado: hay que recordar si esta pulsada entre dos
   interrupciones distintas (una para el Shift, otra para la letra). */
static uint8_t shift_pressed = 0;
static uint8_t altgr_pressed = 0;
static uint8_t e0_prefix = 0;

void keyboard_handler(void) {
    unsigned char scancode = inb(0x60);

    /* 0xE0 no es una tecla: es el prefijo de las teclas "extendidas" (AltGr,
       flechas, '/' del teclado numerico). Llega en su propia interrupcion, asi
       que hay que recordarlo para la siguiente. */
    if (scancode == 0xE0) {
        e0_prefix = 1;
        outb(0x20, 0x20);
        return;
    }
    uint8_t prefixed = e0_prefix;
    e0_prefix = 0;

    /* Los modificadores se miran ANTES del filtro de break codes: su make code
       activa el flag y su break code (make | 0x80) lo apaga, asi que aca si nos
       interesa un scancode >= 0x80. El resto de break codes se siguen ignorando.
       Aceptamos 0x38 con y sin prefijo 0xE0, es decir tanto el Alt izquierdo
       como el AltGr: en un teclado de Mac los dos Option son igual de comodos. */
    if (scancode == 0x38) {
        altgr_pressed = 1;
    }
    else if (scancode == 0xB8) {
        altgr_pressed = 0;
    }
    else if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
    }
    else if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = 0;
    }
    /* bit 7 = 0 -> tecla pulsada (make code); bit 7 = 1 -> tecla soltada (break code, se ignora) */
    else if (!prefixed && scancode < 0x80) {
        char c;
        if (altgr_pressed) {
            c = scancode_to_ascii_altgr[scancode];
        }
        else if (shift_pressed) {
            c = scancode_to_ascii_shift[scancode];
        }
        else {
            c = scancode_to_ascii[scancode];
        }
        if (c != 0) {
            if(tty_task_is_waiting_stdin()){
                tty_put_char(c);
                screen_putchar(c);
                if(c == '\n'){
                    tty_awake_process();
                }
            }

        }
    }

    outb(0x20, 0x20); /* EOI (End Of Interrupt) al PIC maestro */
}
