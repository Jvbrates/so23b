//
// Created by jvbrates on 10/23/23.
//
/* Implemente um segundo escalonador, semelhante ao circular:
 * os processos têm um quantum,
 * e sofrem preempção quando esse quantum é excedido.
 * Os processos têm também uma prioridade, e o escalonador escolhe o processo
 com maior prioridade entre os prontos.
 A prioridade de um processo é calculada da seguinte forma:
 * Quando um processo é criado, recebe prioridade 0,5:
    quando um processo perde o processador (porque bloqueou ou porque foi
    preemptado), a prioridade é calculada como prio = (prio + t_exec/t_quantum),
    onde t_exec é o tempo desde que ele foi escolhido para executar e t_quantum
    é o tempo do quantum.
* */


#include "scheduler_interface.h"
#include "util/linked_list.h"
#include "stdlib.h"

struct scheduler_t{
  node_t * first;
  relogio_t *relogio;
  metricas *log;

};

typedef struct scheduler_t_node{
  process_t *proc;
  int t_start;
  int PID;
  int quantum;
  int curr_quantum;
}sched_packet;





static void *sched_callback_destruct_packet(node_t *node, void *arg){

  if(!node)
    return NULL;

  sched_packet *packet = llist_get_packet(node);

  if(packet){
    free(packet);
  }

  return NULL;

}

static sched_packet *create_packet(int PID,
                                   int QUANTUM,
                                   process_t *process,
                                   int t_start){

  sched_packet *schedPacket = calloc(1, sizeof(sched_packet));

  schedPacket->PID = PID;
  schedPacket->quantum = QUANTUM;
  schedPacket->curr_quantum = 1;
  schedPacket->proc = process;
  schedPacket->t_start = t_start;
  return schedPacket;
}



static void sched_update_prio(sched_packet *self, int time_now){
    if(!self)
      return ;

    double prio =  proc_get_priority(self->proc) + ((time_now - self->t_start)/
                                                    self->curr_quantum);
    proc_set_priority(self->proc, prio);

}


typedef struct {
    int preemp;
    node_t *choose;
} arg_sched_choose_new;

void *callback_sched_choose_new(node_t *node, void *arg){
    arg_sched_choose_new *arg_casted = (arg_sched_choose_new *)arg;

  if(llist_get_key(node) == arg_casted->preemp)
      return NULL;


  sched_packet * schedPacket = llist_get_packet(node);
  sched_packet * sched_curr = llist_get_packet(arg_casted->choose);

  if(!(arg_casted->choose) || (proc_get_priority((sched_curr)->proc)
                                <= proc_get_priority(schedPacket->proc))){
      arg_casted->choose = node;
  }

  return NULL;

}

static void sched_choose_new(scheduler_t *self, int preemp){
    if(!self->first)
      return ;

    arg_sched_choose_new arg = {preemp, NULL};
    llist_iterate_nodes(self->first, callback_sched_choose_new, &arg);

    if(arg.choose)
      self->first = arg.choose;

}


//-------------INTERFACE------------------------------------------------------->

scheduler_t *sched_create(relogio_t *rel, metricas *log){
  scheduler_t  * sched = calloc(1, sizeof(scheduler_t));
  sched->relogio = rel;
  sched->log = log;
  return sched;
}

void sched_destruct(scheduler_t *self){

  //Destruindo os pacotes dos nós, mas não os processos.
  llist_iterate_nodes(self->first, sched_callback_destruct_packet, NULL);

  // Destruindo a lista de nós
  llist_destruct(&(self->first));


  //Destrói a estrutura em si.
  free(self);
}

int sched_add(scheduler_t *self,
              void *process,
              int PID,
              int QUANTUM){

  sched_packet *schedPacket = create_packet(PID, QUANTUM, process,
                                            rel_agora(self->relogio));

  node_t *node = llist_create_node_round(schedPacket, PID);

  //Caso self-first == NULL ele vai substituit self_first,
  //caso não ele vai adicionar a direita

  llist_add_node(&(self->first), node);

  return 0;
}

void *sched_update(scheduler_t *self){
  if(!self->first)
    return NULL;

  sched_packet *schedPacket = llist_get_packet(self->first);

  // Incrementa
  schedPacket->curr_quantum++;
  if(schedPacket->curr_quantum >= schedPacket->quantum){ //preempção
    log_preemp(self->log);
    schedPacket->curr_quantum = 1;

    //Atualizar prioridade
    sched_update_prio(llist_get_packet(self->first), rel_agora(self->relogio));

    //Decidir qual será o próximo processo
    sched_choose_new(self, llist_get_key(self->first));

  }

  return schedPacket->proc;
}


void *sched_get(scheduler_t *self){
  if(!self->first)
    return NULL;

  sched_packet *schedPacket = llist_get_packet(self->first);

  return schedPacket->proc;
}

int sched_remove(scheduler_t *self, int PID){

  node_t  *node = llist_remove_node(&(self->first), PID);

  if(node){
    sched_packet *schedPacket = llist_get_packet(node);

    sched_update_prio(schedPacket, rel_agora(self->relogio));

    free(schedPacket);

    llist_delete_node(node);

    sched_choose_new(self, -1); //Não é preempção, por isto -1 (ñ há PID -1)
    return 0;
  } else {
    exit(-1);
  }

  return -1;
}

