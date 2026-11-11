/* scheduler.c - round-robin preventivo entre tareas de ring3.
   Todas las tareas se registran igual con scheduler_add_task(), incluida la
   primera: no hay un camino especial para "la que arranca kmain" vs "las
   que arrancan por cambio de contexto". Arrancar el scheduler (context_start,
   en usermode.asm) y conmutar de tarea (el tail de irq0 en isr.asm) hacen
   literalmente lo mismo: cargar un esp que apunta a un trap_frame_t y hacer
   popa+iret. La unica diferencia es que context_start ademas recarga
   DS/ES/FS/GS al selector de usuario, porque es la unica vez que esos
   registros todavia tienen el valor de kernel (ver usermode.asm). */

#include "scheduler.h"
#include "page.h"
#include "elf.h"
#include "../arch/x86/gdt.h"
#include "../arch/x86/usermode.h"
#include "../mm/paging.h"
#include "../arch/x86/io.h"
#include "../fs/fs.h"
#include "../mm/pmm.h"
#include "../drivers/screen.h" //TODO: Quitar
#include "../lib/lib.h"
#include "../fd/fd.h"
#include "../errno.h"

/* Layout exacto de lo que hay en la pila de kernel de una tarea que esta
   "fuera de la CPU": primero los 8 registros que pusha empuja (en su mismo
   orden, EAX se empuja primero -> queda en la direccion mas alta, EDI se
   empuja al final -> direccion mas baja / tope de pila), y encima el frame
   que un iret con cambio de privilegio consume (EIP, CS, EFLAGS, ESP, SS,
   en ese orden de direccion baja a alta, que es el orden en que los apilo
   la CPU: SS primero/direccion mas alta, EIP al final/tope de pila). */
typedef struct {
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    uint32_t eip, cs, eflags, esp, ss;
} trap_frame_t;

static task_t tasks[MAX_TASKS];
static int task_count = 0;
static int current = 0;

static uint32_t kstack_top(task_t *t) {
    return (uint32_t)(t->kstack + TASK_KSTACK_WORDS);
}

uint32_t _build_cr3(task_t *t);
void _setup_user_stack(task_t *t, char *argv[]);

uint8_t _is_alive(task_t *t){
    return t->status == STATUS_TASK_RUNNING ||
           t->status == STATUS_TASK_WAITING;
}

uint32_t _index_new_task(void){
    for(int i = 0; i < MAX_TASKS; i++){
        if(!_is_alive(&tasks[i])){
            return i;
        }
    }
    return -1;
}

int _do_index_next_task(void){
    int i = (current + 1) % MAX_TASKS;
    while(i != current && tasks[i].status != STATUS_TASK_RUNNING){
        i = (i + 1) % MAX_TASKS;
    }
    return tasks[i].status == STATUS_TASK_RUNNING ? i : -1;
}

int32_t _index_next_task(void){
    static uint32_t idle_stack[512] __attribute__((aligned(16)));
    static int32_t  idle_owner = -1;   /* task whose context we fell asleep on */
    static uint32_t idle_owner_esp;    /* its real esp, before it gets clobbered */

    uint8_t was_idling = 0;
    if(idle_owner != -1){
        tasks[idle_owner].esp = idle_owner_esp;
        idle_owner = -1;
        was_idling = 1;
    }

    int next = _do_index_next_task();
    if(next != -1){
        current = next;
        return current;
    }
    idle_owner = current;
    idle_owner_esp = tasks[current].esp;

    __asm__ volatile (
        "mov %0, %%esp\n\t"   /* own stack: drops the stranded frame */
        "1: sti\n\t"
        "   hlt\n\t"
        "   jmp 1b\n\t"
        :: "r" (&idle_stack[512])
        : "memory");
    __builtin_unreachable();
}

task_t *_new_task_t_from_pages_and_parent(queue_t *code_pages, queue_t *stack_pages, queue_t *heap_pages, queue_t *mmaps_pages, queue_t *mmaps, queue_t *childs, int16_t pid_to_use, task_t *parent){
    //task_t *t = (task_t *)vmalloc(sizeof(task_t)); //TODO: Deberíamos ir por aqui
    uint32_t index = pid_to_use != NO_PID ? pid_to_use : _index_new_task();
    task_t *t = &tasks[index];
    t->pid = index;
    t->code_pages = code_pages;
    t->stack_pages = stack_pages;
    t->heap_pages = heap_pages;
    t->mmap_pages = mmaps_pages;
    t->mmaps = mmaps;
    t->exited_childs = childs;
    t->status = STATUS_TASK_RUNNING;
    t->tls_base = 0; //sin almacenamiento local de hilo hasta que lo pida set_thread_area
    t->parent = (struct task_t *)parent;
    return t;
}

task_t *_new_task_t_with_parent(int16_t pid_to_use, task_t *parent){
    return _new_task_t_from_pages_and_parent(new_queue(), new_queue(), new_queue(), new_queue(),  new_queue(), new_queue(), pid_to_use, parent);
}

task_t *_new_task_t(int16_t pid_to_use){
    return _new_task_t_from_pages_and_parent(new_queue(), new_queue(), new_queue(), new_queue(),  new_queue(), new_queue(), pid_to_use, NULL);
}

