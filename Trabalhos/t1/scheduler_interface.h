//
// Created by jvbrates on 9/24/23.
//

#ifndef SO23B_scheduler_INTERFACE_H
#define SO23B_scheduler_INTERFACE_H

#include "process_mng.h"
#include "relogio.h"
typedef struct scheduler_t scheduler_t;

scheduler_t *sched_create(void *proc_zer0, relogio_t *rel);

//scheduler_destruct não deve destruir os pprocesso (packet), isto é funcao da
// ptable
void sched_destruct(scheduler_t *self);

int sched_add(scheduler_t *self,
              void *process,
              unsigned int PID,
              unsigned int QUANTUM);

void *sched_get_update(scheduler_t *self);

int sched_remove(scheduler_t *self, unsigned int PID);


#endif // SO23B_scheduler_INTERFACE_H
