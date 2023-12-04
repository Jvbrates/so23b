//
// Created by jvbrates on 10/24/23.
//

#ifndef SO23B_METRICAS_H
#define SO23B_METRICAS_H

#include "../irq.h"

typedef struct metricas metricas;


metricas  *log_init(char filename[]);

void log_save_tofile(metricas *m);

void log_save_proc_tofile(void *proc, metricas *self);

void log_irq(metricas *m, irq_t irq);

void log_ocioso(metricas *m, int time);

void log_proc(metricas *m);

void log_exectime(metricas *m, int time);

void log_preemp(metricas *m);

#endif // SO23B_METRICAS_H
