#include "lib.h"

void *memcpy(void *dest, const void *src, uint32_t n){
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (uint32_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

uint32_t strlen(char *buf){
    int i = 0;
    while(buf[i] != '\0' && buf[i] != NULL){
        i++;
    }
    return i;
}

uint8_t is_null(void *a){
    return a == NULL;
}

char *char_last_pos(char *buf){
    int i = 0;
    while(buf[i] != '\0'){
        i++;
    }
    return &buf[i-1];
}

int strcmp(char *s1, char *s2){
    int i = 0;
    while(s1[i] != '\0' && s1[i] == s2[i]){
        i++;
    }
    return (unsigned char)s1[i] - (unsigned char)s2[i];
}

uint32_t min(uint32_t a, uint32_t b){
    return a < b ? a : b;
}

uint8_t is_root(char *filename){
    return strcmp(filename, "/") == 0;
}