uint32_t _do_load_bin_to_mem_data(uint32_t* ff, task_t *t){
    int bytes_readed;
    uint32_t virtual_address = USER_VIRT_BASE;
    t->virtual_address_entry = USER_VIRT_BASE; //un binario plano arranca en su primer byte
    t->virtual_address_phdr = 0; //un binario plano no tiene cabeceras de programa (tasks[] se reutiliza: hay que limpiarlo)
    t->phdr_count = 0;
    t->phdr_size = 0;
    fs_driver_instance.seek(ff, 0, SEEK_SET); //elf_is_elf() dejo el cursor detras de la cabecera
    do{
        uint32_t pd_phys = pmm_alloc_frame();
        bytes_readed = fs_driver_instance.read(ff, PAGE_SIZE, (uint32_t *)pd_phys);
        _new_page_t_and_enqueue(t->code_pages, pd_phys, virtual_address, PAGE_FLAGS_USER_RW);
        virtual_address += PAGE_SIZE;
    } while(bytes_readed == PAGE_SIZE);
    return virtual_address;
}

uint32_t _do_load_bin_to_mem(uint32_t* ff, task_t *t){
    if(elf_is_elf(ff)){
        return elf_do_load_bin_to_mem(ff, t);
    }
    return _do_load_bin_to_mem_data(ff, t);
}

void _do_allocate_pages_for_heap_and_stack(task_t *t, uint32_t last_virtual_address){
    _new_page_t_and_enqueue(t->heap_pages, pmm_alloc_frame(), last_virtual_address, PAGE_FLAGS_USER_RW);
    _new_page_t_and_enqueue(t->stack_pages, pmm_alloc_frame(), STACK_VIRTUAL_INIT, PAGE_FLAGS_USER_RW);
}

void _do_rest_configurtion_new_task(task_t *t){
    t->cr3 = _build_cr3(t);
    trap_frame_t *tf = (trap_frame_t *)(kstack_top(t) - sizeof(trap_frame_t));
    tf->edi = 0; tf->esi = 0; tf->ebp = 0; tf->esp_dummy = 0;
    tf->ebx = 0; tf->edx = 0; tf->ecx = 0; tf->eax = 0;
    tf->eip = t->virtual_address_entry; //lo fija el cargador: e_entry del ELF o USER_VIRT_BASE
    tf->cs = GDT_USER_CODE;
    tf->eflags = 0x202; /* IF=1 (bit 9) + bit 1 reservado, siempre en 1 */
    tf->esp = _get_page(t->stack_pages->front)->virt; //Set virtual Stack
    tf->ss = GDT_USER_DATA;
    t->esp = (uint32_t) tf;
    _setup_user_stack(t, NULL); //sin argumentos, pero con el formato que espera crt0.asm
    t->heap = _get_page(t->heap_pages->front)->virt; //Set virtual Heap
    t->mmap = MMAP_VIRTUAL_INIT;
}

void _do_init_fds(task_t *t){
    fd_stdin_init(&t->fds[0]);
    fd_stdout_init(&t->fds[1]);
    fd_stderr_init(&t->fds[2]);
}

uint8_t _do_new_task_from_file(uint32_t* ff, int16_t pid_to_use){
    if(task_count >= MAX_TASKS){
        screen_print("Max tasks reached\n");
        return 0;
    }
    task_t *t = _new_task_t(pid_to_use);
    uint32_t last_virtual_address = _do_load_bin_to_mem(ff, t); //Load binary file to mem
    if(last_virtual_address == 0){ //ELF invalido o sin memoria
        screen_print("Could not load binary\n");
        return 0;
    }
    _do_allocate_pages_for_heap_and_stack(t, last_virtual_address); //We allocate 1 page for heap and 1 page for stack. This will be improve in the future. The stack and heap should be allocated dinamically through page faults.
    _do_rest_configurtion_new_task(t);
    _do_init_fds(t);
    return 1;
}

uint8_t _do_scheduler_new_task(char *filename, int16_t pid_to_use){
    uint32_t* ff = fs_driver_instance.open(filename, O_RDONLY);
    if(ff == NULL){
        screen_print("File does not exists or is not a file\n");
        return 0;
    }
    return _do_new_task_from_file(ff, pid_to_use);
}

uint8_t scheduler_new_task(char *filename){
    return _do_scheduler_new_task(filename, NO_PID);
}

#define PTE_COUNT    1024u

void _map_page_in_cr3(uint32_t cr3_phys, uint32_t phys_page, uint32_t virt_page, uint32_t flags){
    uint32_t *pd = (uint32_t *) cr3_phys;
    uint32_t pde_index = virt_page >> 22;
    uint32_t pte_index = (virt_page >> 12) & 0x3FFu;

    uint32_t *kernel_pd = (uint32_t *) paging_get_kernel_pd_phys();
    if (kernel_pd[pde_index] & PAGE_PRESENT) { //esa PDE es del kernel, compartida por puntero
        screen_print("Cannot map user page over kernel space: ");
        screen_print_hex32(virt_page);
        screen_print("\n");
        return;
    }

    uint32_t *pt;
    if (pd[pde_index] & PAGE_PRESENT) {
        pt = (uint32_t *) (pd[pde_index] & 0xFFFFF000u);
    } else {
        uint32_t pt_phys = pmm_alloc_frame();
        pt = (uint32_t *) pt_phys;
        pd[pde_index] = pt_phys | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    }

    pt[pte_index] = phys_page | flags;
}

