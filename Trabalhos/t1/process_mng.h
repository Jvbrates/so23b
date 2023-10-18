//
// Created by jvbrates on 9/22/23.
//

#ifndef SO23B_PROCESS_MNG_H
#define SO23B_PROCESS_MNG_H
#include "scheduler_interface.h"

typedef struct process_table_t process_table_t;

typedef struct process_t process_t;

typedef void * cpu_info_t;

typedef enum { undefined=0,   // useful for dbg
               blocked_read,  // blocked for read
               blocked_write, // blocked for write
               blocked_proc,  // blocked for a proc die
               running,
               waiting,
               dead
} process_state_t;

//ptable <-> process_table_t
//proc <-> process_t

process_table_t *ptable_create();

//Destroi os processos internos a tabela
void ptable_destruct(process_table_t *processTable);

//Registra um processo na process_table, o valor do PID é responsabilidade
// do so.c


void *ptable_add_proc(process_table_t *self, cpu_info_t cpuInfo,
                    unsigned int PID, unsigned int start_address);

process_t *ptable_search(process_table_t *self, unsigned int PID);

int proc_delete(process_table_t *self, unsigned int PID);

/*Acorda(torna-os prontos) todos os processos que estavam bloqueados esperando
* certo processo(PID) e adiciona os processos ao escalonador
*/
 int ptable_wakeup_PID(process_table_t *self, unsigned int PID,
                      void *scheduler, unsigned int quantum);


int ptable_wakeup_dev(process_table_t *self, void *disp, unsigned int PID,
                      void *scheduler, unsigned int quantum);


int ptable_is_empty(process_table_t *self);

process_t *ptable_search_pendencia(process_table_t *self,
                                   process_state_t estado,
                                   int dispositivo);

//Setters

//Salva o ponteiro para cpu_info_t
int proc_set_cpuinfo(process_t *self, cpu_info_t cpuInfo);

//Bloqueia o processo para esperar outro processo
int proc_set_waiting_PID(process_t *p, unsigned int PID);

// Bloqueia o processo para esperar um dispositivo
int proc_set_waiting_disp(process_t *p, void *disp, unsigned int ID);

// Getters
void * proc_get_waiting_disp(process_t *p);
unsigned  int proc_get_waiting_PID(process_t *p);
cpu_info_t proc_get_cpuinfo(process_t* self);
process_state_t proc_get_state(process_t* self);
unsigned int proc_get_PID(process_t* self);
int proc_set_state(process_t *self, process_state_t processState);


#endif // SO23B_PROCESS_MNG_H
