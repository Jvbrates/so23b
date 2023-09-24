//
// Created by jvbrates on 9/22/23.
//
//TODO continuar aqui
#ifndef SO23B_PROCESS_MNG_H
#define SO23B_PROCESS_MNG_H

typedef struct process_table_t process_table_t;

typedef struct process_t process_t;

typedef struct cpu_info_t cpu_info_t;

typedef enum { undefined=0, blocked, running, waiting, dead } process_state_t;

//ptable <-> process_table_t
//proc <-> process_t

process_table_t *ptable_create();
void ptable_destruct_process_table(process_table_t *processTable);

//Registra um processo na process_table, o valor do PID é responsabilidade
// do so.c
int ptable_add_proc(process_table_t *self, cpu_info_t cpuInfo, unsigned int PID);

#endif // SO23B_PROCESS_MNG_H