void _map_page_queue_in_pd(uint32_t *pd, uint32_t *kernel_pd, queue_t *pages){
    for (node_queue_t *n = pages->front; n != 0; n = n->next) {
        page_t *p = _get_page(n);
        _map_page_in_cr3((uint32_t)pd, p->phys, p->virt, p->flags);
    }
}

/* arma un page directory nuevo a partir de las queue_t de page_t de la
   tarea: comparte las PDEs del kernel (mismo puntero fisico que armo
   paging_init(), igual que paging_new_user_address_space) y mapea codigo,
   heap y stack cada uno en la PDE que le corresponda por su virtual.
   Devuelve el CR3 (fisico) de ese page directory. */
//TODO: Mover a paging.c quizas
uint32_t _build_cr3(task_t *t){
    uint32_t pd_phys = pmm_alloc_frame();
    uint32_t *pd = (uint32_t *) pd_phys;
    uint32_t kernel_pd_phys = paging_get_kernel_pd_phys();
    uint32_t *kernel_pd = (uint32_t *) kernel_pd_phys;

    for (uint32_t i = 0; i < PTE_COUNT; i++) {
        pd[i] = kernel_pd[i];
    }

    _map_page_queue_in_pd(pd, kernel_pd, t->code_pages);
    _map_page_queue_in_pd(pd, kernel_pd, t->heap_pages);
    _map_page_queue_in_pd(pd, kernel_pd, t->stack_pages);

    return pd_phys;
}

static void _load_cr3(uint32_t cr3) {
    __asm__ volatile ("mov %0, %%cr3" :: "r" (cr3) : "memory");
}

void _resume_current_task(){
    tss_set_esp0(kstack_top(&tasks[current]));
    gdt_set_tls_base(tasks[current].tls_base);
    _load_cr3(tasks[current].cr3);
    context_start(tasks[current].esp);
}

void scheduler_start(void) {
    current = 0;
    _resume_current_task();
}

uint32_t scheduler_next_task(uint32_t old_esp) {
    outb(0x20, 0x20); /* EOI al PIC maestro: liberarlo antes de conmutar */

    tasks[current].esp = old_esp;
    _index_next_task();

    tss_set_esp0(kstack_top(&tasks[current]));
    gdt_set_tls_base(tasks[current].tls_base); /* el selector es comun a todas; la base, no */
    _load_cr3(tasks[current].cr3); /* mov cr3 flushea el TLB completo: no hace falta invlpg manual */
    return tasks[current].esp;
}

uint32_t scheduler_malloc(uint32_t size) {
    task_t *t = &tasks[current];
    uint32_t old = t->heap;
    t->heap += size;
    return old;
}

uint8_t _cr2_in_heap_range(task_t *t, uint32_t cr2){
    uint32_t low = _get_page(t->heap_pages->front)->virt;
    uint32_t high = t->heap;
    return cr2 >= low && cr2 < high;
}

mmap_t *_get_cr2_in_mmap_range(task_t *t, uint32_t cr2){
    if(t->mmaps == NULL){
        screen_print("ES NULL");
        for(;;){}
        return NULL;
    }
    node_queue_t *current = (node_queue_t *)t->mmaps->front;
    while(current != NULL){
        mmap_t *node = (mmap_t *)current->data;
        if(cr2 >= node->virt_init && cr2 < node->virt_last){
            return node;
        }
        current = current->next;
    }
    return NULL;
}

uint8_t _cr2_in_mmap_range(task_t *t, uint32_t cr2){
    return _get_cr2_in_mmap_range(t, cr2) != NULL;
}

uint8_t _cr2_in_stack_overflow(task_t *t, uint32_t cr2){
    return cr2 < STACK_VIRTUAL_LAST && cr2 >= (STACK_VIRTUAL_LAST - PAGE_SIZE);
}

uint8_t _cr2_in_stack_range(task_t *t, uint32_t cr2){
    return cr2 >= STACK_VIRTUAL_LAST && cr2 <= STACK_VIRTUAL_INIT;
}

void _page_fault_heap(task_t *t, uint32_t cr2, uint32_t virt_page){
    uint32_t phys_page = pmm_alloc_frame();
    _new_page_t_and_enqueue(t->heap_pages, phys_page, virt_page, PAGE_FLAGS_USER_RW);
    _map_page_in_cr3(t->cr3, phys_page, virt_page, PAGE_FLAGS_USER_RW);
}

void _page_fault_stack(task_t *t, uint32_t cr2, uint32_t virt_page){
    uint32_t phys_page = pmm_alloc_frame();
    _new_page_t_and_enqueue(t->stack_pages, phys_page,  virt_page, PAGE_FLAGS_USER_RW);
    _map_page_in_cr3(t->cr3, phys_page, virt_page, PAGE_FLAGS_USER_RW);
}

uint32_t _mmap_prot_to_page_flags(int prot){
    //TODO: respetar prot cuando mprotect() actualice los flags de las paginas ya mapeadas
    (void)prot;
    return PAGE_FLAGS_USER_RW;
}

void _page_fault_mmap_fill_page_fd(mmap_t *node, uint32_t virt_page, uint32_t phys_page){
    uint32_t file_offset = node->pgoffset * PAGE_SIZE + (virt_page - node->virt_init);
    uint32_t *file = ((fd_file_struct *)scheduler_current_task_fd(node->fd))->file;
    fs_driver_instance.seek(file, file_offset, SEEK_SET);
    fs_driver_instance.read(file, PAGE_SIZE, (uint32_t *)phys_page);
}

