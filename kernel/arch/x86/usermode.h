#ifndef USERMODE_H
#define USERMODE_H

#include <stdint.h>

/* arranca la primera tarea: carga su trap_frame_t prefabricado (armado por
   scheduler_add_task(), ver scheduler.c) y salta a ring3. Usado una unica
   vez por scheduler_start(). */
void context_start(uint32_t esp) __attribute__((noreturn));

#endif
