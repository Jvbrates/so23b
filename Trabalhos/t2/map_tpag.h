//
// Created by jvbrates on 11/21/23.
//
/* NÃO ALTERA A MEMÓRIA,
 * a unica alteração efetiva que pode fazer é zerar o bit "acessado"
 * das tabelas de paginas.
 * mantém um mapeamento das páginas dos processos nos quadros de
 * memória.
 * A memória principal tem um espaço/offset de paginas reservado
 * ao sistema operacional, mas isto não é tratado aqui, problema do SO.
 * TODO: pesquisar sobre perfil de execução
 * */

#ifndef SO23B_MAP_TPAG_H
#define SO23B_MAP_TPAG_H

#include "tabpag.h"
#include "console.h"

typedef struct map_tapg_t map_tapg_t;

/* Quando PID é zero, indica que o frame em que está posicionado
 * está não contém processos. Logo, NÃO ACESSE tpag quando PID == 0*/
typedef struct {
  int PID;
  tabpag_t *tpag;
  int pag;
  const int frame; // const para garantir que nao vou alterar isto daqui
} map_node;


// Crie e Destroi, padrão
map_tapg_t * map_tpag_create(int num_pages);
void map_tpag_destruct(map_tapg_t *self);

/* Substitui uma página(mapeamento de página), pela página(map_node) passada
 * como argumento,  * retorna a página retirada
 * */
map_node map_tpag_choose(map_tapg_t *self, map_node mapNode);


/* Demapeia todos os frames de um processo (PID),
 * retorna o número de frames liberados
 * Possivelmente nesta etapa o processo já esteja morto,
 * então não acesse *tpag.*/
int map_tpag_free(map_tapg_t *self, int PID);


/* Imprime no console o mapeamento*/
void map_tpag_dump(map_tapg_t *self, console_t *console);


/* Percorre os frames da memória[estrutura que representa os frames] e zera o
 * bit "acessado"*/
void map_tpag_zera_acessado(map_tapg_t *self);

#endif//SO23B_MAP_TPAG_H