void _page_fault_mmap(task_t *t, uint32_t cr2, uint32_t virt_page){
    mmap_t *node = _get_cr2_in_mmap_range(t, cr2);
    uint32_t phys_page = pmm_alloc_frame();
    uint32_t page_flags = _mmap_prot_to_page_flags(node->prot);
    _new_page_t_and_enqueue(t->mmap_pages, phys_page, virt_page, page_flags);
    _map_page_in_cr3(t->cr3, phys_page, virt_page, page_flags);
    if(node->fd != -1){
        _page_fault_mmap_fill_page_fd(node, virt_page, phys_page);
    }
}

uint8_t scheduler_page_not_found(uint32_t cr2){
    task_t *t = &tasks[current];
    uint32_t virt_page = cr2 & 0xFFFFF000u;
    //screen_print("Fallo mmap: ");
    //screen_print_hex32(cr2);
    //screen_print("\n");
    if (_cr2_in_heap_range(t, cr2)) {
        _page_fault_heap(t, cr2, virt_page);
        return 1;
    }
    if(_cr2_in_stack_range(t, cr2)){
        _page_fault_stack(t, cr2, virt_page);
        return 1;
    }
    if(_cr2_in_stack_overflow(t, cr2)){
        screen_print("Segmentation fault (core dumped) \n");
        scheduler_exit_task(STATUS_TASK_FINISHED_ERROR);
        return 1;
    }
    if(_cr2_in_mmap_range(t, cr2)){
        _page_fault_mmap(t, cr2, virt_page);
        return 1;
    }
    return 0;
}

void _free_page_queue(queue_t *q){
    page_t *p;
    while((p = (page_t *) dequeue(q)) != 0){
        pmm_free_frame(p->phys);
    }
}

void _free_task_address_space_unless_cr3(task_t *t){
    uint32_t *pd = (uint32_t *) t->cr3;
    uint32_t *kernel_pd = (uint32_t *) paging_get_kernel_pd_phys();
    for(uint32_t i = 0; i < PTE_COUNT; i++){
        if(kernel_pd[i] & PAGE_PRESENT){
            continue;
        }
        if(pd[i] & PAGE_PRESENT){
            pmm_free_frame(pd[i] & 0xFFFFF000u);
        }
    }
    _free_page_queue(t->code_pages);
    _free_page_queue(t->stack_pages);
    _free_page_queue(t->heap_pages);
    _free_page_queue(t->mmap_pages);
    //TODO: free vmalloc
}

void _free_task_address_space(task_t *t){
    _free_task_address_space_unless_cr3(t);
    pmm_free_frame(t->cr3);
}

trap_frame_t *_trap_frame_t(task_t *t){
    return (trap_frame_t *)(kstack_top(t) - sizeof(trap_frame_t));
}

void _eax_register_put(task_t *t, uint32_t value){
    _trap_frame_t(t)->eax = value;
}

void _esp_register_put(task_t *t, uint32_t value){
     _trap_frame_t(t)->esp = value;
}

uint32_t _esp_register_get(task_t *t){
    return _trap_frame_t(t)->esp;
}

uint8_t _parent_is_waiting_for_current_task(){
    task_t *parent = tasks[current].parent;
    return parent != NULL && parent->status == STATUS_TASK_WAITING && parent->wait != NULL &&
       (parent->wait->pid == -1 || parent->wait->pid == tasks[current].pid);
}


son_t *_find_exited_child(task_t *task, int16_t pid){ //pid == -1 -> cualquier hijo terminado
    node_queue_t *aux = task->exited_childs->front;
    while(aux != NULL){
        son_t *son = (son_t *)aux->data;
        if(pid == -1 || son->pid == pid){
            return son;
        }
        aux = aux->next;
    }
    return NULL;
}

int8_t _is_pid_in_childs(task_t *task, int16_t pid){
    return _find_exited_child(task, pid) != NULL;
}

void _remove_pid_in_childs(task_t *task, int16_t pid){
    queue_t *childs = task->exited_childs;
    if(pid == -1){
        dequeue(childs);
        return;
    }
    node_queue_t *aux = childs->front;
    while(aux != NULL){
        son_t *son = (son_t *)aux->data;
        if(son->pid == pid){
            queue_remove(childs, aux);
            return;
        }
        aux = aux->next;
    }
}

/**
 * Función que debe setear el valor pid del hijo en el registro eax para devolverlo. y luego debemos entrar en el estacio de usuarios cargando 
 * la cr3 y el puntero de status asignar el valor de status
 */
void _see_if_parent_is_waiting(){
    task_t *parent = tasks[current].parent;
    if(_parent_is_waiting_for_current_task()){
        _remove_pid_in_childs(parent, tasks[current].pid);
        _eax_register_put(parent, tasks[current].pid); //Lo que devolvemos
        _load_cr3(parent->cr3); //Super importante debemos cargar cr3 para asignar el valor de status
        *parent->wait->status = tasks[current].status;
        parent->status = STATUS_TASK_RUNNING;
        parent->wait = NULL; //ya esta entregada: el padre deja de estar esperando
        //vfree(parent->wait);
    }
}

void _close_fds(task_t *t){
    for(int i = 0; i < MAX_FDS; i++){
        if(t->fds[i] != NULL){
            t->fds[i]->close(t->fds[i]);
            t->fds[i] = NULL;
        }
    }
}

