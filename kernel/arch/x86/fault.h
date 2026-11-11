#ifndef FAULT_H
#define FAULT_H

#include <stdint.h>

void gp_fault_handler(uint32_t error_code) __attribute__((noreturn));

void page_fault_handler(uint32_t error_code, uint32_t cr2);

#endif
