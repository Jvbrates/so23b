//
// Created by jvbrates on 10/23/23.
//

#include "scheduler_interface.h"
#include "util/linked_list.h"
#include "stdlib.h"

struct scheduler_t{
  node_t * first;
  relogio_t *relogio;
};

typedef struct scheduler_t_node{
  process_t *proc;
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
                                   process_t *process){

  sched_packet *schedPacket = calloc(1, sizeof(sched_packet));

  schedPacket->PID = PID;
  schedPacket->quantum = QUANTUM;
  schedPacket->curr_quantum = QUANTUM;
  schedPacket->proc = process;

  return schedPacket;
}



static void sched_update_prio(sched_packet *self){

}

//-------------INTERFACE------------------------------------------------------->

scheduler_t *sched_create(relogio_t *rel){
  scheduler_t  * sched = calloc(1, sizeof(scheduler_t));
  sched->relogio = rel;
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

  sched_packet *schedPacket = create_packet(PID, QUANTUM, process);

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

  // Consome quantum
  schedPacket->curr_quantum--;
  if(schedPacket->curr_quantum <= 0){
    schedPacket->curr_quantum = schedPacket->quantum;
    // Como é lista circular não haverá NULL, espera-se isto;
    self->first = llist_get_next(self->first);
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

    free(schedPacket);

    llist_delete_node(node);

    return 0;
  }

  return -1;
}