void _do_scheduler_exit_task(int status){
    task_t *t = &tasks[current];
    t->status = status;
    _close_fds(t); //suelta los extremos del pipe que tuviera abiertos, para que el otro lado vea EOF
    _free_task_address_space(t);
}

void _queue_new_son_to_parent(int status){
    son_t *son = (son_t *)vmalloc(sizeof(son_t));
    son->pid = tasks[current].pid;
    son->status = status;
    enqueue(tasks[current].parent->exited_childs, son);
}

void _notify_to_parent_exit_task(int status){
    if(tasks[current].parent != NULL){
        _queue_new_son_to_parent(status);
        _see_if_parent_is_waiting();
    }
}

void scheduler_exit_task(int status){
    _do_scheduler_exit_task(status);
    _notify_to_parent_exit_task(status);
    _index_next_task();
    _resume_current_task();
}

void* _do_copy_page_t(page_t *page, uint32_t pd_phys){
    page_t *copy = (page_t *)vmalloc(sizeof(page_t));
    copy->phys = pd_phys;
    copy->virt = page->virt;
    copy->flags = page->flags;
    return copy;
}

queue_t * _copy_pages(queue_t  *pages){
    queue_t *copy = (queue_t *)vmalloc(sizeof(queue_t));
    node_queue_t *current = pages->front;
    while(current != NULL){
        uint32_t new_pd_phys = pmm_alloc_frame();
        uint32_t pd_phys = ((page_t *)current->data)->phys;
        memcpy((uint8_t *)new_pd_phys, (uint8_t *)pd_phys, PAGE_SIZE);
        enqueue(copy, _do_copy_page_t((page_t *)current->data, new_pd_phys));
        current = current->next;
    }
    return copy;
}

mmap_t *_copy_mmap(mmap_t *mmap){
    mmap_t *copy = (mmap_t *)vmalloc(sizeof(mmap_t));
    copy->virt_init = mmap->virt_init;
    copy->virt_last = mmap->virt_last;
    copy->prot = mmap->prot;
    copy->flags = mmap->flags;
    copy->fd = mmap->fd;
    copy->pgoffset = mmap->pgoffset;
    return copy;
}

queue_t *_copy_mmaps(queue_t *mmaps){
    queue_t *copy = new_queue();
    node_queue_t *current = mmaps->front;
    while(current != NULL){
        enqueue(copy, _copy_mmap((mmap_t *)current->data));
        current = current->next;
    }
    return copy;
}

void *_copy_fds(task_t *task_src, task_t *task_dst){
    for(int i = 0; i < MAX_FDS; i++){
        fd_copy(&task_dst->fds[i], &task_src->fds[i]); //cada fd se duplica a su manera (p.ej. el pipe sube num_readers/num_writers)
    }
}

task_t *_new_task_fork_t(task_t *current_task){
    return _new_task_t_from_pages_and_parent(
        _copy_pages(current_task->code_pages),
        _copy_pages(current_task->stack_pages),
        _copy_pages(current_task->heap_pages),
        _copy_pages(current_task->mmap_pages),
        _copy_mmaps(current_task->mmaps),
        new_queue(),
        NO_PID,
        current_task
    );
}

void _do_rest_configurtion_fork(task_t *t, task_t *current){
    t->cr3 = _build_cr3(t);
    trap_frame_t *parent_tf = (trap_frame_t *)(kstack_top(current) - sizeof(trap_frame_t));
    trap_frame_t *tf = (trap_frame_t *)(kstack_top(t) - sizeof(trap_frame_t));
    *tf = *parent_tf;
    t->esp = (uint32_t) tf;
    t->heap = current->heap;
    t->mmap = current->mmap;
    t->tls_base = current->tls_base; //mismo espacio de direcciones: el TCB del padre sigue valiendo
    _copy_fds(current, t);
}

uint16_t scheduler_fork(){
    if(task_count >= MAX_TASKS){
        screen_print("Max tasks reached\n");
        _resume_current_task();
        return 0;
    }
    task_t *task_forked = _new_task_fork_t(&tasks[current]);
     _do_rest_configurtion_fork(task_forked, &tasks[current]);
     //Update eax register to task which makes the fork with the child 
     _eax_register_put(&tasks[current], task_forked->pid);
     //Update eax register to cero to the child task
     _eax_register_put(task_forked, 0);
     return task_forked->pid;
}

task_t *_find_by_pid(uint16_t pid){
    //Can be optimized! Actually pid its simply the index on the array
    int i = 0;
    while(i < MAX_TASKS && tasks[i].pid != pid){
        i++;
    }
    return i < MAX_TASKS ? &tasks[i] : NULL;
}

int16_t _do_wait_nohang(uint16_t pid){
    task_t *child = _find_by_pid(pid);
    if(child == NULL || child->parent != &tasks[current]){
        return -1; //Si no se encuentra se devuelve -1
    }
    return _is_alive(child) ? 0 : child->pid; //By definition. If its alive we return 0, otherwise the pid
}

