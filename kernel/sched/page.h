#ifndef PAGE_H
#define PAGE_H

#include <stdint.h>
#include "../lib/queue.h"
#include "../mm/paging.h"
#include "task.h"

/* helpers de page_t compartidos por el scheduler y el cargador ELF.
   flags son los bits de permiso de la PTE (PAGE_FLAGS_USER_RW /
   PAGE_FLAGS_USER_RO, ver mm/paging.h) */
page_t *_new_page_t(uint32_t phys, uint32_t virt, uint32_t flags);

void _new_page_t_and_enqueue(queue_t *q, uint32_t phys, uint32_t virt, uint32_t flags);

page_t *_get_page(node_queue_t *n);

#endif
