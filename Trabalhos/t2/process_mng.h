//
// Created by jvbrates on 9/22/23.
//

#ifndef SO23B_PROCESS_MNG_H
#define SO23B_PROCESS_MNG_H
#include "scheduler_interface.h"
#include "tabpag.h" // <--


typedef struct process_table_t process_table_t;

typedef struct process_t process_t;

typedef void * cpu_info_t;

typedef enum { undefined=0,   // useful for dbg
               blocked_read,  // blocked for read
               blocked_write, // blocked for write
               blocked_proc,  // blocked for a proc die
               running,
               waiting,
               dead,
               suspended,       // Bloqueado por paginação
               suspended_create_proc,       // Bloqueado por paginação quando o sistema tentou ler a string X para criar outro processo
               n_states
} process_state_t;

typedef struct {
  int end_virt_ini;
  int end_virt_fim;
  int pagina_ini;
  int pagina_fim;
  int quadro_ini; // <-- Quadro na memoria secundaria
} ret_so_carrega_programa;


//ptable <-> process_table_t
//proc <-> process_t

process_table_t *ptable_create();

//Destroi os processos internos a tabela
void ptable_destruct(process_table_t *processTable);

//Registra um processo na process_table, o valor do PID é responsabilidade
// do so.c

// TODO alterar o nome 'start_address'
void *ptable_add_proc(process_table_t *self, cpu_info_t cpuInfo,
                    int PID, ret_so_carrega_programa start_address, double priority, double start_time);

process_t *ptable_search(process_table_t *self, int PID);

int proc_delete(process_table_t *self, int PID);

int ptable_is_empty(process_table_t *self);

process_t *ptable_search_pendencia(process_table_t *self,
                                   process_state_t estado,
                                   int dispositivo);


int ptable_delete(process_table_t *self, int PID);

void ptable_log_states(process_table_t *self, int tempo_estado, void* log);

//Setters

//Salva o ponteiro para cpu_info_t
int proc_set_cpuinfo(process_t *self, cpu_info_t cpuInfo);

//Bloqueia o processo para esperar outro processo
int proc_set_waiting_PID(process_t *p, int PID);

/* PID do precesso que está sendo esperado (SO_ESPERA_PROC)
* ou identificador do dispositivo (terminal) que está esperando acessar
*/
int proc_set_PID_or_device(process_t *self, int PID_or_device);
int proc_get_PID_or_device(process_t *self);

void ptable_proc_wait(process_table_t *self,
                      int PID_wait,
                      void *sched,
                      int QUANTUM);
// Getters
unsigned  int proc_get_waiting_PID(process_t *p);
cpu_info_t proc_get_cpuinfo(process_t* self);
process_state_t proc_get_state(process_t* self);
int proc_get_PID(process_t* self);
int proc_set_state(process_t *self, process_state_t processState);

/* Como a prioridade deve manter-se ao longo da existência de um processo,
 * deve ser mantido com a tabela de processos e nao com o escalonador
 */
double proc_get_priority(process_t *self);

void proc_set_priority(process_t *self, double priority);

void proc_incr_preemp(process_t *self);

void proc_set_end_time(process_t*self, int time);


int proc_get_start_time(process_t *self);
int proc_get_end_time(process_t *self);
int proc_get_preemp(process_t *self);
int *proc_get_state_count(process_t *self);
int *proc_get_timestate_count(process_t *self);

char *estado_nome(process_state_t estado);

//T2
tabpag_t *proc_get_tpag(process_t *self);
void proc_set_tpag(process_t *self, tabpag_t *tpag);
int proc_get_quadro_smem(process_t *self);
int proc_get_size(process_t *self);
int proc_get_pagina_fim(process_t *self);

/* Retorna o endereço da página correspondete na memória secundária, -1 se não existir (END_INV)*/
int proc_get_page_addr(process_t *self, int address, int tam_pag);

#endif // SO23B_PROCESS_MNG_H
