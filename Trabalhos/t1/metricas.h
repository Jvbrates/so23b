//
// Created by jvbrates on 10/24/23.
//

#ifndef SO23B_METRICAS_H
#define SO23B_METRICAS_H
#include "irq.h"
#include "stdio.h"

typedef struct {
  int procs;
  int ocioso;
  int irq_count[N_IRQ];
  int preemps
}metricas;


void init_metricas(metricas *m);

void salva_metricas(metricas m, FILE *file);



#endif // SO23B_METRICAS_H
