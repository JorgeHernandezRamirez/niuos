#include "page.h"
#include "../mm/pmm.h"

page_t *_new_page_t(uint32_t phys, uint32_t virt, uint32_t flags){
    page_t *p = (page_t *)vmalloc(sizeof(page_t));
    p->phys = phys;
    p->virt = virt; 
    p->flags = flags;
    return p;
}

void _new_page_t_and_enqueue(queue_t *q, uint32_t phys, uint32_t virt, uint32_t flags){
    page_t *p = _new_page_t(phys, virt, flags);
    enqueue(q, (void *)p);
}

page_t *_get_page(node_queue_t *n){
    return (page_t *) n->data;
}