int16_t _do_wait_hang(int16_t pid, int *status){
    if(pid != -1){
        task_t *child = _find_by_pid(pid);
        if(child == NULL || child->parent != &tasks[current]){
            return -1; //Si no se encuentra se devuelve -1
        }
    }
    son_t *son = _find_exited_child(&tasks[current], pid);
    if(son != NULL){ //el hijo ya habia terminado, no hay que dormirse
        int16_t son_pid = son->pid;
        *status = son->status;
        _remove_pid_in_childs(&tasks[current], son_pid);
        return son_pid;
    }
    tasks[current].status = STATUS_TASK_WAITING;
    tasks[current].wait = (wait_t *)vmalloc(sizeof(wait_t));
    tasks[current].wait->pid = pid;
    tasks[current].wait->status = status;
    tasks[current].esp = (uint32_t) _trap_frame_t(&tasks[current]);
    _index_next_task();
    _resume_current_task();
}

int16_t scheduler_wait(int16_t pid, int *status, int options){
    if(options == WAIT_OPTION_WNOHANG){
        return _do_wait_nohang(pid);
    }
    return _do_wait_hang(pid, status);
}

uint8_t _size_argv(char *argv[]){
    uint8_t argc = 0;
    while(argv[argc] != NULL){
        argc++;
    }
    return argc;
}

char **_copy_argv(char *file_path, char *argv[]){
    uint8_t argc = _size_argv(argv);
    //+1 para file_path (va en argv[0]), +1 para el NULL final
    char **copy = (char **)vmalloc((argc + 2) * sizeof(char *));

    uint32_t path_len = strlen(file_path);
    char *path_copy = (char *)vmalloc(path_len + 1);
    memcpy(path_copy, file_path, path_len + 1);
    copy[0] = path_copy;

    for(uint8_t i = 0; i < argc; i++){
        uint32_t len = strlen(argv[i]);
        char *s = (char *)vmalloc(len + 1);
        memcpy(s, argv[i], len + 1);
        copy[i + 1] = s;
    }
    copy[argc + 1] = NULL;
    return copy;
}

/* tipos del vector auxiliar (los de Linux/System V para i386) */
#define AT_NULL   0
#define AT_PHDR   3
#define AT_PHENT  4
#define AT_PHNUM  5
#define AT_PAGESZ 6
#define AT_ENTRY  9
#define AT_UID    11
#define AT_EUID   12
#define AT_GID    13
#define AT_EGID   14
#define AT_RANDOM 25

/* la pila de la tarea se escribe SIEMPRE a traves del frame fisico: esta
   funcion puede correr con el cr3 de otra tarea cargado (al crear una tarea
   nueva todavia no se ha conmutado), y el fisico es accesible bajo cualquier
   cr3 porque el identity map del kernel lo comparten todos. */
uint8_t *_user_stack_ptr(task_t *t, uint32_t virt){
    page_t *stack = _get_page(t->stack_pages->front);
    return (uint8_t *)(stack->phys + (virt & 0xFFFu));
}

uint32_t _push_string_to_user_stack(task_t *t, uint32_t esp, char *str){
    uint32_t len = strlen(str);
    esp -= len + 1; //+1 para el terminador nulo
    memcpy(_user_stack_ptr(t, esp), str, len + 1);
    return esp;
}

/* arma la pila de arranque que define la ABI de System V para i386, la misma
   que usa Linux. De esp hacia arriba:

       esp+0            argc
       esp+4            argv[0] ... argv[argc-1]   (el array, EN LINEA)
       esp+4+4*argc     NULL
                        envp[0] ... NULL
                        vector auxiliar: pares (tipo, valor) hasta AT_NULL
                        y por encima de todo eso, las cadenas

   crt0.asm lee de ahi argc y argv y se los pasa a main. Un binario de glibc
   lee lo mismo mas el vector auxiliar, del que saca el tamano de pagina, las
   cabeceras de programa y los 16 bytes de AT_RANDOM con los que inicializa
   el canario de pila. Sin ese ultimo desreferencia el nulo y revienta. */
void _setup_user_stack(task_t *t, char *argv[]){
    uint8_t argc = argv == NULL ? 0 : _size_argv(argv);
    uint32_t esp = _get_page(t->stack_pages->front)->virt; //siempre desde el techo: asi execve puede rehacerla
    uint32_t *argv_virt = (uint32_t *)vmalloc((argc + 1) * sizeof(uint32_t));

    //1) las cadenas, arriba del todo
    for(uint8_t i = 0; i < argc; i++){
        esp = _push_string_to_user_stack(t, esp, argv[i]);
        argv_virt[i] = esp;
    }

    //2) los 16 bytes de AT_RANDOM
    esp -= 16;
    uint32_t at_random = esp;
    uint8_t *random = _user_stack_ptr(t, esp);
    for(uint8_t i = 0; i < 16; i++){
        random[i] = (uint8_t)(0x5A ^ (t->pid + i * 31));
    }

    //3) el bloque de abajo. Se cuenta primero para dejar esp alineado a 16
    uint32_t auxv_pairs = t->virtual_address_phdr != 0 ? 11 : 8;
    uint32_t words = 1 + argc + 1 + 1 + auxv_pairs * 2; //argc + argv + NULL + envp NULL + auxv
    esp = (esp - words * sizeof(uint32_t)) & ~0xFu;

    uint32_t *p = (uint32_t *)_user_stack_ptr(t, esp);
    *p++ = argc;
    for(uint8_t i = 0; i < argc; i++){
        *p++ = argv_virt[i];
    }
    *p++ = 0; //fin de argv
    *p++ = 0; //entorno vacio

    if(t->virtual_address_phdr != 0){
        *p++ = AT_PHDR;   *p++ = t->virtual_address_phdr;
        *p++ = AT_PHENT;  *p++ = t->phdr_size;
        *p++ = AT_PHNUM;  *p++ = t->phdr_count;
    }
    *p++ = AT_PAGESZ; *p++ = PAGE_SIZE;
    *p++ = AT_ENTRY;  *p++ = t->virtual_address_entry;
    *p++ = AT_RANDOM; *p++ = at_random;
    *p++ = AT_UID;    *p++ = 0;
    *p++ = AT_EUID;   *p++ = 0;
    *p++ = AT_GID;    *p++ = 0;
    *p++ = AT_EGID;   *p++ = 0;
    *p++ = AT_NULL;   *p++ = 0;

    _esp_register_put(t, esp);
}

