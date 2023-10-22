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

int ptable_is_empty(process_table_t *self);

process_t *ptable_search_pendencia(process_table_t *self,
                                   process_state_t estado,
                                   int dispositivo);


int ptable_delete(process_table_t *self, unsigned int PID);

//Setters

//Salva o ponteiro para cpu_info_t
int proc_set_cpuinfo(process_t *self, cpu_info_t cpuInfo);

//Bloqueia o processo para esperar outro processo
int proc_set_waiting_PID(process_t *p, unsigned int PID);

/* PID do precesso que está sendo esperado (SO_ESPERA_PROC)
* ou identificador do dispositivo (terminal) que está esperando acessar
*/
int proc_set_PID_or_device(process_t *self, unsigned int PID_or_device);
int proc_get_PID_or_device(process_t *self);


// Getters
unsigned  int proc_get_waiting_PID(process_t *p);
cpu_info_t proc_get_cpuinfo(process_t* self);
process_state_t proc_get_state(process_t* self);
unsigned int proc_get_PID(process_t* self);
int proc_set_state(process_t *self, process_state_t processState);


#endif // SO23B_PROCESS_MNG_H
