//
// Created by jvbrates on 9/22/23.
//

#include "process_mng.h"
#include "util/linked_list.h"
#include "stdlib.h"
#include "scheduler_interface.h"

struct process_table_t  {
  node_t *first;
};


int decode(process_state_t estado){
  int valor = (int)estado;

  if(valor == 0)
    return valor;

  int count = 1;

  while (valor != 1){
    count++;
    valor>>=1;
  }
  return count;
}

static char *nomes_estados[10] = {
    [0] = "Indefinido",
    [1] = "Bloqueado  para leitura",
    [2] = "Bloqueado para escrita",
    [3] = "Bloqueado esperando outro processo",
    [4] = "Executando",
    [5] = "Pronto",
    [6] = "Morto",
    [7] = "Suspenso paginando",
    [8] = "Suspenso paginando durante chamada create proc",
};

struct process_t {

  int PID;
  int start_address; //T2: Agora é virtual
  void *cpuInfo;
  process_state_t processState;
  int PID_or_device_or_time; // 'time' referente ao tempo de espera do disco
  double priority;// round_robin_prio
  int last_exec_time;

  //LOG infos
  process_state_t previous_state;
  int time_previous_state;

  //Calcular o tempo de retorno
  int start_time;
  int end_time;

  //Calcular o numero de entradas em cada processo e tempo total em cada um
  int state_count[n_states]; // Numero de entradas em cada estado
  int time_state_count[n_states]; // Tempo em cada estado


  // Tempo médio em running;
  /* Não é necessário, o tempo médio seria a soma dos peridos prontos, divido
   * pelo numeros de vezes que esteve neste estado, este valor já foi contado;*/
  // double average_rtime;

  //Numero de preempções | incrementado pelo escalonador
  int preemp;

  // Adições para o T2 - Paginação de Memória


  tabpag_t *tpag;

  // É preciso saber onde um processo está alocado na memória secundária
  // para poder carregar para a principal quando necessário
  int quadro_sec_mem;

  // Necessário para saber se ocorreu uma falha de página ou `segmentation fault`
  int size;
  int pagina_fim;

};


