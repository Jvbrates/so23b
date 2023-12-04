//
// Created by jvbrates on 10/24/23.
//
#include "metricas.h"
#include "../process_mng.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
struct metricas{
  int procs;
  int ocioso;
  int tempo_ocioso;
  int irq_count[N_IRQ];
  int preemps;
  int exec_time;
  char filename[100];
};

metricas * log_init(char filename[]){
  metricas* m = calloc(1, sizeof(metricas));

  strncpy(m->filename, filename, strlen(filename));

  return m;
}

void log_save_tofile(metricas *m){
  FILE *file = fopen(m->filename, "a");
  if(file == NULL)
    return ;

  fprintf(file, "\n\nInformações Gerais do SO\n");

  //Numero de cada IRQ

  for (int i = 0; i < N_IRQ; ++i) {
    fprintf(file, "Número de IRQs %s: %i\n", irq_nome(i), m->irq_count[i]);
  }


  fprintf(file, "Vezes ocioso: %i\n", m->ocioso);
  fprintf(file, "Tempo total ocioso: %i\n", m->tempo_ocioso);
  fprintf(file, "Preempções: %i\n", m->preemps);
  fprintf(file, "Processos: %i\n", m->procs);
  fprintf(file, "Tempo de execução: %i\n", m->exec_time);


  if(file != NULL)
    fclose(file);
}

void log_irq(metricas *m, irq_t irq){
  m->irq_count[irq]++;
}

void log_proc(metricas *m){
  m->procs++;
}

void log_ocioso(metricas *m, int time){
  m->tempo_ocioso += time;
  m->ocioso++;
}

void log_preemp(metricas *m){
  m->preemps++;
}

void log_exectime(metricas *m, int time){
  m->exec_time = time;
}

void log_save_proc_tofile(void *proc, metricas *m){
  FILE *file = fopen(m->filename, "a");
  if(file == NULL)
    return ;

  process_t * c_proc = (process_t *)proc;
  int *state= proc_get_state_count(c_proc);
  int *t_state = proc_get_timestate_count(c_proc);
  int start = proc_get_start_time(c_proc);
  int end = proc_get_end_time(c_proc);
  int preemps = proc_get_preemp(c_proc);

  fprintf(file, "----------------------------------------------\n");
  fprintf(file, "Informações do Processo %i:\n", proc_get_PID(c_proc));


  //Número de cada state
  for (int i = 1; i < n_states; ++i) {
    char *teste = estado_nome(i);
    fprintf(file, "Número de vezes %s: %i (Tempo total %i)\n", teste, state[i], t_state[i]);
  }

  fprintf(file, "Tempo de Retorno: %i (%i - %i)\n", end - start, start, end);
  fprintf(file, "Preempções: %i\n", preemps);
  fprintf(file, "Tempo médio em estado pronto: %f (%i/%i)\n", ((double )t_state[waiting])/
                                                                      ((double)state[waiting]),
          t_state[waiting], state[waiting]);
  ;
  fprintf(file, "----------------------------------------------\n");
  if(file != NULL)
    fclose(file);
}