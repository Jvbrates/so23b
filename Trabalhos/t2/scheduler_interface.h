//
// Created by jvbrates on 9/24/23.
//

#ifndef SO23B_scheduler_INTERFACE_H
#define SO23B_scheduler_INTERFACE_H

#include "process_mng.h"
#include "relogio.h"
#include "util/metricas.h"

typedef struct scheduler_t scheduler_t;

scheduler_t *sched_create(relogio_t *rel, metricas *log);

//scheduler_destruct não deve destruir os pprocesso (packet), isto é funcao da
// ptable
void sched_destruct(scheduler_t *self);

int sched_add(scheduler_t *self,
              void *process,
              int PID,
              int QUANTUM);

void *sched_get(scheduler_t *self);

void *sched_update(scheduler_t *self);


int sched_remove(scheduler_t *self, int PID);


#endif // SO23B_scheduler_INTERFACE_H