void _set_args_current_task(char *argv[]){
    _setup_user_stack(&tasks[current], argv);
}

uint8_t _do_new_task_from_file_execve(uint32_t* ff, int16_t pid_to_use){
    if(task_count >= MAX_TASKS){
        screen_print("Max tasks reached\n");
        return 0;
    }
    task_t *t = _new_task_t_with_parent(pid_to_use, tasks[current].parent);
    uint32_t last_virtual_address = _do_load_bin_to_mem(ff, t); //Load binary file to mem
    _do_allocate_pages_for_heap_and_stack(t, last_virtual_address); //We allocate 1 page for heap and 1 page for stack. This will be improve in the future. The stack and heap should be allocated dinamically through page faults.
    _do_rest_configurtion_new_task(t);
    return 1;
}

uint8_t _do_scheduler_new_task_execve(char *filename, int16_t pid_to_use){
    uint32_t* ff = fs_driver_instance.open(filename, O_RDONLY);
    if(ff == NULL){
        screen_print("command not found: ");
        screen_print(filename);
        screen_print("\n");
        return 0;
    }
    return _do_new_task_from_file_execve(ff, pid_to_use);
}

void _do_scheduler_exit_task_execve(int status){
    task_t *t = &tasks[current];
    t->status = status;
    _free_task_address_space_unless_cr3(t);
}

uint8_t scheduler_execve(char *file_path, char *argv[]){
    char **argv_copy = _copy_argv(file_path, argv);
    uint16_t current_pid = tasks[current].pid;
    uint32_t cr3 = tasks[current].cr3; //Save cr3 to delete then. Otherwise access to memory will broke
    _do_scheduler_exit_task_execve(STATUS_TASK_FINISHED);
    uint8_t status = _do_scheduler_new_task_execve(file_path, current_pid);
    pmm_free_frame(cr3); //Last item is free cr3
    if(!status){ //Si el estado es incorrecto, cogemos siguiente tarea
        _notify_to_parent_exit_task(STATUS_TASK_FINISHED_ERROR);
        _index_next_task();
    }
    else{
        _load_cr3(tasks[current].cr3); //Sin esto, esp se escribe bajo el cr3 VIEJO, no el de la tarea nueva
        _set_args_current_task(argv_copy);
        //TODO:free argv_copy
    }
    _resume_current_task();
}

/* set_thread_area: guarda la base del bloque de control de hilo y la deja
   activa ya, sin esperar al proximo cambio de contexto, porque quien llama
   carga %gs inmediatamente despues. Devuelve el indice de GDT que ha usado
   para que el usuario arme el selector. */
uint32_t scheduler_set_thread_area(uint32_t tls_base){
    tasks[current].tls_base = tls_base;
    gdt_set_tls_base(tls_base);
    return GDT_TLS_ENTRY;
}

fd_struct *scheduler_current_task_fd(int fd_num){
    return tasks[current].fds[fd_num];
}

task_t *scheduler_current_task(){
     return &tasks[current];
}

uint8_t scheduler_awake(int16_t pid){
    if(pid < 0 || pid >= MAX_TASKS){
        return -1;
    }
    if(tasks[pid].status != STATUS_TASK_WAITING_STDIN && tasks[pid].status != STATUS_TASK_WAITING_PIPE){
        return -1;
    }
    tasks[pid].status = STATUS_TASK_RUNNING;
    return 0;
}

int _new_fd(task_t *t, int num, int *fds){ //reserva num descriptores libres y los deja en fds
    int free_fds[MAX_FDS];
    int found = 0;
    if(num <= 0 || num > MAX_FDS){
        return -1;
    }
    for(int i = 3; i < MAX_FDS && found < num; i++){ //0,1,2 reserved for stdin/stdout/stderr
        if(t->fds[i] == NULL){
            free_fds[found++] = i;
        }
    }
    if(found < num){
        return -1; //No quedan suficientes file descriptors libres
    }
    for(int i = 0; i < num; i++){
        fds[i] = free_fds[i]; //Touching userspace :)
    }
    return 0;
}

int scheduler_open_file(char *file_path, int flags){
    char *_file_path = fs_format_file_path(file_path);
    uint32_t *ff = fs_driver_instance.open(_file_path, flags);
    if(ff == NULL){
        return -1; //File does not exists
    }
    task_t *t = scheduler_current_task();
    int fd;
    if(_new_fd(t, 1, &fd) < 0){
        screen_print("Error -> OPEN: No more fds");
        return -1;
    }
    fd_file_init(&t->fds[fd], ff);
    return fd;
}

