//
// Created by jvbrates on 9/24/23.
//

#ifndef SO23B_SCHEDULLER_INTERFACE_H
#define SO23B_SCHEDULLER_INTERFACE_H

#include "process_mng.h"
#include "relogio.h"
typedef struct scheduller_t scheduller_t;

scheduller_t *sched_create(void *proc_zer0, relogio_t *rel);

//scheduller_destruct não deve destruir os pprocesso (packet), isto é funcao da
// ptable
void sched_destruct(scheduller_t *self);

int sched_add(scheduller_t *self,
              void *process,
              unsigned int PID,
              unsigned int QUANTUM);

void *sched_get_update(scheduller_t *self);

void  *sched_remove(scheduller_t *self, unsigned int PID);


#endif // SO23B_SCHEDULLER_INTERFACE_H
