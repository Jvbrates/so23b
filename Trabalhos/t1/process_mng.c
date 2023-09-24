//
// Created by jvbrates on 9/22/23.
//

#include "process_mng.h"
#include "util/linked_list.h"
#include "cpu.h"
#include "stdlib.h"

struct process_table_t  {
  node_t *first;
};

struct cpu_info_t {
  int PC;
  int X;
  int A;
  // estado interno da CPU
  int complemento;
  cpu_modo_t modo;
};

struct process_t {
  unsigned int PID;
  cpu_info_t cpuInfo;
  process_state_t processState;
};


process_t *proc_create(cpu_info_t cpuInfo, unsigned int PID){
  process_t  *p = calloc(1, sizeof(process_t));
  if(!p)
    return NULL;

  p->cpuInfo = cpuInfo;
  p->PID = PID;
  p->processState = blocked; //Todo processo criado inicia bloqueado

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

  if(p)
    free(p);

  return NULL;
}

void ptable_destruct(process_table_t *self){

  //Destrói processos
  llist_iterate_nodes(self->first, ptable_destruct_proc, NULL);

  //Destrói estrutura linked_list
  llist_destruct(self->first);

  if(self)
      free(self);

}

int ptable_add_proc(process_table_t *self, cpu_info_t cpuInfo, unsigned int PID){
    process_t * p  = proc_create(cpuInfo, PID);

    if(!p)
      return -1;

    node_t *nd = llist_create_node(p, p->PID);
    if(!nd)
      return -1;

    llist_add_node(&(self->first), nd);

    return 0;
}

process_t *ptable_search(process_table_t *self, unsigned int PID){
    node_t  *node = llist_node_search(self->first, PID);
    return llist_get_packet(node);
}

int proc_delete(process_table_t *self, unsigned int PID){
    node_t  *node = llist_remove_node(self->first, PID);
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
}

int proc_set_cpuinfo(process_t *self, cpu_info_t cpuInfo){
    self->cpuInfo = cpuInfo;
}

int proc_set_state(process_t *self, process_state_t processState){
    self->processState = processState;
}