#ifndef ELF_H
#define ELF_H

#include <stdint.h>
#include "../arch/arch.h"
#include "task.h"

#define ELF_HEADER_TYPE_EXEC 0x0002
#define ELF_HEADER_TYPE_DYN  0x0003 //PIE: reconocible, pero el cargador no lo sabe reubicar
#define ELF_HEADER_MACHINE_386 0x0003
#define ELF_HEADER_MACHINE_X86_64  0x003E
#define ELF_HEADER_MACHINE_SUPPORTED_VERSION 0x00000001

#define ELF_PROGRAM_TYPE_NULL    0
#define ELF_PROGRAM_TYPE_LOAD    1
#define ELF_PROGRAM_TYPE_DYNAMIC 2
#define ELF_PROGRAM_TYPE_INTERP  3
#define ELF_PROGRAM_TYPE_NOTE    4
#define ELF_PROGRAM_TYPE_SHLIB   5
#define ELF_PROGRAM_TYPE_PHDR    6

/* p_flags de un program header */
#define ELF_PROGRAM_FLAG_X 0x1
#define ELF_PROGRAM_FLAG_W 0x2
#define ELF_PROGRAM_FLAG_R 0x4

#ifdef ARCH_x86_32
typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} elf_header;


typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} elf_program_header;
#else
#error "Architecture not supported by ELF loader"
#endif

uint8_t elf_is_elf(uint32_t *file);
elf_header *elf_get_header(uint32_t *file);
void elf_print_program_header(elf_header *header, uint32_t *file);

/* mapea los PT_LOAD del ELF en t->code_pages y devuelve la primera
   direccion virtual libre detras de la imagen, o 0 si no se pudo cargar */
uint32_t elf_do_load_bin_to_mem(uint32_t *file, task_t *t);

#endif