process_t *proc_create(cpu_info_t cpuInfo,
                       int PID,
                       ret_so_carrega_programa start_address,
                       double priority,
                       int start_time){
  process_t  *p = calloc(1, sizeof(process_t));
  if(!p)
    return NULL;

  p->cpuInfo = cpuInfo;
  p->PID = PID;
  p->start_address = start_address.end_virt_ini; // T2
  p->processState = waiting; //Tod0 processo criado inicia esperando

  p->priority = priority;

  //LOG
  p->start_time = start_time;
  p->previous_state = undefined;
  p->time_previous_state = start_time;
  p->preemp  = 0;
  p->end_time = 0;


  // T2
  p->quadro_sec_mem = start_address.quadro_ini;
  p->size = start_address.end_virt_fim - start_address.end_virt_ini;
  p->pagina_fim = start_address.pagina_fim; // TODO Acho que não é necessario guardar isto
  p->tpag = tabpag_cria();

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
    tabpag_destroi(p->tpag);
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

void * ptable_add_proc(process_table_t *self, cpu_info_t cpuInfo, int PID,
                    ret_so_carrega_programa start_address, double priority, double start_time){
    process_t * p  = proc_create(cpuInfo, PID, start_address, priority, start_time);

    if(!p)
      return NULL;

    p->processState = waiting;
    node_t *nd = llist_create_node(p, p->PID);
    if(!nd)
      return NULL;

    llist_add_node(&(self->first), nd);

    return p;
}

process_t *ptable_search(process_table_t *self, int PID){
    if(!self)
      return NULL;
    node_t  *node = llist_node_search(self->first, PID);
    return llist_get_packet(node);
}

int ptable_is_empty(process_table_t *self){
    if (self->first == NULL)
      return 1;
    return 0;
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

    if(((p->processState & arg->state) > 0) && (p->PID_or_device_or_time == arg->dispositivo)){
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

//-----------------------------------------------------------------------------|

// Para paginação
// Vai buscar os processo com base no estado e adicioná-lo a uma lista,
// o tamanho da lista é problema do so

typedef struct {
    process_state_t state;
    int compare;
    process_t **proc_list;
    int count;
} ptable_search_hthan_arg;

void *callback_hthan(node_t *node, void *argument){

    ptable_search_hthan_arg *arg = argument;

    process_t *p = llist_get_packet(node);

    if(((p->processState & arg->state) > 0) && (p->PID_or_device_or_time <= arg->compare)){
      arg->proc_list[arg->count++] = p;
    }

    return NULL;
}

void ptable_search_hthan(process_table_t *self,
                         process_state_t estado,
                         int compare, process_t **proc_list){

    ptable_search_hthan_arg arg = {estado, compare, proc_list, 0};

    llist_iterate_nodes(self->first,
                        callback_hthan,
                        &arg);

}

//-----------------------------------------------------------------------------|
typedef struct {
    scheduler_t *sched_t;
    int PID_wait_t;
    int quantum;
} arg_wait;

void *callback_wait_proc(node_t *node, void *argument){

    arg_wait *arg = argument;

    process_t *p = llist_get_packet(node);

    if(p->processState == blocked_proc && p->PID_or_device_or_time == arg->PID_wait_t){
      p->processState = waiting;
      sched_add(arg->sched_t, p, p->PID, arg->quantum);
    }

    return NULL;
}


void ptable_proc_wait(process_table_t *self,
                      int PID_wait,
                      void *sched,
                      int QUANTUM_){

    arg_wait arg = {(scheduler_t *)sched, PID_wait, QUANTUM_} ;

    arg.sched_t = sched;

    llist_iterate_nodes(self->first,
                        callback_wait_proc,
                        &arg);
}

//*----------------------------------------------------------------------------

int ptable_delete(process_table_t *self, int PID){
    node_t  *node = llist_remove_node(&(self->first), PID);
    if(!node)//node not found
      return -1;
    process_t  *p = llist_get_packet(node);
    llist_delete_node(node);


    if(p) {
      if(p->cpuInfo)
        free(p->cpuInfo);
      free(p);
    }
    return 0;
}

cpu_info_t proc_get_cpuinfo(process_t* self){
    return self->cpuInfo;
}

process_state_t proc_get_state(process_t* self){
    return self->processState;
}

int proc_get_PID(process_t* self){
    return self->PID;
}

int proc_get_start_address(process_t* self){
    return self->start_address;
}

int proc_set_cpuinfo(process_t *self, cpu_info_t cpuInfo){
    self->cpuInfo = cpuInfo;
    return 0;
}

int proc_set_state(process_t *self, process_state_t processState, int time){
    /*if(self->time_previous_state != time) {

      self->time_state_count[decode(self->previous_state)] +=
              (time - self->time_previous_state);

      if (processState != self->processState) {
        self->state_count[decode(self->previous_state)]++;
        self->previous_state = self->processState;
      }

      self->time_previous_state = time;
    }*/
    //self->previous_state = self->processState;

    self->processState = processState;
    return 0;
}

int proc_set_PID_or_device(process_t *self, int PID_or_device_or_time){
    self->PID_or_device_or_time = PID_or_device_or_time;
    return 0;
}



int proc_get_PID_or_device(process_t *self){
    if(self)
      return self->PID_or_device_or_time;

    return -1;
}


double proc_get_priority(process_t *self){
    if(self)
      return self->priority;

    return -1;
}


void proc_set_priority(process_t *self, double priority){
    if(self)
      self->priority = priority;

}


void proc_set_end_time(process_t*self, int time){
    if(self)
      self->end_time = time;
}


void proc_incr_preemp(process_t *self){
    if(self)
      self->preemp++;
}

int proc_get_last_exec_time(process_t *self){
    if(self){
      return self->last_exec_time;
    }
    return -1;
}


void proc_set_last_exec_time(process_t *self, int time){
    if(self){
      self->last_exec_time = time;
    }
}





//-----------------------------------------------------------------------------

typedef struct {
    int tempo_estado;
    process_table_t *self;
    metricas *log;
}log_kill;


void *callback_log_states(node_t *node, void *argument) {
    if(!node)
      return NULL;

    log_kill *lk = (log_kill *)argument;

    process_t *p = llist_get_packet(node);

   if(p->processState != p->previous_state){
      p->state_count[decode(p->processState)]++;
   }



    p->time_state_count[decode(p->previous_state)]+= lk->tempo_estado;
    p->previous_state = p->processState;

    if(p->processState == dead) {
      log_save_proc_tofile(p, lk->log);
      ptable_delete(lk->self, p->PID);
    }

    return NULL;
}
void ptable_log_states(process_table_t *self, int tempo_estado, void *log){


    log_kill lk = {tempo_estado, self, log};

    llist_iterate_nodes(self->first, callback_log_states, &lk);
}

int *proc_get_timestate_count(process_t *self){
    if(self)
      return self->time_state_count;

    return NULL;
}


int *proc_get_state_count(process_t *self){
    if(self)
      return self->state_count;

    return NULL;
}

int proc_get_preemp(process_t *self){
    if(self)
      return self->preemp;

    return -1;
}

int proc_get_end_time(process_t *self){
    if(self)
      return self->end_time;
    return -1;
}

int proc_get_start_time(process_t *self){
    if(self)
      return self->start_time;
    return -1;
}

char *estado_nome(int estado){
    return nomes_estados[estado];
}


// T2

tabpag_t *proc_get_tpag(process_t *self){
    return self->tpag;
}
void proc_set_tpag(process_t *self, tabpag_t *tpag){
    self->tpag = tpag;
}
int proc_get_quadro_smem(process_t *self){
    return self->quadro_sec_mem;
}
int proc_get_size(process_t *self){
    return self->size;
}
int proc_get_pagina_fim(process_t *self){
    return self->pagina_fim;
}

int proc_get_page_addr(process_t *self, int address, int tam_pag){
    if(self->size + self->start_address < address || address < self->start_address) {
      return -1;
    }

    address = address - self->start_address;

    int pag = address / tam_pag;

    return (self->quadro_sec_mem * tam_pag)+(pag * tam_pag);

}

process_state_t proc_get_prev_state(process_t* self){
    return self->previous_state;
}