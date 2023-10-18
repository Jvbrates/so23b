//
// Created by jvbrates on 9/22/23.
//

#include "process_mng.h"
#include "util/linked_list.h"
#include "stdlib.h"

struct process_table_t  {
  node_t *first;
};



struct process_t {
  unsigned int PID;
  unsigned int start_address;
  void *cpuInfo;
  process_state_t processState;
  unsigned int PID_or_device;

};


process_t *proc_create(cpu_info_t cpuInfo,
                       unsigned int PID,
                       unsigned int start_address){
  process_t  *p = calloc(1, sizeof(process_t));
  if(!p)
    return NULL;

  p->cpuInfo = cpuInfo;
  p->PID = PID;
  p->start_address = start_address;
  p->processState = waiting; //Tod0 processo criado inicia esperando

  return p;
}

//-----------------------------------------------------------------------------|
process_table_t *ptable_create(){
  process_table_t *p = calloc(1, sizeof(process_table_t));
  if(!p)
    return NULL;
  return p;
}

void *ptable_destruct_proc(node_t *node, void * arg){
  process_t *p = (process_t *)llist_get_packet(node);

  if(p) {
    free(p->cpuInfo);
    free(p);
  }
  return NULL;
}

void ptable_destruct(process_table_t *self){

  //Destrói processos
  llist_iterate_nodes(self->first, ptable_destruct_proc, NULL);

  //Destrói estrutura linked_list
  llist_destruct(&(self->first));

  if(self)
      free(self);

}

/*

typedef struct {
  //waiting_id w_id;
  scheduler_t *sched;
  unsigned int quantum;
} wakeup_struct;


void *callback_wakeup_proc_pid(node_t *node, void *argument){
  process_t *p = llist_get_packet(node);

  wakeup_struct *wakeupStruct = (wakeup_struct *)argument;

  if(p->processState == blocked_proc &&
      p->id.PID == (unsigned int)wakeupStruct->w_id.PID) {
      p->processState = waiting;
      sched_add(wakeupStruct->sched, p, p->PID, wakeupStruct->quantum);
  }
  return NULL;
}

void *callback_wakeup_proc_dev(node_t *node, void *argument){
  process_t *p = llist_get_packet(node);

  wakeup_struct *wakeupStruct = (wakeup_struct *)argument;

  if(p->processState == blocked_proc &&
      p->id.disp == wakeupStruct->w_id.disp &&
      p->id.PID == wakeupStruct->w_id.PID) {
      p->processState = waiting;
      sched_add(wakeupStruct->sched, p, p->PID, wakeupStruct->quantum);
  }
  return NULL;
}

int ptable_wakeup_PID(process_table_t *self, unsigned int PID,
                      void *scheduler, unsigned int quantum){

  wakeup_struct wakeupStruct;
  wakeupStruct.sched = (scheduler_t *)scheduler;
  wakeupStruct.quantum = quantum;
  wakeupStruct.w_id.PID = PID;

  llist_iterate_nodes(self->first,
                      callback_wakeup_proc_pid, (void * )&wakeupStruct);
  return 0;
}
*/

/*
int ptable_wakeup_dev(process_table_t *self, void *disp, unsigned int ID,
                      void *scheduler, unsigned int quantum){

  llist_iterate_nodes(self->first, callback_wakeup_proc_dev, disp);

  wakeup_struct wakeupStruct;
  wakeupStruct.sched = (scheduler_t *)scheduler;
  wakeupStruct.quantum = quantum;
  wakeupStruct.w_id.PID = ID;
  wakeupStruct.w_id.disp = disp;

  llist_iterate_nodes(self->first,
                      callback_wakeup_proc_pid, (void * )&wakeupStruct);
  return 0;
}
*/


void * ptable_add_proc(process_table_t *self, cpu_info_t cpuInfo, unsigned int PID,
                    unsigned int start_address){
    process_t * p  = proc_create(cpuInfo, PID, start_address);

    if(!p)
      return NULL;

    node_t *nd = llist_create_node(p, p->PID);
    if(!nd)
      return NULL;

    llist_add_node(&(self->first), nd);

    return p;
}

process_t *ptable_search(process_table_t *self, unsigned int PID){
    if(!self)
      return NULL;
    node_t  *node = llist_node_search(self->first, PID);
    return llist_get_packet(node);
}

int ptable_is_empty(process_table_t *self){
    return (self->first == NULL);
}


/*
 * Estado deve ser blocked_read ou blocked_write
 */

struct ptable_search_pendencia_arg{
    process_state_t state;
    int dispositivo;
};

void *callback_search_block(node_t *node, void *argument){

    struct ptable_search_pendencia_arg *arg = argument;

    process_t *p = llist_get_packet(node);

    if(p->processState == arg->state && p->PID_or_device == argument){
      return node;
    }

    return NULL;
}

process_t *ptable_search_pendencia(process_table_t *self,
                                   process_state_t estado,
                                   int dispositivo){

    struct ptable_search_pendencia_arg pspa = {estado, dispositivo};

    node_t *node = llist_iterate_nodes(self->first,
                                       callback_search_block,
                                       &pspa);

    return  llist_get_packet(node);

}

//*----------------------------------------------------------------------------

int proc_delete(process_table_t *self, unsigned int PID){
    node_t  *node = llist_remove_node(&(self->first), PID);
    if(!node)//node not found
      return -1;
    process_t  *p = llist_get_packet(node);
    llist_delete_node(node);

    if(p)
      free(p);

    return 0;
}

cpu_info_t proc_get_cpuinfo(process_t* self){
    return self->cpuInfo;
}

process_state_t proc_get_state(process_t* self){
    return self->processState;
}

unsigned int proc_get_PID(process_t* self){
    return self->PID;
    return 0;
}

unsigned int proc_get_start_address(process_t* self){
    return self->start_address;
    return 0;
}

int proc_set_cpuinfo(process_t *self, cpu_info_t cpuInfo){
    self->cpuInfo = cpuInfo;
    return 0;
}

int proc_set_state(process_t *self, process_state_t processState){
    self->processState = processState;
    return 0;
}

/*
unsigned  int proc_get_waiting_PID(process_t *p){
    return p->id.PID;
}

void * proc_get_waiting_disp(process_t *p){
    return p->id.disp;
}

int proc_set_waiting_PID(process_t *p, unsigned int PID){
    p->id.PID = PID;
    return 0;
}

int proc_set_waiting_disp(process_t *p, void *disp, unsigned int ID){
    p->id.disp = disp;
    p->id.PID = ID;

    return 0;
}

*/