int scheduler_stat(char *file_path, stat_t *st){
    if(st == NULL){
        return -1;
    }
    char *_file_path = fs_format_file_path(file_path);
    uint32_t *ff = fs_driver_instance.open(_file_path, O_RDONLY); //nunca O_CREAT: stat no crea el fichero
    if(ff == NULL){
        return -1; //File does not exists
    }
    return fs_driver_instance.stat(ff, st);
}

uint8_t scheduler_seek_file(int fd_num, long offset, int whence){
    fd_file_struct *fd = (fd_file_struct *)scheduler_current_task_fd(fd_num);
    if(fd->file == NULL){
        return -1; //fd no es un fichero (stdin/stdout/stderr)
    }
    fs_driver_instance.seek(fd->file, offset, whence);
    return 0;
}

int scheduler_close_fd(int fd){
    if(fd < 0 || fd >= MAX_FDS){
        return -EBADF;
    }
    task_t *t = scheduler_current_task();
    if(t->fds[fd] == NULL){ //ya estaba cerrado
        return -EBADF;
    }
    t->fds[fd]->close(t->fds[fd]);
    t->fds[fd] = NULL;
    return 0;
}

int scheduler_dup2(int oldfd, int newfd){
    if(oldfd < 0 || oldfd >= MAX_FDS || newfd < 0 || newfd >= MAX_FDS){
        return -EBADF;
    }
    if(oldfd == newfd){
        return newfd;
    }
    task_t *t = scheduler_current_task();
    if(t->fds[oldfd] == NULL){ //oldfd no está abierto
        return -EBADF;
    }
    if(t->fds[newfd] != NULL){ //newfd se cierra antes de ser machacado (incluido stdin/stdout/stderr)
        t->fds[newfd]->close(t->fds[newfd]);
    }
    fd_copy(&t->fds[newfd], &t->fds[oldfd]);
    return newfd;
}

int scheduler_new_pipe(int *fds){
    task_t *t = scheduler_current_task();
    if(_new_fd(t, 2, fds) < 0){ //los dos indices de golpe, asi no se repiten entre si
        screen_print("Error -> PIPE: No more fds");
        return -1;
    }
    fd_pipe_init(&t->fds[fds[0]], &t->fds[fds[1]]);
    return 0;
}

int scheduler_getdents64(int fd_num, dirent *buffer, int size){
    if(fd_num < 0 || fd_num >= MAX_FDS || size < (int)sizeof(dirent)){
        return -1;
    }
    fd_file_struct *fd = (fd_file_struct *)scheduler_current_task_fd(fd_num);
    if(fd == NULL || fd->file == NULL || !fs_driver_instance.isdir(fd->file)){
        return -1; /* fd libre, o no es un fichero (stdin/stdout/stderr/pipe) */
    }

    int bytes = 0;
    while(bytes + (int)sizeof(dirent) <= size){
        dirent *entry = fs_driver_instance.readdir(fd->file);
        if(entry == NULL){
            break;
        }
        *buffer = *entry;
        buffer->d_reclen = (unsigned short)sizeof(dirent);
        buffer++;
        bytes += sizeof(dirent);
    }
    return bytes;
}

mmap_t *_new_map_t(uint32_t virt_init, uint32_t virt_last, int prot, int flags, int fd, int pgoffset){
    mmap_t *n = (mmap_t *)vmalloc(sizeof(mmap_t));
    n->fd = fd;
    n->prot = prot;
    n->flags = flags;
    n->pgoffset = pgoffset; 
    n->virt_init = virt_init;
    n->virt_last = virt_last;
    return n;
}

void _new_mmap_t_and_enqueue(queue_t *q, uint32_t virt_init, uint32_t virt_last, int prot, int flags, int fd, int pgoffset){
    mmap_t *p = _new_map_t(virt_init, virt_last, prot,  flags, fd, pgoffset);
    enqueue(q, (void *)p);
}

int _do_scheduler_mmap2_no_fd(int length, int prot, int flags){
    task_t *t = scheduler_current_task();
    uint32_t size = ALIGN_UP_PAGE(length); //mmap reserva paginas enteras: el resto de la ultima tambien es accesible
    t->mmap -= size;
    _new_mmap_t_and_enqueue(t->mmaps, t->mmap, t->mmap + size, prot, flags, -1, 0);
    return t->mmap;
}

int _do_scheduler_mmap2_fd(int length, int prot, int flags, int fd_num, int pgoffset){
    task_t *t = scheduler_current_task();
    if(t->fds[fd_num] == NULL){ //oldfd no está abierto
        return -EBADF;
    }
    uint32_t *file = ((fd_file_struct *)t->fds[fd_num])->file;
    fs_driver_instance.seek(file, pgoffset, SEEK_SET);
    uint32_t size = ALIGN_UP_PAGE(length);
    t->mmap -= size;
    _new_mmap_t_and_enqueue(t->mmaps, t->mmap, t->mmap + size, prot, flags, fd_num, pgoffset);
    return t->mmap;
}

int scheduler_mmap2(void *addr, int length, int prot, int flags, int fd, int pgoffset){
    if(addr != NULL){
        screen_print("PANIC: We don't support addr on mmap2");
        for(;;){}
    }
    if(flags & MAP_SHARED){
        screen_print("PANIC: We don't support MAP_SHARED on mmap2");
        for(;;){}
    }
    if(fd == -1){
        return _do_scheduler_mmap2_no_fd(length, prot, flags);
    }
    else{
        return _do_scheduler_mmap2_fd(length, prot, flags, fd, pgoffset);
    }
}