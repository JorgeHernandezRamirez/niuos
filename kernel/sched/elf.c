#include "elf.h"
#include "../fs/fs.h"
#include "../lib/lib.h"
#include "../drivers/screen.h"
#include "../mm/pmm.h"
#include "task.h"
#include "page.h"
#include "../mm/paging.h"

uint8_t _has_elf_header_ident(elf_header *header){
    return header->e_ident[0] == 0x7f && \
     (char)header->e_ident[1] == 'E' && \
     (char)header->e_ident[2] == 'L' && \
     (char)header->e_ident[3] == 'F' && \
     header->e_ident[4] == 1    && \
     header->e_ident[5] == 1;
}

uint16_t _get_machine_version(void){
    #ifdef ARCH_x86_32
    return ELF_HEADER_MACHINE_386;
    #elif defined(ARCH_x86_64)
    return ELF_MACHINE_X86_64;
    #else
    #error "arquitectura no soportada por el cargador ELF"
    #endif
}

uint8_t elf_is_elf(uint32_t *file){
    elf_header header;
    fs_driver_instance.seek(file, 0, SEEK_SET);
    uint16_t bytes_readed = fs_driver_instance.read(file, sizeof(elf_header), (uint32_t *)&header);
    if(bytes_readed < sizeof(elf_header)){
        return 0;
    }
    return _has_elf_header_ident(&header) && \
        (header.e_type == ELF_HEADER_TYPE_EXEC) && \
        header.e_machine == _get_machine_version() && \
        header.e_version == ELF_HEADER_MACHINE_SUPPORTED_VERSION; 
}

elf_header *elf_get_header(uint32_t *file){
    fs_driver_instance.seek(file, 0, SEEK_SET);
    elf_header *header = (elf_header *)vmalloc(sizeof(elf_header));
    uint16_t bytes_readed = fs_driver_instance.read(file, sizeof(elf_header), (uint32_t *)header);
    if(bytes_readed < sizeof(elf_header)){
        return NULL;
    }
    return header;
}

void elf_print_program_header(elf_header *header, uint32_t *file){
    fs_driver_instance.seek(file, header->e_phoff, SEEK_SET);

    //e_phentsize -> Tamaño de cada program header
    //e_phnum -> numero de program header
    screen_print("\nNumero de program header: ");
    screen_print_int(header->e_phnum);
    for(int i=0; i < header->e_phnum; i++){
        elf_program_header program_header;
        fs_driver_instance.read(file, sizeof(elf_program_header),  (uint32_t *)&program_header);
        screen_print("\nTYPE: ");
        screen_print_int(program_header.p_type);
        screen_print("\nVIRTUAL ADDRESS: ");
        screen_print_hex32(program_header.p_vaddr);
        screen_print("\nSIZE MEM: "); //Cuantas paginas de 4KB necesitamos
        screen_print_int(program_header.p_memsz);
        screen_print("\nFLAGS: "); //Cuantas paginas de 4KB necesitamos
        screen_print_int(program_header.p_flags);

    }
}

page_t *_elf_page_for_virt(task_t *t, uint32_t virt_page){
    for(node_queue_t *n = t->code_pages->front; n != 0; n = n->next){
        page_t *p = _get_page(n);
        if(p->virt == virt_page){
            return p;
        }
    }
    return NULL;
}

uint32_t _elf_page_flags(uint32_t p_flags){
    uint32_t flags = PAGE_PRESENT | PAGE_USER;
    if(p_flags & ELF_PROGRAM_FLAG_W){
        flags |= PAGE_RW;
    }
    return flags;
}

uint32_t _elf_get_or_alloc_frame(task_t *t, uint32_t virt_page, uint32_t flags){
    page_t *p = _elf_page_for_virt(t, virt_page);
    if(p != NULL){
        p->flags |= flags;
        return p->phys;
    }
    uint32_t phys = pmm_alloc_frame(); //viene a cero: el .bss no necesita limpieza extra
    if(phys == 0){
        return 0;
    }
    _new_page_t_and_enqueue(t->code_pages, phys, virt_page, flags);
    return phys;
}

uint32_t _elf_do_load_bin_to_mem_segment(uint32_t* file, elf_program_header *program_header, task_t *t){
    if(program_header->p_type != ELF_PROGRAM_TYPE_LOAD || program_header->p_memsz == 0){
        return 0;
    }

    uint32_t segment_end = program_header->p_vaddr + program_header->p_memsz;
    uint32_t bytes_left = program_header->p_filesz;
    uint32_t page_flags = _elf_page_flags(program_header->p_flags);
    fs_driver_instance.seek(file, program_header->p_offset, SEEK_SET);

    for(uint32_t virt = ALIGN_DOWN_PAGE(program_header->p_vaddr); virt < segment_end; virt += PAGE_SIZE){
        uint32_t phys = _elf_get_or_alloc_frame(t, virt, page_flags); //TODO Mirar mñn. Crea que siempre hay que hacer pmm_alloc_frame()
        if(phys == 0){ //sin memoria
            return 0;
        }
        /* solo la primera pagina puede arrancar a media pagina: p_vaddr no
           tiene por que estar alineado */
        uint32_t offset_in_page = virt < program_header->p_vaddr ? program_header->p_vaddr - virt : 0;
        uint32_t to_read = PAGE_SIZE - offset_in_page;
        if(to_read > bytes_left){
            to_read = bytes_left;
        }
        if(to_read > 0){
            bytes_left -= fs_driver_instance.read(file, to_read, (uint32_t *)(phys + offset_in_page));
        }
    }
    return ALIGN_UP_PAGE(segment_end);
}

uint32_t elf_do_load_bin_to_mem(uint32_t* file, task_t *t){
    elf_header *header = elf_get_header(file);
    if(header == NULL){
        return 0;
    }
    if(header->e_type != ELF_HEADER_TYPE_EXEC){
        return 0;
    }
    t->virtual_address_entry = header->e_entry;
    t->virtual_address_phdr = 0;
    t->phdr_count = header->e_phnum;
    t->phdr_size = header->e_phentsize;

    uint32_t phoff = header->e_phoff;
    uint16_t phnum = header->e_phnum;
    uint16_t phentsize = header->e_phentsize;
    uint32_t free_virtual_address = 0;

    for(uint16_t i = 0; i < phnum; i++){
        elf_program_header program_header;
        /* re-posicionarse en cada vuelta: cargar un segmento deja el cursor
           del fichero dentro del segmento, no en el phdr siguiente */
        fs_driver_instance.seek(file, phoff + i * phentsize, SEEK_SET);
        if(fs_driver_instance.read(file, sizeof(elf_program_header), (uint32_t *)&program_header) < sizeof(elf_program_header)){
            return 0;
        }
        if(program_header.p_type == ELF_PROGRAM_TYPE_LOAD &&
           program_header.p_offset <= phoff &&
           phoff + (uint32_t)phnum * phentsize <= program_header.p_offset + program_header.p_filesz){
            t->virtual_address_phdr = program_header.p_vaddr + (phoff - program_header.p_offset);
        }
        uint32_t segment_end = _elf_do_load_bin_to_mem_segment(file, &program_header, t);
        if(segment_end > free_virtual_address){
            free_virtual_address = segment_end;
        }
    }
    return free_virtual_address;
}
