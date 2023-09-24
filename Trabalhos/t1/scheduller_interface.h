//
// Created by jvbrates on 9/24/23.
//

#ifndef SO23B_SCHEDULLER_INTERFACE_H
#define SO23B_SCHEDULLER_INTERFACE_H

#include "process_mng.h"
#include "relogio.h"
typedef struct scheduller_t scheduller_t;

scheduller_t *sched_create(process_t *proc_zer0, relogio_t *rel);

void sched_destruct(scheduller_t *self);

int sched_add(scheduller_t *self,
              process_t *process,
              unsigned int PID,
              unsigned int QUANTUM);

process_t *sched_get_update(scheduller_t *self);

process_t  *sched_remove(scheduller_t *self, unsigned int PID);






#endif // SO23B_SCHEDULLER_INTERFACE_H
