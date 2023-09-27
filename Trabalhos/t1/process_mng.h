//
// Created by jvbrates on 9/22/23.
//

#ifndef SO23B_PROCESS_MNG_H
#define SO23B_PROCESS_MNG_H
#include "scheduller_interface.h"
typedef struct process_table_t process_table_t;

typedef struct process_t process_t;

typedef void * cpu_info_t;

typedef enum { undefined=0,
               blocked_dev, // Waiting until process
               blocked_proc, // Blocked until device
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

//Acorda todos os processos que estavam bloqueados esperando certo processo(PID)
// E já adiciona o processo no escalonador
int ptable_wakeup_PID(process_table_t *self, unsigned int PID,
                      scheduller_t *scheduller, unsigned int quantum);
int ptable_wakeup_dev(process_table_t *self, void *disp,
                      scheduller_t *scheduller, unsigned int quantum);

//Setters
int proc_set_cpuinfo(process_t *self, cpu_info_t cpuInfo);
int proc_set_waiting_PID(process_t *p, unsigned int PID);
int proc_set_waiting_disp(process_t *p, void *disp);

// Getters
void * proc_get_waiting_disp(process_t *p);
unsigned  int proc_get_waiting_PID(process_t *p);
cpu_info_t proc_get_cpuinfo(process_t* self);
process_state_t proc_get_state(process_t* self);
unsigned int proc_get_PID(process_t* self);
int proc_set_state(process_t *self, process_state_t processState);


#endif // SO23B_PROCESS_MNG_H
