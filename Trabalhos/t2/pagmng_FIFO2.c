//
// Created by jvbrates on 11/22/23.
//
#include "map_tpag.h"
#include "stdlib.h"
#include "util/linked_list.h"
#include <string.h>

struct map_tapg_t {
  int total_frames;
  int used_frames;
  node_t *node;
};

/* Lista ligada, mais novos a esquerda, mais antigos a direita*/



static map_node *map_node_create(int PID, tabpag_t *tabpag, int pag, int frame){
  map_node *mp = malloc(sizeof(map_node));


  map_node temp = {PID, tabpag, pag, frame};
  memcpy(mp, &temp, sizeof(map_node));

  return mp;
}

static void * deleteMapNode(node_t *node, void *arg){

  free(llist_get_packet(node));

  return NULL;
}
/* "Static" porque o que é criado com malloc é dito alocação dinâmica*/
static map_node getStaticCpy(map_node *mp){
  map_node  retorno = {
          mp->PID,
          mp->tpag,
          mp->pag,
          mp->frame
  };



  return retorno;

}


//-----------------------------------------------------------------------------|
// NOTE: O que acontece se eu der llist_node_unlink em um nó único?
// NOTE: O que acontece se eu adicionar um nó llist_insert_next(node, node),
//  inserir si proprio?
typedef struct {
  int pid;
  int count;
  node_t *next_it;
} arg_cb_tpag_free;

void *callback_tpag_free(node_t *node, void *arg_){
  arg_cb_tpag_free *arg = (arg_cb_tpag_free *)arg_;

  if(llist_get_key(node) == arg->pid){

    llist_node_unlink(node);                  // Remove do circulo
    llist_add_node_previous(arg->next_it, node);  // insere como uma página nova
    llist_set_key(node, 0);                   // Define como página não alocada

    map_node *mp = llist_get_packet(node);
    mp->PID = 0;
    mp->tpag = 0;
    mp->pag = 0;
    arg->count++;

  }

  return NULL;
}

//-----------------------------------------------------------------------------|
typedef struct {int pid; int pag; int acessado;} linha;

static void * dump_cb(node_t *node, void *arg_){
  linha *arg = (linha *)(arg_);

  map_node  *mp = llist_get_packet(node);
  arg[mp->frame].pag = mp->pag;
  arg[mp->frame].pid = mp->PID;

  if(mp->PID != 0) {
    arg[mp->frame].acessado = tabpag_bit_acesso(mp->tpag, mp->pag);
  } else {
    arg[mp->frame].acessado = 0;
  }


  return NULL;
}

//-----------------------------------------------------------------------------|

void *non_read_cb(node_t *node, void *arg){
  map_node *mp = llist_get_packet(node);

  if(llist_get_key(node) == 0){
    return node;
  }


  bool acessado = tabpag_bit_acesso(mp->tpag, mp->pag);

  if(!acessado){
    return node;
  }

  return NULL;
}

//-----------------------------------------------------------------------------|

void *cb_zerabit(node_t *node, void *arg){
  map_node *mp = llist_get_packet(node);

  if(mp->PID != 0){
    tabpag_zera_bit_acesso(mp->tpag, mp->pag);
  }

  return NULL;

}
//-----------------------------------------------------------------------------|

// Daqui pra baixo não é static

map_tapg_t * map_tpag_create(int num_pages){

  map_tapg_t  * mapTapg = malloc(sizeof(map_tapg_t));
  mapTapg->total_frames = num_pages;
  mapTapg->used_frames = 0;

  mapTapg->node = llist_create_node_round(map_node_create(0, 0, 0, 0), 0);


  for (int i = 1; i < num_pages; ++i) {


    map_node *mp = map_node_create(0, 0, 0, i);
    node_t  * n = llist_create_node_round(mp, 0);

    llist_add_node_previous(mapTapg->node, n);

  }

  return mapTapg;
}
void map_tpag_destruct(map_tapg_t *self){
  llist_iterate_nodes(self->node, deleteMapNode, NULL);
  llist_destruct(&self->node);
  free(self);
}

map_node map_tpag_choose(map_tapg_t *self, map_node mapNode){




  node_t *most_old_nused = llist_iterate_nodes(self->node, non_read_cb, NULL);

  if(most_old_nused){

    //Reorganiza
    if(most_old_nused == self->node) {
      self->node = llist_get_next(self->node);
     }else{
       llist_node_unlink(most_old_nused);
       llist_add_node_previous(self->node, most_old_nused);
     }



    //Redefine Página
    map_node *most_old_nused_p = llist_get_packet(most_old_nused);
    map_node ret = getStaticCpy(most_old_nused_p); // Retorna o que foi retirado
    llist_set_key(most_old_nused, mapNode.PID);
    most_old_nused_p->pag = mapNode.pag;
    most_old_nused_p->PID = mapNode.PID;
    most_old_nused_p->tpag = mapNode.tpag; // <--- FIXME






    return ret;



  } else {

    // Executa o FIFO comum
    map_node *most_old = llist_get_packet(self->node);

    llist_set_key(self->node, mapNode.PID);
    map_node ret = getStaticCpy(most_old);

    most_old->pag = mapNode.pag;
    most_old->PID = mapNode.PID;
    most_old->tpag = mapNode.tpag;
    //most_old->frame = mapNode.frame;

    self->node = llist_get_next(self->node);

    return ret;

  }


}

int map_tpag_free(map_tapg_t *self, int PID){

  int count = 0;
  if(llist_get_key(self->node) == PID){
    llist_set_key(self->node, 0);
    map_node  *mapNode = llist_get_packet(self->node);
    mapNode->PID = 0;
    count++;
  }

  arg_cb_tpag_free arg = {PID, count, self->node};
  llist_iterate_nodes(self->node, callback_tpag_free, &arg);

  return arg.count;

}


void map_tpag_dump(map_tapg_t *self, console_t *console){
  linha tabela[self->total_frames];

  llist_iterate_nodes(self->node, dump_cb, tabela);

  console_printf(console, "PAGMNG: [FRAME][PROCESSO][PAGINA][ACESSADO]");
  for (int i = 0; i < self->total_frames; ++i) {
    console_printf(console, "PAGMNG: [%i][%i][%i][%i]", i,
                   tabela[i].pid,
                   tabela[i].pag,
                   tabela[i].acessado);
  }
}



void map_tpag_zera_acessado(map_tapg_t *self){
  llist_iterate_nodes(self->node, cb_zerabit, NULL);
}